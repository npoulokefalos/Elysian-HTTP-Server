/*
 * This file is part of Elysian Web Server
 *
 * Copyright (C) 2016,  Nikos Poulokefalos, npoulokefalos at gmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include "elysian.h" 

/*
	- 	elysian_schdlr_state_poll(server, 0); should be used at the enad of the particular event to re-enable fast polling,
		and not after every ERR_OK result. [else it will cause ps leak in the following case {ERR_OK:poll(0)}+ -> {ERR_MEM}+
		
	- 	every elysian_schdlr_state_poll_backoff(server); should be follwed by return;
*/


const char* elysian_schdlr_ev_name[]= {
    [elysian_schdlr_EV_ENTRY] = "EV_ENTRY",
    [elysian_schdlr_EV_READ] = "EV_READ",
    [elysian_schdlr_EV_POLL] = "EV_POLL",
    //[elysian_schdlr_EV_CLOSED] = "EV_CLOSED",
    [elysian_schdlr_EV_TIMER1] = "EV_TIMER1",
    [elysian_schdlr_EV_ABORT] = "EV_ABORT",
};

elysian_err_t elysian_state_http_connection_accepted(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_http_request_headers_receive(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_http_request_headers_store(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_http_request_headers_parse(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_http_expect_reply(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_http_request_body_receive(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_http_request_authenticate(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_http_request_params_get(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_fatal_error_entry(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_http_response_entry(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_mvc_pre_configuration(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_mvc_post_configuration(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_prepare_http_response(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_build_http_response(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_http_response_send(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_http_cleanup(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_websocket(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_http_keepalive(elysian_t* server, elysian_schdlr_ev_t ev);
elysian_err_t elysian_state_http_disconnect(elysian_t* server, elysian_schdlr_ev_t ev);

elysian_err_t elysian_client_cleanup(elysian_t* server);

elysian_err_t elysian_state_http_connection_accepted(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	static uint32_t client_unique_id = 0;
    
	ELYSIAN_LOG("[event = %s] ", elysian_schdlr_ev_name[ev]);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            client->id = ++client_unique_id;
			
            ELYSIAN_LOG("HTTP client #%u connected!", client->id);
			
			client->rcv_cbuf_list = NULL;
			client->store_cbuf_list = NULL;

            client->httpreq.url = NULL;
            client->httpreq.multipart_boundary = NULL;
			client->httpreq.params = NULL;
			
            elysian_fs_finit(server, &client->httpreq.headers_file);
			strcpy(client->httpreq.headers_filename, "");
            
            elysian_fs_finit(server, &client->httpreq.body_file);
			strcpy(client->httpreq.body_filename, "");

			elysian_mvc_init(server);
			
            client->httpresp.buf = NULL;

			elysian_resource_init(server);
			
			elysian_websocket_init(server);
			
			return elysian_schdlr_state_next(server, elysian_state_http_request_headers_receive);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, NULL);
}

/*
** Return ELYSIAN_ERR_OK only when the whole client->store_cbuf_list has been stored to the
** file (client->store_cbuf_list_size becomes 0), and the file has been closed and reopened
** in READ mode.
*/
elysian_err_t elysian_store_cbuf_to_file(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_cbuf_t* cbuf;
    elysian_file_t* file;
    char* filename;
    char* filename_template;
    uint32_t actual_write_sz;
    elysian_err_t err;
	elysian_mvc_controller_def_t* controller_def;

    if (client->isp.func == elysian_isp_http_headers) {
        file = &client->httpreq.headers_file;
        filename = client->httpreq.headers_filename;
        filename_template = ELYSIAN_FS_RAM_VRT_ROOT"/h_%u";
    } else {
        file = &client->httpreq.body_file;
        filename = client->httpreq.body_filename;
		controller_def = elysian_mvc_controller_def_get(server, client->httpreq.url, client->httpreq.method);
		if((controller_def) && (controller_def->flags & ELYSIAN_MVC_CONTROLLER_FLAG_USE_EXT_FS)) {
			filename_template = ELYSIAN_FS_EXT_VRT_ROOT"/b_%u";
		} else {
			filename_template = ELYSIAN_FS_RAM_VRT_ROOT"/b_%u";
		}
    }

	while (1) {
		if (!elysian_fs_fisopened(server, file)) {
			if (strlen(filename) == 0) {
				/*
				** File not yet created
				*/
				elysian_sprintf(filename, filename_template, client->id);
				ELYSIAN_ASSERT(strlen(filename) < sizeof(client->httpreq.headers_filename));
				err = elysian_fs_fopen(server, filename, ELYSIAN_FILE_MODE_WRITE, file);
				if(err != ELYSIAN_ERR_OK){
					strcpy(filename, "");
					return err;
				}
			} else {
				/*
				** File created, completely written and closed.
				** We need to open it in read mode.
				*/
				ELYSIAN_LOG("Opening file in read mode..");
				err = elysian_fs_fopen(server, filename, ELYSIAN_FILE_MODE_READ, file);
				return err;
			}
		} else {
			/*
			** File is opened, continue writting
			*/
			//ELYSIAN_LOG("writting cbuf chain..");
			while(client->store_cbuf_list){
				//ELYSIAN_LOG("writting cbuf..");
				cbuf = client->store_cbuf_list;
				//ELYSIAN_LOG("Storring '%s'", &cbuf->data[client->store_cbuf_list_offset]);
				err = elysian_fs_fwrite(server, file, &cbuf->data[client->store_cbuf_list_offset],  cbuf->len - client->store_cbuf_list_offset, &actual_write_sz);
				if(err != ELYSIAN_ERR_OK){
					return err;
				}
				
				client->store_cbuf_list_offset += actual_write_sz;
				if(client->store_cbuf_list_offset == cbuf->len){  
					client->store_cbuf_list = cbuf->next;
					elysian_cbuf_free(server, cbuf);
					client->store_cbuf_list_offset = 0;
				}
				client->store_cbuf_list_size -= actual_write_sz;
			}
			ELYSIAN_LOG("store_cbuf_list_size is %u",client->store_cbuf_list_size);
			if (client->store_cbuf_list_size == 0) {
				/*
				** The whore range was stored, close the file
				*/
				ELYSIAN_ASSERT(client->store_cbuf_list == NULL);
				ELYSIAN_ASSERT(client->store_cbuf_list_offset == 0);
				ELYSIAN_LOG("File write completed, closing..");
				elysian_fs_fclose(server, file);
				//continue and reopen file
			} else {
				return ELYSIAN_ERR_READ;
			}
		}
	}
}

elysian_err_t elysian_state_http_request_store(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_cbuf_t* cbuf;
    elysian_err_t err;
    
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timer1_set(server, 4000);
			elysian_schdlr_state_priority_set(server, elysian_schdlr_TASK_PRIO_LOW);

			ELYSIAN_ASSERT(client->store_cbuf_list == NULL);
			
			/*
			** Suppose we expect infinite bytes, until the input stream 
			** processor identifies the exact number
			*/
			//client->store_cbuf_list = NULL;
			client->store_cbuf_list_offset = 0;
            client->store_cbuf_list_size = -1;
			
            /*
            ** Don't break and continue to the READ event, asking a POLL
			** to open the file and store the already received data.
            */
        };//break;
        case elysian_schdlr_EV_READ:
        {
			elysian_schdlr_state_timer1_reset(server);
			
			cbuf = elysian_schdlr_state_socket_read(server);
			elysian_cbuf_list_append(&client->rcv_cbuf_list, cbuf);
            cbuf_list_print(client->rcv_cbuf_list);
            
			elysian_schdlr_state_poll_enable(server);
        }break;
        case elysian_schdlr_EV_POLL:
        {
			err = client->isp.func(server, &client->rcv_cbuf_list, &client->store_cbuf_list, 0);
			switch(err){
                case ELYSIAN_ERR_OK:
					/*
					** We have received the whole headers/body, set the correct store size.
					** For HTTP headers this is going to happen only once.
					** For HTTP body it is going to be happenning multiple times until we receive the body.
					*/
					client->store_cbuf_list_size = elysian_cbuf_list_len(client->store_cbuf_list);
					ELYSIAN_LOG("ISP finished, remaining store size is %u", client->store_cbuf_list_size);
					break;
				case ELYSIAN_ERR_READ:

                    break;
                case ELYSIAN_ERR_POLL:
                    elysian_schdlr_state_poll_backoff(server);
					return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
                    break;
                case ELYSIAN_ERR_FATAL:
					return elysian_schdlr_state_next(server, elysian_state_fatal_error_entry);
                    break;
                default:
                    ELYSIAN_ASSERT(0);
                    break;
            };
			
			ELYSIAN_LOG("Trying to store..");
			err = elysian_store_cbuf_to_file(server);
            switch(err){
                case ELYSIAN_ERR_OK:
					if (client->isp.func == elysian_isp_http_headers) {
						/*
						** The whole HTTP request headers were received and stored
						*/
						return elysian_schdlr_state_next(server, elysian_state_http_request_headers_parse);
					} else {
						/*
						** The whole HTTP request body was received and stored
						*/
						return elysian_schdlr_state_next(server, elysian_state_http_request_authenticate);
					}
                    break;
                case ELYSIAN_ERR_POLL:
                    elysian_schdlr_state_poll_backoff(server);
					return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
                    break;
				case ELYSIAN_ERR_READ:
					/*
					** Disable POLL, wait READ
					*/
                    elysian_schdlr_state_poll_disable(server);
                    break;
                case ELYSIAN_ERR_FATAL:
					return elysian_schdlr_state_next(server, elysian_state_fatal_error_entry);
                    break;
                default:
					ELYSIAN_LOG("err was %u",err);
                    ELYSIAN_ASSERT(0);
                    break;
            };
        }break;
		case elysian_schdlr_EV_TIMER1:
        case elysian_schdlr_EV_ABORT:
        {
			return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}

elysian_err_t elysian_state_http_request_headers_receive(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timer1_set(server, 4000);
			elysian_schdlr_state_priority_set(server, elysian_schdlr_TASK_PRIO_LOW);
			
			ELYSIAN_ASSERT(client->store_cbuf_list == NULL);
			
			client->http_pipelining_enabled = 0;
			client->httpresp.current_status_code = ELYSIAN_HTTP_STATUS_CODE_NA;
			client->httpresp.fatal_status_code = ELYSIAN_HTTP_STATUS_CODE_NA;
			client->httpresp.attempts = 0;
			
            client->httpreq_onservice_handler = NULL;
			client->httpreq_onservice_handler_data = NULL;
			
			memset(&client->isp, 0, sizeof(client->isp));
			
			client->isp.func = elysian_isp_http_headers;
			
			ELYSIAN_LOG("Waiting new HTTP request from client #%u", client->id);
			
			return elysian_schdlr_state_next(server, elysian_state_http_request_store);
        };break;
        case elysian_schdlr_EV_READ:
        {

        }break;
		case elysian_schdlr_EV_TIMER1:
        case elysian_schdlr_EV_ABORT:
        {
			return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}

elysian_err_t elysian_state_http_request_body_receive(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timer1_set(server, 4000);
			elysian_schdlr_state_priority_set(server, elysian_schdlr_TASK_PRIO_LOW);
			
			ELYSIAN_ASSERT(client->store_cbuf_list == NULL);
			
			memset(&client->isp, 0, sizeof(client->isp));
			
			if (client->httpreq.transfer_encoding == ELYSIAN_HTTP_TRANSFER_ENCODING_IDENTITY) {
				if (client->httpreq.content_type == ELYSIAN_HTTP_CONTENT_TYPE_MULTIPART__FORM_DATA) {
					client->isp.func = elysian_isp_http_body_raw_multipart;
					//client->isp.func = elysian_isp_http_body_raw;
				} else {
					client->isp.func = elysian_isp_http_body_raw;
				}
			} else if (client->httpreq.transfer_encoding == ELYSIAN_HTTP_TRANSFER_ENCODING_CHUNKED) {
				if (client->httpreq.content_type == ELYSIAN_HTTP_CONTENT_TYPE_MULTIPART__FORM_DATA) {
					client->isp.func = elysian_isp_http_body_chunked_multipart;
					//client->isp.func = elysian_isp_http_body_chunked;
				} else {
					client->isp.func = elysian_isp_http_body_chunked;
				}
			} else {
				/*
				** Unknown transfer encoding
				*/
				client->isp.func = elysian_isp_http_body_raw;
			}
			
			return elysian_schdlr_state_next(server, elysian_state_http_request_store);

        };break;
        case elysian_schdlr_EV_READ:
        {

        }break;
		case elysian_schdlr_EV_TIMER1:
        case elysian_schdlr_EV_ABORT:
        {
			return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}



elysian_err_t elysian_state_http_request_headers_parse(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
    
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timer1_set(server, 4000);
            elysian_schdlr_state_poll_enable(server);
        }break;
        case elysian_schdlr_EV_READ:
        {
        }break;
        case elysian_schdlr_EV_POLL:
        {
            err = elysian_http_request_headers_parse(server);
            switch(err){
                case ELYSIAN_ERR_OK:
					if(client->httpreq.expect_status_code != ELYSIAN_HTTP_STATUS_CODE_NA){
						ELYSIAN_LOG("Expect status code is %u\r\n", client->httpreq.expect_status_code);
						return elysian_schdlr_state_next(server, elysian_state_http_expect_reply);
					} else {
						return elysian_schdlr_state_next(server, elysian_state_http_request_body_receive);
					}
                    break;
                case ELYSIAN_ERR_POLL:
                    elysian_schdlr_state_poll_backoff(server);
					return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
                    break;
                case ELYSIAN_ERR_FATAL:
					return elysian_schdlr_state_next(server, elysian_state_fatal_error_entry);
                    break;
                default:
                    ELYSIAN_ASSERT(0);
                    break;
            };
        }break;
		case elysian_schdlr_EV_TIMER1:
        case elysian_schdlr_EV_ABORT:
        {
			return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}

elysian_err_t elysian_state_http_expect_reply(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	uint32_t send_size_actual;
	char http_expect_response[64];
    elysian_err_t err;
    
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timer1_set(server, 4000);
            elysian_schdlr_state_poll_enable(server);
			
			client->httpresp.buf_index = 0;
        }break;
        case elysian_schdlr_EV_READ:
        {
        }break;
        case elysian_schdlr_EV_POLL:
        {
			elysian_sprintf(http_expect_response, "HTTP/1.1 %u %s\r\n\r\n", elysian_http_get_status_code_num(client->httpreq.expect_status_code), 
																	elysian_http_get_status_code_msg(client->httpreq.expect_status_code));

			client->httpresp.buf = 	(uint8_t*) http_expect_response;
			client->httpresp.buf_len = strlen(http_expect_response);		
			err = elysian_socket_write(&client->socket, &client->httpresp.buf[client->httpresp.buf_index], client->httpresp.buf_len, &send_size_actual);
			client->httpresp.buf = NULL;
			switch(err){
				case ELYSIAN_ERR_OK:
					client->httpresp.buf_index += send_size_actual;
					if(client->httpresp.buf_index == client->httpresp.buf_len){
						if (client->httpreq.expect_status_code == ELYSIAN_HTTP_STATUS_CODE_100) {
							return elysian_schdlr_state_next(server, elysian_state_http_request_body_receive);
						} else {
							return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
						}
					}
					break;
				case ELYSIAN_ERR_POLL:
					elysian_schdlr_state_poll_backoff(server);
					return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
					break;
				default:
					return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
					break;
			};
        }break;
		case elysian_schdlr_EV_TIMER1:
        case elysian_schdlr_EV_ABORT:
        {
			return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}

elysian_err_t elysian_state_http_request_authenticate(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
    
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timer1_set(server, 4000);
			elysian_schdlr_state_poll_enable(server);
			
			/*
			** Since we reached here,  we have succesfully parsed the HTTP request's
			** headers & body so we are able to pipeline new ones while serving it.
			*/
			client->http_pipelining_enabled = 1; 
        }break;
        case elysian_schdlr_EV_READ:
        {
        }break;
        case elysian_schdlr_EV_POLL:
        {
			err = elysian_http_authenticate(server);
			switch(err){
				case ELYSIAN_ERR_OK:
					ELYSIAN_LOG("Authentication success!");
					return elysian_schdlr_state_next(server, elysian_state_http_request_params_get);
					break;
				case ELYSIAN_ERR_AUTH:
					ELYSIAN_LOG("Authentication failure!");
					elysian_set_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_401);
					return elysian_schdlr_state_next(server, elysian_state_http_response_entry);
					break;
				case ELYSIAN_ERR_POLL:
					elysian_schdlr_state_poll_backoff(server);
					return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
					break;
				case ELYSIAN_ERR_FATAL:
					return elysian_schdlr_state_next(server, elysian_state_fatal_error_entry);
					break;
				default:
					ELYSIAN_ASSERT(0);
					break;
			};
        }break;
		case elysian_schdlr_EV_TIMER1:
        case elysian_schdlr_EV_ABORT:
        {
			return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}

elysian_err_t elysian_state_http_request_params_get(elysian_t* server, elysian_schdlr_ev_t ev) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
    
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timer1_set(server, 4000);
			elysian_schdlr_state_poll_enable(server);
        }break;
        case elysian_schdlr_EV_READ:
        {
        }break;
        case elysian_schdlr_EV_POLL:
        {
			err = elysian_http_request_get_params(server);
			switch(err){
				case ELYSIAN_ERR_OK:
					return elysian_schdlr_state_next(server, elysian_state_http_response_entry);
					break;
				case ELYSIAN_ERR_POLL:
					elysian_schdlr_state_poll_backoff(server);
					return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
					break;
				case ELYSIAN_ERR_FATAL:
					return elysian_schdlr_state_next(server, elysian_state_fatal_error_entry);
					break;
				default:
					ELYSIAN_ASSERT(0);
					break;
			};
        }break;
		case elysian_schdlr_EV_TIMER1:
        case elysian_schdlr_EV_ABORT:
        {
			return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}

void elysian_set_http_status_code(elysian_t* server, elysian_http_status_code_e status_code) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	ELYSIAN_LOG("HTTP status changed to %u", elysian_http_get_status_code_num(status_code));
	client->httpresp.current_status_code = status_code;
}

void elysian_set_fatal_http_status_code(elysian_t* server, elysian_http_status_code_e status_code) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	client->httpresp.fatal_status_code = status_code;
}

elysian_err_t elysian_state_fatal_error_entry(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
			if(client->httpresp.fatal_status_code == ELYSIAN_HTTP_STATUS_CODE_NA) {
				/*
				** Set a generic error status code if nothing has been set
				*/
				client->httpresp.fatal_status_code = ELYSIAN_HTTP_STATUS_CODE_500;
			}
			
			elysian_set_http_status_code(server, client->httpresp.fatal_status_code);
			client->httpresp.fatal_status_code = ELYSIAN_HTTP_STATUS_CODE_NA;
			return elysian_schdlr_state_next(server, elysian_state_http_response_entry);
        }break;
        case elysian_schdlr_EV_READ:
        {
			
        }break;
        case elysian_schdlr_EV_POLL:
        {
			
        }break;
		case elysian_schdlr_EV_TIMER1:
        case elysian_schdlr_EV_ABORT:
        {
			return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}

elysian_err_t elysian_state_http_response_entry(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
			ELYSIAN_ASSERT(!elysian_resource_isopened(server));

			ELYSIAN_LOG("Attempting to enter the HTTP response phase..");
			if (client->httpresp.attempts >= 2) {
				/*
				** We have already made 2 (failed) attempts to prepare the HTTP response, abort.
				*/
				ELYSIAN_LOG("Maximum HTTP response attempts reached, aborting");
				return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
			} else {
				client->httpresp.attempts++;
				
				if (client->httpresp.current_status_code == ELYSIAN_HTTP_STATUS_CODE_500) {
					/*
					** Don't allow any other HTTP responses to be sent after that (etc a 404 after a 500)
					** This is going to block any infinite circular attempts to send the response.
					** For example "try to send status 500" -> ERR_FATAL -> "try to send status 500" -> ERR_FATAL ..
					*/
					client->httpresp.attempts = 2;
				}
				
				return elysian_schdlr_state_next(server, elysian_state_mvc_pre_configuration);
			}
        }break;
        case elysian_schdlr_EV_READ:
        {
			
        }break;
        case elysian_schdlr_EV_POLL:
        {
			
        }break;
		case elysian_schdlr_EV_TIMER1:
        case elysian_schdlr_EV_ABORT:
        {
			return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}

elysian_err_t elysian_state_mvc_pre_configuration(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;

	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timer1_set(server, 5000);
            elysian_schdlr_state_poll_enable(server);

        }break;
        case elysian_schdlr_EV_READ:
        {
			
        }break;
        case elysian_schdlr_EV_POLL:
        {
			elysian_mvc_clear(server);

			err = elysian_mvc_pre_configure(server);
			switch(err){
				case ELYSIAN_ERR_OK:
					return elysian_schdlr_state_next(server, elysian_state_prepare_http_response);
					break;
				case ELYSIAN_ERR_POLL:
					elysian_schdlr_state_poll_backoff(server);
					return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
					break;
				case ELYSIAN_ERR_FATAL:
					return elysian_schdlr_state_next(server, elysian_state_fatal_error_entry);
					break;
				default:
					ELYSIAN_ASSERT(0);
					return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
					break;
			};  
        }break;
		case elysian_schdlr_EV_TIMER1:
        case elysian_schdlr_EV_ABORT:
        {
			return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}


elysian_err_t elysian_state_prepare_http_response(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;

	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timer1_set(server, 5000);
            elysian_schdlr_state_poll_enable(server);
        }break;
        case elysian_schdlr_EV_READ:
        {
          
        }break;
        case elysian_schdlr_EV_POLL:
        {
			err = elysian_resource_open(server);
			switch(err){
				case ELYSIAN_ERR_OK:
				{
					return elysian_schdlr_state_next(server, elysian_state_mvc_post_configuration);
				} break;
				case ELYSIAN_ERR_POLL:
				{
					elysian_schdlr_state_poll_backoff(server);
					return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
				} break;
				case ELYSIAN_ERR_NOTFOUND:
				{
					if ((client->mvc.status_code >= ELYSIAN_HTTP_STATUS_CODE_200) && (client->mvc.status_code <= ELYSIAN_HTTP_STATUS_CODE_206)) {
						elysian_set_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_404);
					} else {
						elysian_set_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_500);
					}
					return elysian_schdlr_state_next(server, elysian_state_http_response_entry);
				} break;
				case ELYSIAN_ERR_FATAL:
				{
					return elysian_schdlr_state_next(server, elysian_state_fatal_error_entry);
				} break;
				default:
				{
					ELYSIAN_ASSERT(0);
					return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
				} break;
			};	
        }break;
		case elysian_schdlr_EV_TIMER1:
        case elysian_schdlr_EV_ABORT:
        {
			return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}

/*
** MVC configuration after the resource is opened
*/
elysian_err_t elysian_state_mvc_post_configuration(elysian_t* server, elysian_schdlr_ev_t ev) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	
    ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
    
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timer1_set(server, 5000);
            elysian_schdlr_state_poll_enable(server);
			
        }break;
        case elysian_schdlr_EV_READ:
        {
			
        }break;
        case elysian_schdlr_EV_POLL:
        {
			err = elysian_mvc_post_configure(server);
			if (err != ELYSIAN_ERR_OK) {
				elysian_resource_close(server);
				return elysian_schdlr_state_next(server, elysian_state_fatal_error_entry);
			}
			
			if (client->mvc.transfer_encoding == ELYSIAN_HTTP_TRANSFER_ENCODING_CHUNKED) {
				client->httpresp.body_size = -1; // Tranfer size is unknown, send until EOF.
			} else {
				client->httpresp.body_size = client->mvc.content_length;
			}
			
			return elysian_schdlr_state_next(server, elysian_state_build_http_response);
        }break;
		case elysian_schdlr_EV_TIMER1:
        case elysian_schdlr_EV_ABORT:
        {
			return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}

elysian_err_t elysian_state_build_http_response(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	
    ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
    
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timer1_set(server, 5000);
            elysian_schdlr_state_poll_enable(server);
			
			client->httpresp.buf_size = ELYSIAN_HTTP_RESPONSE_BODY_BUF_SZ_MAX;
        }break;
        case elysian_schdlr_EV_READ:
        {
        
        }break;
        case elysian_schdlr_EV_POLL:
        {
            /*
            ** Build HTTP response headers
            */
			client->httpresp.buf = elysian_mem_malloc(server, client->httpresp.buf_size);
			if(!client->httpresp.buf){
				elysian_schdlr_state_poll_backoff(server);
				return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
			}
            err = elysian_http_response_build(server);
			switch(err){
				case ELYSIAN_ERR_OK:
					client->httpresp.headers_size = client->httpresp.buf_len;
					if(client->httpreq.method == ELYSIAN_HTTP_METHOD_HEAD){
						client->httpresp.body_size = 0;
					}
					return elysian_schdlr_state_next(server, elysian_state_http_response_send);
					break;
				case ELYSIAN_ERR_BUF:
					elysian_mem_free(server, client->httpresp.buf);
					client->httpresp.buf = NULL;
					client->httpresp.buf_size += 128; /* The response could not fit, try increasing the buffer */
					elysian_schdlr_state_poll_enable(server); /* This is not a mem issue, reset backoff */
					return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
					break;
				case ELYSIAN_ERR_POLL:
					elysian_mem_free(server, client->httpresp.buf);
					client->httpresp.buf = NULL;
					elysian_schdlr_state_poll_backoff(server);
					return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
					break;
				case ELYSIAN_ERR_FATAL:
					elysian_resource_close(server);
					return elysian_schdlr_state_next(server, elysian_state_fatal_error_entry);
					break;
				default:
					ELYSIAN_ASSERT(0);
					return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
					break;
			}; 
        }break;
		case elysian_schdlr_EV_TIMER1:
        case elysian_schdlr_EV_ABORT:
        {
			return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}

elysian_err_t elysian_state_http_response_send(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
    uint32_t read_size;
    uint32_t read_size_actual;
    //uint32_t send_size;
    uint32_t send_size_actual;
    //uint32_t i;
	uint32_t packet_count;
	uint8_t read_complete;
	
	//ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
			ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
            elysian_schdlr_state_timer1_set(server, 5000);
            elysian_schdlr_state_poll_enable(server);
            elysian_schdlr_state_priority_set(server, elysian_schdlr_TASK_PRIO_HIGH);
			
            client->httpresp.sent_size = 0;
        }break;
        case elysian_schdlr_EV_READ:
        {

        }break;
        case elysian_schdlr_EV_POLL:
        {
            if (!client->httpresp.buf) {
#if 1
				client->httpresp.buf_size = (30 * ELYSIAN_MAX_MEMORY_USAGE_KB * 1024) / 100;
				client->httpresp.buf_size = client->httpresp.buf_size / 3;
				if (client->httpresp.buf_size > ELYSIAN_HTTP_RESPONSE_BODY_BUF_SZ_MAX) {
					client->httpresp.buf_size = ELYSIAN_HTTP_RESPONSE_BODY_BUF_SZ_MAX;
				}
				if (client->httpresp.buf_size < 128) {
					client->httpresp.buf_size = 128;
				}	
#else
				client->httpresp.buf_size = ELYSIAN_HTTP_RESPONSE_BODY_BUF_SZ_MAX;
#endif
				while (1) {
					client->httpresp.buf = elysian_mem_malloc(server, client->httpresp.buf_size);
					if (!client->httpresp.buf) {
						/*
						** HTTP response buffer allocation failed, try allocating a smaller buffer
						*/
						client->httpresp.buf_size = (client->httpresp.buf_size > 128) ? client->httpresp.buf_size - 128 : 0;
						if (client->httpresp.buf_size < 128) {
							elysian_schdlr_state_poll_backoff(server);
							return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
						}
					} else {
						/*
						** HTTP response buffer allocated
						*/
						client->httpresp.buf_index = 0;
						client->httpresp.buf_len = 0;
						break;
					}
				};
            }

			packet_count = 0;
			do {
				/*
				** Defrag
				*/
#if 0
				if (client->httpresp.buf_len == 0) {
					client->httpresp.buf_index = 0;
				} else {
					if((client->httpresp.buf_index * 100) / client->httpresp.buf_size >= 25){
						ELYSIAN_LOG("Defragging, index = %u/%u", client->httpresp.buf_index, client->httpresp.buf_size);
						for(i = 0; i < client->httpresp.buf_size - client->httpresp.buf_index; i++){
							client->httpresp.buf[i] = client->httpresp.buf[client->httpresp.buf_index + i];
						}
						client->httpresp.buf_index = 0;
					}
				}
#else
				if (client->httpresp.buf_len == 0) {
					client->httpresp.buf_index = 0;
				}
#endif
				
				/*
				** Read
				*/
				read_complete = 0;
				if (client->httpresp.body_size == (uint32_t) -1 ) {
					// Read until EOF (Chunked HTTP Response)
					read_size = -1;
				} else {
					// Read only partial range
					read_size = client->httpresp.headers_size + client->httpresp.body_size - (client->httpresp.sent_size + client->httpresp.buf_len);
				}
				
				if (read_size) {
					/*
					** We haven't read the whole resource, read more data if possible
					*/
					if (read_size > client->httpresp.buf_size - (client->httpresp.buf_index + client->httpresp.buf_len)) {
						read_size = client->httpresp.buf_size - (client->httpresp.buf_index + client->httpresp.buf_len);
					}
					
					if (read_size) {
						err = elysian_resource_read(server, &client->httpresp.buf[client->httpresp.buf_index + client->httpresp.buf_len], read_size, &read_size_actual);
						switch(err){
							case ELYSIAN_ERR_OK:
								client->httpresp.buf_len += read_size_actual;
								//ELYSIAN_LOG("Client %u HTTP response: read %u bytes.", read_size_actual);
								if (read_size_actual < read_size) {
									/*
									** The whole resource was read
									*/
									read_complete = 1;
								}
								break;
							case ELYSIAN_ERR_FATAL:
								return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
								break;
							default:
								ELYSIAN_ASSERT(0);
								return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
								break;
						};
					}
				} else {
					/*
					** The whole resource was read
					*/
					read_complete = 1;
				}

				/*
				** Send
				*/
				if (client->httpresp.buf_len) {
					err = elysian_socket_write(&client->socket, &client->httpresp.buf[client->httpresp.buf_index], client->httpresp.buf_len, &send_size_actual);
					switch(err){
						case ELYSIAN_ERR_OK:
							//ELYSIAN_LOG("Client %u HTTP response: sent %u bytes.", send_size_actual);
							elysian_schdlr_state_timer1_reset(server);
							client->httpresp.buf_index += send_size_actual;
							client->httpresp.buf_len -= send_size_actual;
							client->httpresp.sent_size += send_size_actual;
							packet_count++;
							break;
						case ELYSIAN_ERR_POLL:
							elysian_schdlr_state_poll_backoff(server); 
							return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
							break;
						default:
							return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
							break;
					};
				} 

				/*
				** Check if finished
				*/
				if ((read_complete == 1) && (client->httpresp.buf_len == 0)) {
					ELYSIAN_LOG("Client %u HTTP response: Completed!", client->id);
					if ((client->httpreq.connection == ELYSIAN_HTTP_CONNECTION_UPGRADE) && (client->httpreq.connection_upgrade == ELYSIAN_HTTP_CONNECTION_UPGRADE_WEBSOCKET)) {
						return elysian_schdlr_state_next(server, elysian_state_websocket);
					}
					
					if ((client->mvc.connection == ELYSIAN_HTTP_CONNECTION_KEEPALIVE) && (client->http_pipelining_enabled)) {
						return elysian_schdlr_state_next(server, elysian_state_http_keepalive);
					}else{
						return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
					}
				}
            } while (packet_count < 8);
			
            /*
            ** Check if we can release the response buffer so it can be used by other clients
            */
            if(client->httpresp.buf_len == 0){
                elysian_mem_free(server, client->httpresp.buf);
                client->httpresp.buf = NULL;
            }
            
            elysian_schdlr_state_poll_enable(server);
        }break;
		case elysian_schdlr_EV_TIMER1:
        case elysian_schdlr_EV_ABORT:
        {
			return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}

elysian_err_t elysian_state_websocket(elysian_t* server, elysian_schdlr_ev_t ev) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_cbuf_t* cbuf;
	elysian_err_t err;
	
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {    
            elysian_schdlr_state_priority_set(server, elysian_schdlr_TASK_PRIO_HIGH);
			
			ELYSIAN_ASSERT(client->websocket.def == NULL);
			
			/* This should be called before cleanup, since we need to retrieve 
			the websocket controller from the HTTP req URL */
			err = elysian_websocket_connected(server);
			if (err != ELYSIAN_ERR_OK) {
				return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
			}
			
			err = elysian_client_cleanup(server);
			if (err != ELYSIAN_ERR_OK) {
				ELYSIAN_LOG("Cleanup failed!");
				ELYSIAN_ASSERT(0);
				return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
				
			}
			
			client->isp.func = elysian_isp_websocket;
			
			/*
			** Don't break, check for any received data
			*/
        }//break;
        case elysian_schdlr_EV_READ:
        {
			cbuf = elysian_schdlr_state_socket_read(server);
			elysian_cbuf_list_append(&client->rcv_cbuf_list, cbuf);
            cbuf_list_print(client->rcv_cbuf_list);
			
			elysian_schdlr_state_poll_enable(server);
        }break;
        case elysian_schdlr_EV_POLL:
        {
			err = elysian_websocket_process(server);
			switch(err){
                case ELYSIAN_ERR_OK:
					/* No pending operations */
					elysian_schdlr_state_poll_disable(server);
					break;
                case ELYSIAN_ERR_POLL:
					/* Pending Rx frames exist */
					elysian_schdlr_state_poll_backoff(server);
					return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
                    break;
                case ELYSIAN_ERR_FATAL:
                default:
					return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
                    break;
            };		  
        }break;
		case elysian_schdlr_EV_TIMER1:
		{
			err = elysian_websocket_app_timer(server);
			if (err != ELYSIAN_ERR_OK) {
				return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
			}
			
			/* Timer might have generated Tx packets, enable poll */
			elysian_schdlr_state_poll_enable(server);
			
		} break;
		case elysian_schdlr_EV_TIMER2:
		{
			err = elysian_websocket_ping_timer(server);
			if (err != ELYSIAN_ERR_OK) {
				return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
			}
			
			/* Timer might have generated Tx packets, enable poll */
			elysian_schdlr_state_poll_enable(server);
			
		} break;
        case elysian_schdlr_EV_ABORT:
        {
			return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}

elysian_err_t elysian_state_http_keepalive(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {           
            elysian_schdlr_state_timer1_set(server, 10000);
            elysian_schdlr_state_poll_enable(server);
            elysian_schdlr_state_priority_set(server, elysian_schdlr_TASK_PRIO_HIGH);
        }break;
        case elysian_schdlr_EV_READ:
        {
        }break;
        case elysian_schdlr_EV_POLL:
        {
			/*
			** Never exit unless we free up all the resources ?
			*/
			elysian_schdlr_state_timer1_reset(server);
			
			err = elysian_client_cleanup(server);
			if (err != ELYSIAN_ERR_OK) {
				ELYSIAN_LOG("Cleanup failed!");
				return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
			}
			
            /*
            ** This cb should be called after client_cleanup() to make sure
            ** that the web server has closed any file handles and so any files 
            ** created from within controllre can be now safely removed by the application.
            */
            if(client->httpreq_onservice_handler){
                client->httpreq_onservice_handler(server, client->httpreq_onservice_handler_data);
            }

			return elysian_schdlr_state_next(server, elysian_state_http_request_headers_receive);
        }break;
		case elysian_schdlr_EV_TIMER1:
        case elysian_schdlr_EV_ABORT:
        {
			return elysian_schdlr_state_next(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}

elysian_err_t elysian_state_http_disconnect(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timer1_set(server, 10000);
            elysian_schdlr_state_poll_enable(server);
            elysian_schdlr_state_priority_set(server, elysian_schdlr_TASK_PRIO_HIGH);
			
			ELYSIAN_LOG("Disconnecting HTTP client #%u", client->id);
        }break;
        case elysian_schdlr_EV_READ:
        {
        }break;
        case elysian_schdlr_EV_POLL:
        {
			/*
			** Never exit unless we free up all the resources ?
			*/
			elysian_schdlr_state_timer1_reset(server);
			
			err = elysian_client_cleanup(server);
			if (err != ELYSIAN_ERR_OK) {
				ELYSIAN_LOG("Cleanup failed!");
				return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
			}
			
            /*
            ** This cb should be called after client_cleanup() to make sure
            ** that the web server has closed any file handles and so any files 
            ** created from within controllre can be now safely removed by the application.
            */
            if(client->httpreq_onservice_handler){
                client->httpreq_onservice_handler(server, client->httpreq_onservice_handler_data);
            }
            
            if(client->rcv_cbuf_list){
				elysian_cbuf_list_free(server, client->rcv_cbuf_list );
				client->rcv_cbuf_list  = NULL;
			}
			
			if(client->store_cbuf_list){
				elysian_cbuf_list_free(server, client->store_cbuf_list );
				client->rcv_cbuf_list  = NULL;
			}
			
			/*
			** Clear websockets
			*/
			elysian_websocket_cleanup(server);
			
            return elysian_schdlr_state_next(server, NULL);
        }break;
		case elysian_schdlr_EV_TIMER1:
        case elysian_schdlr_EV_ABORT:
        {
			/*
			** Ignore any abort signals, we should cleanup before exiting
			*/
        }break;
        default:
        {
            ELYSIAN_ASSERT(0);
        }break;
    };
	
	return elysian_schdlr_state_next(server, elysian_schdlr_same_state);
}

elysian_err_t elysian_client_cleanup(elysian_t* server){
    elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err = ELYSIAN_ERR_OK;
	elysian_req_param_t* param_next;
	
    /*
    ** Clear mvc
    */
    elysian_mvc_clear(server);

	/*
	** Clear ISP
	*/
	elysian_isp_cleanup(server);
	
    /*
    ** Release resource
    */
    if(elysian_resource_isopened(server)){
        elysian_resource_close(server);
    }
    
    /*
    ** Release response
    */
    if(client->httpresp.buf){
        elysian_mem_free(server, client->httpresp.buf);
        client->httpresp.buf = NULL;
    }
    
    if(client->httpreq.url){
        elysian_mem_free(server, client->httpreq.url);
        client->httpreq.url = NULL;
    }  

	if(client->httpreq.multipart_boundary){
		elysian_mem_free(server, client->httpreq.multipart_boundary);
		client->httpreq.multipart_boundary = NULL;
	}
	
	/*
	** Release HTTP request parameters
	*/
	while(client->httpreq.params){
		param_next = client->httpreq.params->next;
		if(client->httpreq.params->name){
			elysian_mem_free(server, client->httpreq.params->name);
		}
		if(client->httpreq.params->filename){
			elysian_mem_free(server, client->httpreq.params->filename);
		}
		elysian_mem_free(server, client->httpreq.params);
		client->httpreq.params = param_next;
	};
	
	if(client->httpreq.headers_filename[0] != '\0'){
        ELYSIAN_LOG("Removing header file..");
		if(elysian_fs_fisopened(server, &client->httpreq.headers_file)){
            ELYSIAN_LOG("Closing headers file..");
			elysian_fs_fclose(server, &client->httpreq.headers_file);
		}
		err = elysian_fs_fremove(server, client->httpreq.headers_filename);
		if(err != ELYSIAN_ERR_OK) {
			return err;
		}
		strcpy(client->httpreq.headers_filename, "");
	}
    
    if(client->httpreq.body_filename[0] != '\0'){
		if(elysian_fs_fisopened(server, &client->httpreq.body_file)){
            ELYSIAN_LOG("Closing body file..");
			elysian_fs_fclose(server, &client->httpreq.body_file);
		}
		elysian_fs_fremove(server, client->httpreq.body_filename);
		if(err != ELYSIAN_ERR_OK) {
			return err;
		}
		strcpy(client->httpreq.body_filename, "");
	}
	
	return ELYSIAN_ERR_OK;
}

/* ---------------------------------------------------------------------------------------------------- */
#include <signal.h>

elysian_t* elysian_new(){
    elysian_t* server;
    
    if(!(server = elysian_mem_malloc(NULL, sizeof(elysian_t)))){
        return NULL;
    }
    
    server->controller_def = NULL;
	server->file_rom_def = NULL;
	server->file_vrt_def = NULL;

    return server;
}

elysian_err_t elysian_start(elysian_t* server, uint16_t port, const elysian_mvc_controller_def_t controller_def[], const elysian_file_rom_def_t file_rom_def[], const elysian_file_vrt_def_t file_vrt_def[], const elysian_websocket_def_t websocket_def[], elysian_authentication_cb_t authentication_cb) {
    elysian_err_t err;
	
#if defined(ELYSIAN_OS_ENV_UNIX)
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) { 
		ELYSIAN_LOG("Could not ignore the SIGPIPE signal!")
	}
#endif
	server->controller_def = (elysian_mvc_controller_def_t*) controller_def;
	server->file_rom_def = (elysian_file_rom_def_t*) file_rom_def;
	server->file_vrt_def = (elysian_file_vrt_def_t*) file_vrt_def;
	server->websocket_def = (elysian_websocket_def_t*) websocket_def;
	
	server->listening_port = port;
	server->authentication_cb = authentication_cb;
	
	
    err = elysian_schdlr_init(server, port, elysian_state_http_connection_accepted);
    
    return err;
}

void elysian_stop(elysian_t* server){
	elysian_schdlr_stop(server);
}

elysian_err_t elysian_poll(elysian_t* server, uint32_t interval_ms){
    elysian_schdlr_poll(server, interval_ms);
    return ELYSIAN_ERR_OK;
}

elysian_client_t* elysian_current_client(elysian_t* server) {
	return elysian_schdlr_current_client_get(server);
}
