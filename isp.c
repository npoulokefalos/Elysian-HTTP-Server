#include "elysian.h"

elysian_err_t elysian_isp_raw(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_isp_raw_t * args = &client->isp.raw;
	elysian_cbuf_t* cbuf_list_out_tmp;
	uint32_t split_size;
	uint32_t cbuf_list_in_len;
	elysian_err_t err;
	while(1) {
		cbuf_list_in_len = elysian_cbuf_list_len(*cbuf_list_in);
		switch(args->state){
			case 1: /* Receiving HTTP body */
			{
				split_size = client->httpreq.body_len - args->index;
				if (split_size > cbuf_list_in_len) {
					split_size = cbuf_list_in_len;
				}
				err = elysian_cbuf_list_split(server, cbuf_list_in, split_size, &cbuf_list_out_tmp);
				if (err != ELYSIAN_ERR_OK) {
					return err;
				} else {
					elysian_cbuf_list_append(cbuf_list_out, cbuf_list_out_tmp);
					args->index += split_size;
					if (args->index == client->httpreq.body_len) {
						args->state = 2;
					} else {
						return ELYSIAN_ERR_READ;
					}
				}
			} break;
			case 2: /* HTTP body received  */
			{
				return ELYSIAN_ERR_OK;
			} break;	
		}
	}
}

#if 0
elysian_err_t elysian_isp_chunked(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, void* isp_args) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_isp_chunked_t* args = &client->isp.chunked;
	elysian_cbuf_t* cbuf_list_out_tmp;
	elysian_err_t err;
	uint32_t cbuf_list_in_len;
	uint32_t max_rechain_sz;
	
	while (1) {
		if (*cbuf_list_in == NULL) {
			/*
			** No data to process
			*/
			return ELYSIAN_ERR_READ;
		} else {
			cbuf_list_in_len = elysian_cbuf_list_len(*cbuf_list_in);
		}
		switch (args->state) {
			case 1: /* [hex chunk size]\r\n */
			{
				if (cbuf_list_in_len < 3) {
					return ELYSIAN_ERR_READ;
				}
				index = elysian_cbuf_strstr(*cbuf_list_in, 0, "\r\n", 0);
				if (index = ELYSIAN_INDEX_OOB32) {
					if(elysian_cbuf_list_len(*cbuf_list_in) > 8 /* 0xffffffff */ + 2) {
						return ELYSIAN_ERR_FATAL;
					} else {
						return ELYSIAN_ERR_READ;
					}
				} else {
					if (index > 8) {
						/*
						** Invalid hex size
						*/
						return ELYSIAN_ERR_FATAL;
					} else {
						args->chunkSz = 0;//;...;
						args->chunkSzProcessed = 0;
						err = elysian_cbuf_split(server, cbuf_list_in, index + 2, cbuf_list_out);
						if(err != ELYSIAN_ERR_OK) {
							return err;
						} else {
							elysian_cbuf_list_free(server, *cbuf_list_in);
							*cbuf_list_in = *cbuf_list_out;
							if (args->chunkSz == 0) {
								args->state = 4;
							} else {
								args->state = 2;
							}
						}
					}
				}
			} break;
			case 2: /* [chunk data] */
			{
				if (cbuf_list_in_len < 1) {
					return ELYSIAN_ERR_READ;
				}
				max_rechain_sz = cbuf_list_in_len;
				if (max_rechain_sz > args->chunkSz - args->chunkSzProcessed) {
					max_rechain_sz = args->chunkSz - args->chunkSzProcessed;
				}
				err = elysian_cbuf_rechain(server, cbuf_list_in, max_rechain_sz, &cbuf_list_out_tmp);
				if(err != ELYSIAN_ERR_OK) {
					return err;
				} else {
					elysian_cbuf_list_append(cbuf_list_out, *cbuf_list_in); // extend valid data
					*cbuf_list_in = cbuf_list_out_tmp; // still unprocessed
					args->chunkSzProcessed -= max_rechain_sz;
					if(args->chunkSzProcessed == args->chunkSz) {
						args->state = 3;
						break;
					} else {
						return ELYSIAN_ERR_READ; // still not finished
					}
				}
			} break;
			case 3: /* "\r\n" after [chunk data] */
			{
				if (cbuf_list_in_len < 2) {
					return ELYSIAN_ERR_READ;
				}
				index = elysian_cbuf_strstr(*cbuf_list_in, 0, "\r\n", 0);
				if (index != 0) {
						return ELYSIAN_ERR_FATAL;
				} else {
					err = elysian_cbuf_rechain(server, cbuf_list_in, 2, &cbuf_list_out_tmp);
					if(err != ELYSIAN_ERR_OK) {
						return err;
					} else {
						elysian_cbuf_list_free(server, *cbuf_list_in);
						*cbuf_list_in = cbuf_list_out_tmp; // still unprocessed
						args->state = 1;
					}
				}
			} break;
			case 4: /* [\r\n] or [header0][\r\n]...[headerN][\r\n][\r\n]  */
			{
				if (cbuf_list_in_len < 2) {
					return ELYSIAN_ERR_READ;
				}
				index = elysian_cbuf_strstr(*cbuf_list_in, 0, "\r\n", 0);
				if (index = ELYSIAN_INDEX_OOB32) {
					if(elysian_cbuf_list_len(*cbuf_list_in) > 200) {
						/*
						** "\r\n" was not found in the first 200 bytes of the trailing
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
						err = elysian_cbuf_rechain(server, cbuf_list_in, 2, &cbuf_list_out_tmp);
						if(err != ELYSIAN_ERR_OK) {
							return err;
						} else {
							elysian_cbuf_list_free(server, *cbuf_list_in);
							*cbuf_list_in = cbuf_list_out_tmp; // still unprocessed
							args->state = 5;
							return ELYSIAN_ERR_OK;
						}
					} else {
						/*
						** Get next header
						*/
						err = elysian_cbuf_rechain(server, cbuf_list_in, index + 2, cbuf_list_out_tmp);
						if(err != ELYSIAN_ERR_OK) {
							return err;
						} else {
							elysian_cbuf_list_free(server, *cbuf_list_in); // just ommit the header
							*cbuf_list_in = *cbuf_list_out_tmp; // still unprocessed
							break;
						}
					}
				}
			} break;
			case 5: /* We have finished  */
			{
				return ELYSIAN_ERR_OK;
			} break;
		} // switch (args->state) {
	} // while(1)
		
	/*
	** Never reach here
	*/
	return ELYSIAN_ERR_FATAL;
}
#endif

elysian_err_t elysian_isp_multipart(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream) {
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
				uint32_t header_index = args->index;
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




elysian_err_t elysian_isp_raw_multipart(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream) {
	//elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	
	err = elysian_isp_raw(server, cbuf_list_in, cbuf_list_out, 0);
	switch(err){
		case ELYSIAN_ERR_READ:
		{
			// Stream not exchausted
			err = elysian_isp_multipart(server, cbuf_list_in, cbuf_list_out, 0);
		}break;
		case ELYSIAN_ERR_OK:
		{
			// End-of-stream detected
			err = elysian_isp_multipart(server, cbuf_list_in, cbuf_list_out, 1);
		}break;
		default:
		{
		}break;
	}
	return err;
}