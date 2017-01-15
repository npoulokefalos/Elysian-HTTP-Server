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


/*
  http://www.w3.org/TR/html401/interact/forms.html#h-17.13.4.2
 
		 FORMAT:
		  '--[BOUNDARY]\r\n
		  [HEADER]\r\n
		  [HEADER]\r\n\r\n
			
		  [DATA]\r\n
		  --[BOUNDARY]\r\n
		  [HEADER]\r\n
		  [HEADER]\r\n\r\n

		  [DATA]\r\n
		  --[BOUNDARY]--\r\n  (\r\n after -- is optional)
		  '
		  
		 SAMPLE:
		  '-----------------------------548189129986
		  Content-Disposition: form-data; name="text1"


		  -----------------------------548189129986
		  Content-Disposition: form-data; name="file1"; filename=""
		  Content-Type: application/octet-stream


		  -----------------------------548189129986--'
		   
		 EMPTY SAMPLE:
		  '-----------------------------25857811622488--'
*/
elysian_err_t elysian_isp_http_body_multipart(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_isp_multipart_t * args = &client->isp.multipart; 
	uint32_t strlen_boundary;
	uint32_t cbuf_len;
	//elysian_cbuf_t* cbuf;
	uint32_t split_size;
	elysian_req_param_t* param;
	elysian_req_param_t* param_next;
	uint32_t index;
	uint32_t index0;
	uint32_t index1;
	elysian_err_t err;
	elysian_cbuf_t* cbuf_list_out_tmp;
	
	//ELYSIAN_LOG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	//cbuf_list_print(*cbuf_list_in);
	//ELYSIAN_LOG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	
	strlen_boundary = strlen(client->httpreq.multipart_boundary);
	while(1) {
		cbuf_len = elysian_cbuf_list_len(*cbuf_list_in);
		
		if ((args->state == 0) && (cbuf_len == 0) && (end_of_stream)) {
			/*
			** Zero bodied multipart
			*/
			args->state = 4;
		}
		switch(args->state){
			ELYSIAN_LOG("state %u", args->state);
			//cbuf_list_print(*cbuf_list_in);
			case 0: /* Part body */
			{
				if (cbuf_len <  strlen_boundary) {
					err = ELYSIAN_ERR_READ;
					goto handle_error;
				}
				index = elysian_cbuf_strstr(*cbuf_list_in, 0, client->httpreq.multipart_boundary, 1);
				if (index == ELYSIAN_INDEX_OOB32) {
					split_size = cbuf_len - strlen_boundary;
					if (split_size) {
						err = elysian_cbuf_list_split(server, cbuf_list_in, split_size, &cbuf_list_out_tmp);
						if (err != ELYSIAN_ERR_OK) {
							goto handle_error;
						} else {
							elysian_cbuf_list_append(cbuf_list_out, cbuf_list_out_tmp);
							args->index += split_size;
							err = ELYSIAN_ERR_READ;
							goto handle_error;
						}
					} else {
						err = ELYSIAN_ERR_READ;
						goto handle_error;
					}
				} else {
					split_size = index + strlen_boundary;
					err = elysian_cbuf_list_split(server, cbuf_list_in, split_size, &cbuf_list_out_tmp);
					if (err != ELYSIAN_ERR_OK) {
						goto handle_error;
					} else {
						if (args->params) {
							args->params->data_size = (args->index + index - 4 /* \r\n--*/) - args->params->data_index;
						}
						elysian_cbuf_list_append(cbuf_list_out, cbuf_list_out_tmp);
						args->index += split_size;
						args->state = 1;
					}
				}
			} break;
			case 1: /* Do we have we more parts? */
			{
				if (cbuf_len < 2) {
					err = ELYSIAN_ERR_READ;
					goto handle_error;
				}
				index0 = elysian_cbuf_strstr(*cbuf_list_in, 0, "\r\n", 1);
				index1 = elysian_cbuf_strstr(*cbuf_list_in, 0, "--", 1);
				if (index1 == 0) {
					/*
					** This is the las part
					*/
					split_size = 2;
					err = elysian_cbuf_list_split(server, cbuf_list_in, split_size, &cbuf_list_out_tmp);
					if (err != ELYSIAN_ERR_OK) {
						goto handle_error;
					} else {
						elysian_cbuf_list_append(cbuf_list_out, cbuf_list_out_tmp);
						args->index += split_size;
						args->state = 3; /* Done */
					}
				} else if (index0 == 0) { 
					/* 
					* New part follows 
					*/
					param = elysian_mem_malloc(server, sizeof(elysian_req_param_t));
					if (!param) {
						err = ELYSIAN_ERR_POLL;
						goto handle_error;
					} else {
						split_size = 2;
						err = elysian_cbuf_list_split(server, cbuf_list_in, split_size, &cbuf_list_out_tmp);
						if (err != ELYSIAN_ERR_OK) {
							elysian_mem_free(server, param);
							goto handle_error;
						} else {
							param->next = args->params;
							args->params = param;
							param->client = client;
							param->file = &client->httpreq.body_file;
							param->name = NULL;
							param->filename = NULL;
							elysian_cbuf_list_append(cbuf_list_out, cbuf_list_out_tmp);
							args->index += split_size;
							args->state = 2;
						}
					}
				} else {
					err = ELYSIAN_ERR_FATAL;
					goto handle_error;
				}
			} break;
			case 2: /* Part header */
			{
				if (cbuf_len < 4) {
					err = ELYSIAN_ERR_READ;
					goto handle_error;
				}
				index = elysian_cbuf_strstr(*cbuf_list_in, 0, "\r\n\r\n", 1);
				if (index == ELYSIAN_INDEX_OOB32) {
					err = ELYSIAN_ERR_READ;
					goto handle_error;
				} else {
					/*
					** Part header end located. index points to "\r\n\r\n" 
					** All searches should be verified to included in the [0, index] range.
					** Locate and retrieve any name="file1"; filename="" sub headers.
					** We should return here with a code != ELYSIAN_ERR_OK unless all allocated memory
					** (for name & filename values) is freed! FATAL errors should release the whole
					** param list too.
					*/
					args->params->data_index = args->index + index + 4;
					char* search_strs[2] = {"name=\"", "filename=\""};
					uint8_t search_strs_mandatory[2] = {1, 0};
					uint32_t search_strs_index[2] = {ELYSIAN_INDEX_OOB32, ELYSIAN_INDEX_OOB32};
					int i, k;
					uint32_t search_index;
					uint8_t found;
					
					for (i = 0; i < sizeof(search_strs) / sizeof(search_strs[0]); i++) {
						search_index = 0;
						while (1) {
							//ELYSIAN_LOG("searching for '%s' from index %u, max index = %u", search_strs[i], search_index, index);
							if (search_index >= index) {
								index0 = ELYSIAN_INDEX_OOB32;
							} else {
								index0 = elysian_cbuf_strstr(*cbuf_list_in, search_index, search_strs[i], 0);
								if (index0 >= index) {
									index0 = ELYSIAN_INDEX_OOB32;
								}
							}
							if (index0 == ELYSIAN_INDEX_OOB32) {
								break;
							} else {
								/*
								** Ignore false positives etc "name=" could be matched with "filename="
								** due to same suffix.
								*/
								found = 0;
								for (k = 0; k < i; k++) {
									if(search_strs_index[k] == index0) {
										found = 1;
										break;
									}
								}
								if (found) {
									/*
									** Found, but this was a false positive, etc we located the substring
									** <name="> of <filename=">
									*/
									search_index = index0 + 1;
								} else {
									/*
									** Found
									*/
									search_strs_index[i] = index0;
									break;
								}
							}
						}

						if (index0 == ELYSIAN_INDEX_OOB32) {
							if (search_strs_mandatory[i]) {
								/*
								** We received a part without name
								*/
								err = ELYSIAN_ERR_FATAL;
								goto handle_error;
							} else {
								continue;
							}
						}
						
						//ELYSIAN_LOG("Found at index %u", index0);
						
						/*
						** Locate ending index
						*/
						index1 = elysian_cbuf_strstr(*cbuf_list_in, index0 + strlen(search_strs[i]), "\"", 0);
						if (index1 == ELYSIAN_INDEX_OOB32) {
							//elysian_cbuf_list_append(cbuf_list_in, cbuf_list_out_tmp);
							err = ELYSIAN_ERR_FATAL;
							goto handle_error;
						} else {
							char* search_value = elysian_mem_malloc(server, index1 - (index0 + strlen(search_strs[i])) + 1);
							if (!search_value) {
								//elysian_cbuf_list_append(cbuf_list_in, cbuf_list_out_tmp);
								if(args->params->name){
									elysian_mem_free(server, args->params->name);
									args->params->name = NULL;
								}
								if(args->params->filename){
									elysian_mem_free(server, args->params->filename);
									args->params->filename = NULL;
								}
								//elysian_cbuf_list_append(cbuf_list_in, cbuf_list_out_tmp);
								err = ELYSIAN_ERR_POLL;
								goto handle_error;
							} else {
								if (index0 + strlen(search_strs[i]) == index1) {
									strcpy(search_value, "");
								} else {
									//ELYSIAN_LOG("Copying str index [%u, %u]", index0, index1);
									elysian_cbuf_strcpy(*cbuf_list_in, index0 + strlen(search_strs[i]) , index1 - 1, search_value);
								}
								switch(i) {									
									case 0:
									{
										/*
										** Name
										*/
										//ELYSIAN_LOG("Part name '%s'", search_value);
										args->params->name = search_value;
									} break;
									case 1:
									{
										/*
										** Filename
										*/
										//ELYSIAN_LOG("Part filename '%s'", search_value);
										args->params->filename = search_value;
									} break;

									default:
									{
										ELYSIAN_ASSERT(0);
										elysian_mem_free(server, search_value);
										err = ELYSIAN_ERR_FATAL;
										goto handle_error;
									}break;
								}
							}

						}
					}
					
					/*
					** All things of interest were processed fuccesfully, now split
					*/
					split_size = index + 4;
					err = elysian_cbuf_list_split(server, cbuf_list_in, split_size, &cbuf_list_out_tmp);
					if (err != ELYSIAN_ERR_OK) {
						if(args->params->name){
							elysian_mem_free(server, args->params->name);
							args->params->name = NULL;
						}
						if(args->params->filename){
							elysian_mem_free(server, args->params->filename);
							args->params->filename = NULL;
						}
						return err;
					} else {
						elysian_cbuf_list_append(cbuf_list_out, cbuf_list_out_tmp);
						args->index += split_size;
						args->state = 0;
					}
				}
			} break;	
			case 3: /* Optional data */
			{
				elysian_cbuf_list_append(cbuf_list_out, *cbuf_list_in);
				*cbuf_list_in = NULL;
				if (end_of_stream) {
					args->state = 4;
				} else {
					err = ELYSIAN_ERR_READ;
					goto handle_error;
				}
			} break;
			case 4: /* Finished  */
			{
				err = ELYSIAN_ERR_OK;
				goto handle_error;
			} break;
			case 5: /* Fatal error  */
			{
				err = ELYSIAN_ERR_FATAL;
				goto handle_error;
			} break;
		}
	}
	
	handle_error:
		if ((err == ELYSIAN_ERR_READ) && (end_of_stream)) {
			err = ELYSIAN_ERR_FATAL;
		}
		if (err == ELYSIAN_ERR_FATAL) {
			args->state = 5;
			while(args->params){
				param_next = args->params->next;
				if(args->params->name){
					elysian_mem_free(server, args->params->name);
					args->params->name = NULL;
				}
				if(args->params->filename){
					elysian_mem_free(server, args->params->filename);
					args->params->filename = NULL;
				}
				elysian_mem_free(server, args->params);
				args->params = param_next;
			};
		}
	return err;
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

elysian_err_t elysian_isp_http_body_raw_multipart(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_cbuf_t* cbuf_list_out_tmp = NULL;
	elysian_err_t err;
	
	//ELYSIAN_LOG("[1] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	//cbuf_list_print(*cbuf_list_in);
	//ELYSIAN_LOG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	
	err = elysian_isp_http_body_raw(server, cbuf_list_in, &cbuf_list_out_tmp, 0);
	
	elysian_cbuf_list_append(&client->isp.cbuf_list, cbuf_list_out_tmp);

	switch(err){
		case ELYSIAN_ERR_READ:
		{
			// Stream not exchausted
			err = elysian_isp_http_body_multipart(server, &client->isp.cbuf_list, cbuf_list_out, 0);
		}break;
		case ELYSIAN_ERR_OK:
		{
			// End-of-stream detected
			err = elysian_isp_http_body_multipart(server, &client->isp.cbuf_list, cbuf_list_out, 1);
		}break;
		default:
		{
		}break;
	}

	//ELYSIAN_ASSERT(index0 <= index1, "");

	if (err == ELYSIAN_ERR_FATAL) {
		elysian_isp_cleanup(server);
	}
	
	return err;
}

elysian_err_t elysian_isp_http_body_chunked_multipart(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_cbuf_t* cbuf_list_out_tmp = NULL;
	elysian_err_t err;
	
	err = elysian_isp_http_body_chunked(server, cbuf_list_in, &cbuf_list_out_tmp, 0);

	elysian_cbuf_list_append(&client->isp.cbuf_list, cbuf_list_out_tmp);

	switch(err){
		case ELYSIAN_ERR_READ:
		{
			// Stream not exchausted
			err = elysian_isp_http_body_multipart(server, &client->isp.cbuf_list, cbuf_list_out, 0);
		}break;
		case ELYSIAN_ERR_OK:
		{
			// End-of-stream detected
			err = elysian_isp_http_body_multipart(server, &client->isp.cbuf_list, cbuf_list_out, 1);
		}break;
		default:
		{
		}break;
	}

	if (err == ELYSIAN_ERR_FATAL) {
		elysian_isp_cleanup(server);
	}
	
	return err;
}

void elysian_isp_cleanup(elysian_t* server) {
	elysian_req_param_t* param_next;
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	
	if (client->isp.cbuf_list) {
		elysian_cbuf_list_free(server, client->isp.cbuf_list);
		client->isp.cbuf_list = NULL;
	}
	
	while(client->isp.multipart.params){
		param_next = client->isp.multipart.params->next;
		if(client->isp.multipart.params->name){
			elysian_mem_free(server, client->isp.multipart.params->name);
			client->isp.multipart.params->name = NULL;
		}
		if(client->isp.multipart.params->filename){
			elysian_mem_free(server, client->isp.multipart.params->filename);
			client->isp.multipart.params->filename = NULL;
		}
		elysian_mem_free(server, client->isp.multipart.params);
		client->isp.multipart.params = param_next;
	};
}
