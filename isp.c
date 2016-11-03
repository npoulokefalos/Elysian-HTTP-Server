#include "elysian.h"






elysian_err_t elysian_isp_multipart(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, void* vargs, uint8_t end_of_stream) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_isp_multipart_t * args = (elysian_isp_multipart_t*) vargs;
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

elysian_err_t elysian_isp_raw(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, void* vargs, uint8_t end_of_stream) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_isp_raw_t * args = (elysian_isp_raw_t*) vargs;
	elysian_cbuf_t* cbuf_list_out_tmp;
	uint32_t split_size;
	uint32_t cbuf_len;
	elysian_err_t err;
	while(1) {
		cbuf_len = elysian_cbuf_list_len(*cbuf_list_in);
		switch(args->state){
			case 1: /* Receiving HTTP body */
			{
				split_size = client->httpreq.body_len - args->index;
				if (split_size > cbuf_len) {
					split_size = cbuf_len;
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


elysian_err_t elysian_isp_raw_multipart(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, void* vargs, uint8_t end_of_stream) {
	elysian_isp_raw_multipart_t * args = (elysian_isp_raw_multipart_t*) vargs;
	elysian_err_t err;
	
	err = elysian_isp_raw(server, cbuf_list_in, cbuf_list_out, &args->raw, 0);
	switch(err){
		case ELYSIAN_ERR_READ:
		{
			// Stream not exchausted
			err = elysian_isp_multipart(server, cbuf_list_in, cbuf_list_out, &args->multipart, 0);
		}break;
		case ELYSIAN_ERR_OK:
		{
			// End-of-stream detected
			err = elysian_isp_multipart(server, cbuf_list_in, cbuf_list_out, &args->multipart, 1);
		}break;
		default:
		{
		}break;
	}
	return err;
}