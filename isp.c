#include "elysian.h"

/*
** Parses the stream until a "\r\n\r\n" is detected
*/
elysian_err_t elysian_isp_http_headers(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_isp_raw_t * args = &client->isp.raw;
	elysian_cbuf_t* cbuf_list_out_tmp;
	uint32_t cbuf_list_in_len;
	elysian_err_t err;
	uint32_t index;
	
	while(1) {
		cbuf_list_in_len = elysian_cbuf_list_len(*cbuf_list_in);
		switch(args->state){
			case 0: /* Receiving HTTP headers */
			{
				if(cbuf_list_in_len < 4) {
					return ELYSIAN_ERR_READ;
				} else {
					index = elysian_cbuf_strstr(*cbuf_list_in, 0, "\r\n\r\n", 0);
					if (index == ELYSIAN_INDEX_OOB32) {
						if (elysian_cbuf_list_len(client->rcv_cbuf_list) > 1024) {
							elysian_set_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_413);
							return ELYSIAN_ERR_FATAL;
						} else {
							return ELYSIAN_ERR_READ;
						}
					} else {
						cbuf_list_out_tmp = NULL;
						err = elysian_cbuf_list_split(server, cbuf_list_in, index + 4, &cbuf_list_out_tmp);
						if (err != ELYSIAN_ERR_OK) {
							return err;
						} else {
							elysian_cbuf_list_append(cbuf_list_out, cbuf_list_out_tmp);
							args->state = 1;
						}
					}
				}
			} break;
			case 1:
			{
				return ELYSIAN_ERR_OK;
			} break;
			default:
			{
				return ELYSIAN_ERR_FATAL;
			} break;
		}
	}
}

elysian_err_t elysian_isp_http_body_chunked(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_isp_chunked_t* args = &client->isp.chunked;
	elysian_cbuf_t* cbuf_list_out_tmp;
	elysian_err_t err;
	uint32_t cbuf_list_in_len;
	uint32_t max_rechain_sz;
	uint32_t index;
	char strhex[9];
	
	while (1) {
		cbuf_list_in_len = elysian_cbuf_list_len(*cbuf_list_in);
		ELYSIAN_LOG("cbuf_list_in_len is %u", cbuf_list_in_len);
		//if( cbuf_list_in_len == 0) {
		//	return ELYSIAN_ERR_READ;
		//}
		switch (args->state) {
			case 0: /* [hex chunk size]\r\n */
			{
				if (cbuf_list_in_len < 3) {
					return ELYSIAN_ERR_READ;
				}
				index = elysian_cbuf_strstr(*cbuf_list_in, 0, "\r\n", 0);
				if (index == ELYSIAN_INDEX_OOB32) {
					if(elysian_cbuf_list_len(*cbuf_list_in) > 8 /* 0xffffffff */ + 2) {
						return ELYSIAN_ERR_FATAL;
					} else {
						return ELYSIAN_ERR_READ;
					}
				} else {
					if ((index == 0) || (index > 8)) {
						/*
						** Invalid hex size
						*/
						return ELYSIAN_ERR_FATAL;
					} else {
						elysian_cbuf_strcpy(*cbuf_list_in, 0, index, strhex);
						ELYSIAN_LOG("hex str = '%s'", strhex);
						elysian_strhex2uint(strhex, &args->chunkSz);
						ELYSIAN_LOG("uint = %u", args->chunkSz);
						args->chunkSzProcessed = 0;
						ELYSIAN_LOG("Chunk size is %u, index = %u, splitting on %u", args->chunkSz, index, index + 2);
						err = elysian_cbuf_list_split(server, cbuf_list_in, index + 2, &cbuf_list_out_tmp);
						if(err != ELYSIAN_ERR_OK) {
							return err;
						} else {
							elysian_cbuf_list_free(server, cbuf_list_out_tmp);
							if (args->chunkSz == 0) {
								args->state = 4;
							} else {
								args->state = 1;
							}
						}
					}
				}
			} break;
			case 1: /* [chunk data] */
			{
				ELYSIAN_LOG("Chunk data %u/%u", args->chunkSzProcessed, args->chunkSz);
				if (cbuf_list_in_len == 0) {
					return ELYSIAN_ERR_READ;
				}
				max_rechain_sz = cbuf_list_in_len;
				if (max_rechain_sz > args->chunkSz - args->chunkSzProcessed) {
					max_rechain_sz = args->chunkSz - args->chunkSzProcessed;
				}
				ELYSIAN_LOG("Chunk size is %u, splitting on %u", args->chunkSz, max_rechain_sz);
				err = elysian_cbuf_list_split(server, cbuf_list_in, max_rechain_sz, &cbuf_list_out_tmp);
				if(err != ELYSIAN_ERR_OK) {
					return err;
				} else {
					ELYSIAN_LOG("Appending data..");
					cbuf_list_print(cbuf_list_out_tmp);
					elysian_cbuf_list_append(cbuf_list_out, cbuf_list_out_tmp); // extend valid data
					args->chunkSzProcessed += max_rechain_sz;
					if(args->chunkSzProcessed == args->chunkSz) {
						args->state = 2;
					} else {
						ELYSIAN_LOG("Need more input!");
						return ELYSIAN_ERR_READ; // still not finished
					}
				}
			} break;
			case 2: /* "\r\n" after [chunk data] */
			{
				ELYSIAN_LOG("Skipping rn after data..");
				if (cbuf_list_in_len < 2) {
					return ELYSIAN_ERR_READ;
				}
				index = elysian_cbuf_strstr(*cbuf_list_in, 0, "\r\n", 0);
				ELYSIAN_LOG("rn index %u", index);
				if (index != 0) {
						return ELYSIAN_ERR_FATAL;
				} else {
					cbuf_list_out_tmp = NULL;
					err = elysian_cbuf_list_split(server, cbuf_list_in, 2, &cbuf_list_out_tmp);
					if(err != ELYSIAN_ERR_OK) {
						return err;
					} else {
						elysian_cbuf_list_free(server, cbuf_list_out_tmp);
						args->state = 0;
					}
				}
			} break;
			case 3: /* [\r\n] or [header0][\r\n]...[headerN][\r\n][\r\n]  */
			{
				if (cbuf_list_in_len < 2) {
					return ELYSIAN_ERR_READ;
				}
				index = elysian_cbuf_strstr(*cbuf_list_in, 0, "\r\n", 0);
				if (index == ELYSIAN_INDEX_OOB32) {
					if (cbuf_list_in_len > 256) {
						/*
						** "\r\n" was not found in the first 256 bytes of the trailing
						** HTTP headers. Since it is highly unlikely for  a header to be
						** that long, abort.
						*/
						return ELYSIAN_ERR_FATAL;
					} else {
						return ELYSIAN_ERR_READ;
					}
				} else {
					if (index == 0) {
						/*
						** We are done
						*/
						err = elysian_cbuf_list_split(server, cbuf_list_in, 2, &cbuf_list_out_tmp);
						if(err != ELYSIAN_ERR_OK) {
							return err;
						} else {
							elysian_cbuf_list_free(server, cbuf_list_out_tmp);
							args->state = 4;
						}
					} else {
						/*
						** Get next header
						*/
						err = elysian_cbuf_list_split(server, cbuf_list_in, index + 2, &cbuf_list_out_tmp);
						if(err != ELYSIAN_ERR_OK) {
							return err;
						} else {
							elysian_cbuf_list_free(server, cbuf_list_out_tmp); // just ommit the header
						}
					}
				}
			} break;
			case 4: /* We have finished  */
			{
				ELYSIAN_LOG("Chunked encoding finished!!!!!!!");
				return ELYSIAN_ERR_OK;
			} break;
		} // switch (args->state) {
	} // while(1)
		
	/*
	** Never reach here
	*/
	return ELYSIAN_ERR_FATAL;
}


elysian_err_t elysian_isp_http_body_multipart(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_isp_multipart_t * args = &client->isp.multipart; 
	uint32_t strlen_boundary;
	uint32_t cbuf_len;
	//elysian_cbuf_t* cbuf;
	uint32_t rechain_size;
	elysian_req_param_t* param;
	uint32_t index;
	elysian_err_t err;
	elysian_cbuf_t* cbuf_list_out_tmp;
	
	strlen_boundary = strlen(client->httpreq.multipart_boundary);
	while(1) {
		cbuf_len = elysian_cbuf_list_len(*cbuf_list_in);
		switch(args->state){
			case 1: /* Part body */
			{
				if (cbuf_len < strlen_boundary) {
					return ELYSIAN_ERR_READ;
				}
				index = elysian_cbuf_strstr(*cbuf_list_in, 0, client->httpreq.multipart_boundary, 1);
				if (index == ELYSIAN_INDEX_OOB32) {
					return ELYSIAN_ERR_READ;
				} else {
					rechain_size = index + strlen_boundary;
					//err = elysian_cbuf_rechain(server, cbuf_list_in, rechain_size);
					err = elysian_cbuf_list_split(server, cbuf_list_in, rechain_size, &cbuf_list_out_tmp);
					if (err != ELYSIAN_ERR_OK) {
						return err;
					} else {
						if (args->params) {
							args->params->data_size = (index - 2) - args->params->data_index;
						}
						elysian_cbuf_list_append(cbuf_list_out, cbuf_list_out_tmp);
						args->index += rechain_size;
						args->state = 2;
					}
				}
			} break;
			case 2: /* Do we have we more parts? */
			{
				if (cbuf_len < 2) {
					return ELYSIAN_ERR_READ;
				}
				index = elysian_cbuf_strstr(*cbuf_list_in, 0, "\r\n", 1);
				if (index != 0) {
					return ELYSIAN_ERR_FATAL;
				}
				if (index == ELYSIAN_INDEX_OOB32) {
					index = elysian_cbuf_strstr(*cbuf_list_in, 0, "--", 1);
					if (index == ELYSIAN_INDEX_OOB32) {
						return ELYSIAN_ERR_FATAL;
					} else {
						rechain_size = 2;
						//err = elysian_cbuf_rechain(server, cbuf_list_in, rechain_size);
						err = elysian_cbuf_list_split(server, cbuf_list_in, rechain_size, &cbuf_list_out_tmp);
						if (err != ELYSIAN_ERR_OK) {
							return err;
						} else {
							elysian_cbuf_list_append(cbuf_list_out, cbuf_list_out_tmp);
							args->index += rechain_size;
							args->state = 4; /* Done */
						}
					}
				} else {
					rechain_size = 2;
					param = elysian_mem_malloc(server, sizeof(elysian_req_param_t), ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
					if (!param) {
						return ELYSIAN_ERR_POLL;
					} else {
						//err = elysian_cbuf_rechain(server, cbuf_list_in, rechain_size);
						err = elysian_cbuf_list_split(server, cbuf_list_in, rechain_size, &cbuf_list_out_tmp);
						if (err != ELYSIAN_ERR_OK) {
							elysian_mem_free(server, param);
							return err;
						} else {
							param->next = args->params;
							args->params = param;
							//args->params->header_index = args->index + 2;

							elysian_cbuf_list_append(cbuf_list_out, cbuf_list_out_tmp);
							args->index += rechain_size;
							args->state = 3;
						}
					}
				}
			} break;
			case 3: /* Part header */
			{
				if (cbuf_len < 4) {
					return ELYSIAN_ERR_READ;
				}
				//uint32_t header_index = args->index;
				index = elysian_cbuf_strstr(*cbuf_list_in, 0, "\r\n\r\n", 1);
				if (index == ELYSIAN_INDEX_OOB32) {
					return ELYSIAN_ERR_READ;
				} else {
					rechain_size = index + 4;
					//err = elysian_cbuf_rechain(server, cbuf_list_in, rechain_size);
					err = elysian_cbuf_list_split(server, cbuf_list_in, rechain_size, &cbuf_list_out_tmp);
					if (err != ELYSIAN_ERR_OK) {
						elysian_mem_free(server, param);
						return err;
					} else {
						args->params->data_index = args->index + 4;
						
						index = elysian_cbuf_strstr(cbuf_list_out_tmp, 0, "name=\"", 0);
						if (index == ELYSIAN_INDEX_OOB32) {
							elysian_cbuf_list_append(cbuf_list_in, cbuf_list_out_tmp);
							return ELYSIAN_ERR_FATAL;
						} else {
							index = elysian_cbuf_strstr(cbuf_list_out_tmp, 6, "\"", 0);
							if (index == ELYSIAN_INDEX_OOB32) {
								elysian_cbuf_list_append(cbuf_list_in, cbuf_list_out_tmp);
								return ELYSIAN_ERR_FATAL;
							} else {
								args->params->name = elysian_mem_malloc(server, index, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
								if(!param->name){
									elysian_cbuf_list_append(cbuf_list_in, cbuf_list_out_tmp);
									return ELYSIAN_ERR_POLL;
								} else {
									//elysian_cbuf_strget(*cbuf_list_in, 6, args->params->name, index);
									elysian_cbuf_strcpy(cbuf_list_out_tmp, 0 , 5, args->params->name);
									args->params->name[index] = '\0';
									ELYSIAN_LOG("This is part NAME: %s", args->params->name);
								}
							}
						}
						//cbuf = client->rcv_cbuf_list;
						//*cbuf_list_in = cbuf->next;
						//cbuf->next = NULL;
						elysian_cbuf_list_append(cbuf_list_out, cbuf_list_out_tmp);
						args->index += (index + rechain_size);
						args->state = 1;
					}
				}
			} break;	
			case 4: /* Optional data */
			{
				elysian_cbuf_list_append(cbuf_list_out, *cbuf_list_in);
				if (end_of_stream) {
					return ELYSIAN_ERR_OK; /* We have finished */
				} else {
					return ELYSIAN_ERR_READ;
				}
			} break;	
		}
	}
}


elysian_err_t elysian_isp_http_body_raw(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_isp_raw_t * args = &client->isp.raw;
	elysian_cbuf_t* cbuf_list_out_tmp;
	uint32_t split_size;
	uint32_t cbuf_list_in_len;
	elysian_err_t err;
	
	while(1) {
		cbuf_list_in_len = elysian_cbuf_list_len(*cbuf_list_in);
		ELYSIAN_LOG("[RAW BODY ISP, len = %u]", cbuf_list_in_len);
		switch(args->state){
			case 0: /* Receiving HTTP body */
			{
				split_size = client->httpreq.body_len - args->index;
				if (split_size > cbuf_list_in_len) {
					split_size = cbuf_list_in_len;
				}
				cbuf_list_out_tmp = NULL;
				err = elysian_cbuf_list_split(server, cbuf_list_in, split_size, &cbuf_list_out_tmp);
				if (err != ELYSIAN_ERR_OK) {
					return err;
				} else {
					elysian_cbuf_list_append(cbuf_list_out, cbuf_list_out_tmp);
					args->index += split_size;
					if (args->index == client->httpreq.body_len) {
						ELYSIAN_LOG("[ISP_FINISHED]");
						args->state = 1;
					} else {
						return ELYSIAN_ERR_READ;
					}
				}

			} break;
			case 1: /* HTTP body received  */
			{
				ELYSIAN_LOG("[ISP ELYSIAN_ERR_OK]");
				return ELYSIAN_ERR_OK;
			} break;	
		}
	}
}

elysian_err_t elysian_isp_raw_http_body_multipart(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream) {
	//elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	
	err = elysian_isp_http_body_raw(server, cbuf_list_in, cbuf_list_out, 0);
	switch(err){
		case ELYSIAN_ERR_READ:
		{
			// Stream not exchausted
			err = elysian_isp_http_body_multipart(server, cbuf_list_in, cbuf_list_out, 0);
		}break;
		case ELYSIAN_ERR_OK:
		{
			// End-of-stream detected
			err = elysian_isp_http_body_multipart(server, cbuf_list_in, cbuf_list_out, 1);
		}break;
		default:
		{
		}break;
	}
	return err;
}
