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
    //[elysian_schdlr_EV_TIMER] = "EV_TIMER",
    [elysian_schdlr_EV_ABORT] = "EV_ABORT",
};

void elysian_state_http_connection_accepted(elysian_t* server, elysian_schdlr_ev_t ev);
void elysian_state_http_request_headers_receive(elysian_t* server, elysian_schdlr_ev_t ev);
void elysian_state_http_request_headers_store(elysian_t* server, elysian_schdlr_ev_t ev);
void elysian_state_http_request_headers_parse(elysian_t* server, elysian_schdlr_ev_t ev);
void elysian_state_http_expect_reply(elysian_t* server, elysian_schdlr_ev_t ev);
void elysian_state_http_request_body_receive(elysian_t* server, elysian_schdlr_ev_t ev);
void elysian_state_http_request_authenticate(elysian_t* server, elysian_schdlr_ev_t ev);
void elysian_state_fatal_error_entry(elysian_t* server, elysian_schdlr_ev_t ev);
void elysian_state_http_response_entry(elysian_t* server, elysian_schdlr_ev_t ev);
void elysian_state_configure_mvc(elysian_t* server, elysian_schdlr_ev_t ev);
void elysian_state_prepare_http_response(elysian_t* server, elysian_schdlr_ev_t ev);
void elysian_state_build_http_response(elysian_t* server, elysian_schdlr_ev_t ev);
void elysian_state_http_response_send(elysian_t* server, elysian_schdlr_ev_t ev);
void elysian_state_http_cleanup(elysian_t* server, elysian_schdlr_ev_t ev);
void elysian_state_http_keepalive(elysian_t* server, elysian_schdlr_ev_t ev);
void elysian_state_http_disconnect(elysian_t* server, elysian_schdlr_ev_t ev);

elysian_err_t elysian_client_cleanup(elysian_t* server);


void elysian_state_http_connection_accepted(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	static uint32_t client_unique_id = 0;
    
	ELYSIAN_LOG("[event = %s] ", elysian_schdlr_ev_name[ev]);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            ELYSIAN_LOG("Connection %u accepted! ====================================================================== ", client_unique_id + 1);
            
            client->id = ++client_unique_id;
            
			client->rcv_cbuf_list = NULL;
			client->store_cbuf_list = NULL;

            client->httpreq.url = NULL;
            client->httpreq.multipart_boundary = NULL;
			
            elysian_fs_finit(server, &client->httpreq.headers_file);
			strcpy(client->httpreq.headers_filename, "");
            
            elysian_fs_finit(server, &client->httpreq.body_file);
			strcpy(client->httpreq.body_filename, "");

			elysian_mvc_init(server);
			
			client->httpresp.redirection_url = NULL;
            client->httpresp.buf = NULL;

			elysian_resource_init(server);
			
            elysian_schdlr_state_set(server, elysian_state_http_request_headers_receive);
			return;
        }break;
        default:
        {
            ELYSIAN_ASSERT(0, "");
        }break;
    };
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
	elysian_mvc_controller_t* controller;

    if (client->isp.func == elysian_isp_http_headers) {
        file = &client->httpreq.headers_file;
        filename = client->httpreq.headers_filename;
        filename_template = ELYSIAN_FS_RAM_VRT_ROOT"/h_%u";
    } else {
        file = &client->httpreq.body_file;
        filename = client->httpreq.body_filename;
		controller = elysian_mvc_controller_get(server, client->httpreq.url, client->httpreq.method);
		if((controller) && (controller->flags & ELYSIAN_MVC_CONTROLLER_FLAG_SAVE_TO_DISK)) {
			filename_template = ELYSIAN_FS_DISK_VRT_ROOT"/b_%u";
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
				ELYSIAN_ASSERT(strlen(filename) < sizeof(client->httpreq.headers_filename), "");
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
				err = elysian_fs_fopen(server, filename, ELYSIAN_FILE_MODE_READ, file);
				return err;
			}
		} else {
			/*
			** File is opened, continue writting
			*/
			ELYSIAN_LOG("writting cbuf chain..");
			while(client->store_cbuf_list){
				ELYSIAN_LOG("writting cbuf..");
				cbuf = client->store_cbuf_list;

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
			if (client->store_cbuf_list_size == 0) {
				/*
				** The whore range was stored, close the file
				*/
				ELYSIAN_ASSERT(client->store_cbuf_list == NULL, "");
				ELYSIAN_ASSERT(client->store_cbuf_list_offset == 0, "");
				elysian_fs_fclose(server, file);
			}
		}
	}
}

void elysian_state_http_request_store(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_cbuf_t* cbuf;
    elysian_err_t err;
    
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timeout_set(server, 4000);
			elysian_schdlr_state_priority_set(server, elysian_schdlr_TASK_PRIO_LOW);

			ELYSIAN_ASSERT(client->store_cbuf_list == NULL, "");
			
			/*
			** Suppose we expect infinity bytes, until the input stream 
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
			elysian_schdlr_state_timeout_reset(server);
			
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
					break;
				case ELYSIAN_ERR_READ:
					if(client->store_cbuf_list == NULL) {
						/*
						** Input stream processor needs more data, disable POLL, wait READ
						*/
						elysian_schdlr_state_poll_disable(server);
					} else {
						/*
						** We need poll to store the cbuf list
						*/
					}
                    break;
                case ELYSIAN_ERR_POLL:
                    elysian_schdlr_state_poll_backoff(server);
					return;
                    break;
                case ELYSIAN_ERR_FATAL:
                    elysian_schdlr_state_set(server, elysian_state_fatal_error_entry);
					return;
                    break;
                default:
                    ELYSIAN_ASSERT(0, "");
                    break;
            };
			
			err = elysian_store_cbuf_to_file(server);
            switch(err){
                case ELYSIAN_ERR_OK:
					if (client->isp.func == elysian_isp_http_headers) {
						/*
						** The whole HTTP request headers were received and stored
						*/
						elysian_schdlr_state_set(server, elysian_state_http_request_headers_parse);
						return;
					} else {
						/*
						** The whole HTTP request body was received and stored
						*/
						elysian_schdlr_state_set(server, elysian_state_http_request_authenticate);
						return;
					}
                    break;
                case ELYSIAN_ERR_POLL:
                    elysian_schdlr_state_poll_backoff(server);
					return;
                    break;
                case ELYSIAN_ERR_FATAL:
                    elysian_schdlr_state_set(server, elysian_state_fatal_error_entry);
					return;
                    break;
                default:
					ELYSIAN_LOG("err was %u",err);
                    ELYSIAN_ASSERT(0, "");
                    break;
            };
        }break;
        case elysian_schdlr_EV_ABORT:
        {
			elysian_schdlr_state_set(server, elysian_state_http_disconnect);
			return;
        }break;
        default:
        {
            ELYSIAN_ASSERT(0, "");
        }break;
    };
}

void elysian_state_http_request_headers_receive(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_cbuf_t* cbuf;
    elysian_err_t err;
    
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timeout_set(server, 4000);
			elysian_schdlr_state_priority_set(server, elysian_schdlr_TASK_PRIO_LOW);
			
			ELYSIAN_ASSERT(client->store_cbuf_list == NULL, "");
			
			client->http_pipelining_enabled = 0;
			client->httpresp.status_code = ELYSIAN_HTTP_STATUS_CODE_NA;
			client->httpresp.fatal_status_code = ELYSIAN_HTTP_STATUS_CODE_NA;
			client->httpresp.attempts = 0;
			
            client->httpresp.keep_alive = 1;
            client->reqserved_cb = NULL;
			client->reqserved_cb_data = NULL;
			
			memset(&client->isp, 0, sizeof(client->isp));
			
			client->isp.func = elysian_isp_http_headers;
			
			elysian_schdlr_state_set(server, elysian_state_http_request_store);
			return;
        };break;
        case elysian_schdlr_EV_READ:
        {

        }break;
        case elysian_schdlr_EV_ABORT:
        {
			elysian_schdlr_state_set(server, elysian_state_http_disconnect);
			return;
        }break;
        default:
        {
            ELYSIAN_ASSERT(0, "");
        }break;
    };
}

void elysian_state_http_request_body_receive(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_cbuf_t* cbuf;
    elysian_err_t err;
    
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timeout_set(server, 4000);
			elysian_schdlr_state_priority_set(server, elysian_schdlr_TASK_PRIO_LOW);
			
			ELYSIAN_ASSERT(client->store_cbuf_list == NULL, "");
			
			memset(&client->isp, 0, sizeof(client->isp));
			
			if (client->httpreq.transfer_encoding == ELYSIAN_HTTP_TRANSFER_ENCODING_RAW) {
				if (client->httpreq.content_type == ELYSIAN_HTTP_CONTENT_TYPE_MULTIPART__FORM_DATA) {
					client->isp.func = elysian_isp_http_body_raw_multipart;
				} else {
					client->isp.func = elysian_isp_http_body_raw;
				}
			} else if (client->httpreq.transfer_encoding == ELYSIAN_HTTP_TRANSFER_ENCODING_CHUNKED) {
				if (client->httpreq.content_type == ELYSIAN_HTTP_CONTENT_TYPE_MULTIPART__FORM_DATA) {
					//client->isp.func = elysian_isp_chunked_multipart;
				} else {
					//client->isp.func = elysian_isp_chunked;
				}
			} else {
				elysian_schdlr_state_set(server, elysian_state_http_disconnect);
				return;
			}
			
			elysian_schdlr_state_set(server, elysian_state_http_request_store);
			return;

        };break;
        case elysian_schdlr_EV_READ:
        {

        }break;
        case elysian_schdlr_EV_ABORT:
        {
			elysian_schdlr_state_set(server, elysian_state_http_disconnect);
			return;
        }break;
        default:
        {
            ELYSIAN_ASSERT(0, "");
        }break;
    };
}



void elysian_state_http_request_headers_parse(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
    
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timeout_set(server, 4000);
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
					ELYSIAN_LOG("Expect status code is %u\r\n", client->httpreq.expect_status_code);
					if(client->httpreq.expect_status_code != ELYSIAN_HTTP_STATUS_CODE_NA){
						elysian_schdlr_state_set(server, elysian_state_http_expect_reply);
						return;
					} else {
						elysian_schdlr_state_set(server, elysian_state_http_request_body_receive);
						return;
					}
                    break;
                case ELYSIAN_ERR_POLL:
                    elysian_schdlr_state_poll_backoff(server);
					return;
                    break;
                case ELYSIAN_ERR_FATAL:
                    elysian_schdlr_state_set(server, elysian_state_fatal_error_entry);
					return;
                    break;
                default:
                    ELYSIAN_ASSERT(0, "");
                    break;
            };
        }break;
        case elysian_schdlr_EV_ABORT:
        {
			elysian_schdlr_state_set(server, elysian_state_http_disconnect);
			return;
        }break;
        default:
        {
            ELYSIAN_ASSERT(0, "");
        }break;
    };
}

void elysian_state_http_expect_reply(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	uint32_t send_size_actual;
	char http_expect_response[64];
    elysian_err_t err;
    
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timeout_set(server, 4000);
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
							elysian_schdlr_state_set(server, elysian_state_http_request_body_receive);
							return;
						} else {
							elysian_schdlr_state_set(server, elysian_state_http_disconnect);
							return;
						}
					}
					break;
				case ELYSIAN_ERR_POLL:
					elysian_schdlr_state_poll_backoff(server);
					return;
					break;
				default:
					elysian_schdlr_state_set(server, elysian_state_http_disconnect);
					return;
					break;
			};
        }break;
        case elysian_schdlr_EV_ABORT:
        {
			elysian_schdlr_state_set(server, elysian_state_http_disconnect);
        }break;
        default:
        {
            ELYSIAN_ASSERT(0, "");
        }break;
    };
}

void elysian_state_http_request_authenticate(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
    
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timeout_set(server, 4000);
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
					elysian_schdlr_state_set(server, elysian_state_http_response_entry);
					return;
					break;
				case ELYSIAN_ERR_AUTH:
					ELYSIAN_LOG("Authentication failure!");
					elysian_set_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_401);
					elysian_schdlr_state_set(server, elysian_state_http_response_entry);
					return;
					break;
				case ELYSIAN_ERR_POLL:
					elysian_schdlr_state_poll_backoff(server);
					return;
					break;
				case ELYSIAN_ERR_FATAL:
					elysian_schdlr_state_set(server, elysian_state_fatal_error_entry);
					return;
					break;
				default:
					ELYSIAN_ASSERT(0, "");
					break;
			};
        }break;
        case elysian_schdlr_EV_ABORT:
        {
			elysian_schdlr_state_set(server, elysian_state_http_disconnect);
			return;
        }break;
        default:
        {
            ELYSIAN_ASSERT(0, "");
        }break;
    };
}


void elysian_set_http_status_code(elysian_t* server, elysian_http_status_code_e status_code) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	
	ELYSIAN_LOG("HTTP status changed to %u", elysian_http_get_status_code_num(status_code));
	//uint8_t* keep[2];
	
	if(status_code != ELYSIAN_HTTP_STATUS_CODE_206) {
		client->httpreq.range_start = ELYSIAN_HTTP_RANGE_WF;
		client->httpreq.range_end = ELYSIAN_HTTP_RANGE_WF;		
	}
	
	
	/*
	** Set status code
	*/
	client->httpresp.status_code = status_code;
}

void elysian_set_fatal_http_status_code(elysian_t* server, elysian_http_status_code_e status_code) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	client->httpresp.fatal_status_code = status_code;
}

void elysian_state_fatal_error_entry(elysian_t* server, elysian_schdlr_ev_t ev){
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
			elysian_schdlr_state_set(server, elysian_state_http_response_entry);
			return;
        }break;
        case elysian_schdlr_EV_READ:
        {
			
        }break;
        case elysian_schdlr_EV_POLL:
        {
			
        }break;
        case elysian_schdlr_EV_ABORT:
        {
			elysian_schdlr_state_set(server, elysian_state_http_disconnect);
			return;
        }break;
        default:
        {
            ELYSIAN_ASSERT(0, "");
        }break;
    };
}

void elysian_state_http_response_entry(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
			ELYSIAN_LOG("Attempting to enter the HTTP response phase..");
			uint8_t max_http_resp_attempts = 2; // This will allow 200 -> 500, user defined -> 500
			if (client->httpresp.attempts == max_http_resp_attempts) {
				/*
				** We have already made 2 attempts to prepare the HTTP response, abort.
				** This is going to block any infinite circular attempts to send the response.
				** 
				*/
				ELYSIAN_LOG("Maximum HTTP response attempts reached, aborting");
				elysian_schdlr_state_set(server, elysian_state_http_disconnect);
			} else {
				client->httpresp.attempts++;
				elysian_schdlr_state_set(server, elysian_state_configure_mvc);
				if (client->httpresp.status_code == ELYSIAN_HTTP_STATUS_CODE_500) {
					/*
					** Don't allow any other HTTP responses to be sent after that (etc a 404 after a 500)
					** This is going to block any infinite circular attempts to send the response.
					** For example "try to send status 500" -> ERR_FATAL -> "try to send status 500" -> ERR_FATAL ..
					*/
					client->httpresp.attempts = max_http_resp_attempts;
				}
			}
			return;
        }break;
        case elysian_schdlr_EV_READ:
        {
			
        }break;
        case elysian_schdlr_EV_POLL:
        {
			
        }break;
        case elysian_schdlr_EV_ABORT:
        {
			elysian_schdlr_state_set(server, elysian_state_http_disconnect);
			return;
        }break;
        default:
        {
            ELYSIAN_ASSERT(0, "");
        }break;
    };
}

void elysian_state_configure_mvc(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	char status_code_page_name[32];
	elysian_err_t err;

	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timeout_set(server, 5000);
            elysian_schdlr_state_poll_enable(server);
        }break;
        case elysian_schdlr_EV_READ:
        {
			
        }break;
        case elysian_schdlr_EV_POLL:
        {
			elysian_mvc_clear(server);

			if(client->httpresp.status_code != ELYSIAN_HTTP_STATUS_CODE_NA) {
				/*
				** Status code is available, no need to query application layer
				*/
				ELYSIAN_LOG("Trying to open error page %u", elysian_http_get_status_code_num(client->httpresp.status_code));
				elysian_sprintf(status_code_page_name, ELYSIAN_FS_WS_VRT_ROOT"/%u.html", elysian_http_get_status_code_num(client->httpresp.status_code));
				err = elysian_mvc_set_view(server, status_code_page_name);
			} else {
				/*
				** Status code is not available, query application layer.
				** This will allow app layer to set the desired status (etc redirection).
				** If no status code is set, we are going to set one automatically depending the resource availability.
				*/
				err = elysian_mvc_configure(server);
			}
			
			switch(err){
				case ELYSIAN_ERR_OK:
					elysian_schdlr_state_set(server, elysian_state_prepare_http_response);
					return;
					break;
				case ELYSIAN_ERR_POLL:
					elysian_schdlr_state_poll_backoff(server);
					return;
					break;
				case ELYSIAN_ERR_FATAL:
					elysian_schdlr_state_set(server, elysian_state_fatal_error_entry);
					return;
					break;
				default:
					ELYSIAN_ASSERT(0, "");
					elysian_schdlr_state_set(server, elysian_state_http_disconnect);
					return;
					break;
			};  
        }break;
        case elysian_schdlr_EV_ABORT:
        {
			elysian_schdlr_state_set(server, elysian_state_http_disconnect);
			return;
        }break;
        default:
        {
            ELYSIAN_ASSERT(0, "");
        }break;
    };
}


void elysian_state_prepare_http_response(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;

	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timeout_set(server, 5000);
            elysian_schdlr_state_poll_enable(server);
        }break;
        case elysian_schdlr_EV_READ:
        {
          
        }break;
        case elysian_schdlr_EV_POLL:
        {
			/*
            ** Setup filestream
            */
			if (client->httpresp.status_code == ELYSIAN_HTTP_STATUS_CODE_NA) {
				/*
				** HTTP status has not been set, check if it is 200, 206 or 404
				*/
				if((client->httpreq.range_start == ELYSIAN_HTTP_RANGE_WF) && (client->httpreq.range_end == ELYSIAN_HTTP_RANGE_WF)){
					err = elysian_resource_open(server, 0, &client->httpresp.resource_size);
					switch(err){
						case ELYSIAN_ERR_OK:
						{
							elysian_set_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_200);
							client->httpresp.body_size = client->httpresp.resource_size;
							elysian_schdlr_state_set(server, elysian_state_build_http_response);
							return;
						}
						case ELYSIAN_ERR_NOTFOUND:
						{
							elysian_set_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_404);
							elysian_schdlr_state_set(server, elysian_state_http_response_entry);
							return;
						}
						default:
						{
							break;
						}
					};
				}else{
					err = elysian_resource_open(server, client->httpreq.range_start, &client->httpresp.resource_size);
					switch(err){
						case ELYSIAN_ERR_OK:
						{
							if(client->httpreq.range_end == ELYSIAN_HTTP_RANGE_EOF){
								client->httpreq.range_end = client->httpresp.resource_size;
								if(client->httpreq.range_end) {
									client->httpreq.range_end--;
								}
							}
							if(client->httpreq.range_end >= client->httpresp.resource_size){
								err = ELYSIAN_ERR_FATAL;
								elysian_set_fatal_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_400);
								break;
							}
							elysian_set_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_206);
							client->httpresp.body_size = client->httpreq.range_end - client->httpreq.range_start + 1;
							elysian_schdlr_state_set(server, elysian_state_build_http_response);
							return;
						}
						case ELYSIAN_ERR_NOTFOUND:
						{
							elysian_set_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_404);
							elysian_schdlr_state_set(server, elysian_state_http_response_entry);
							return;
						}
						default:
						{
							break;
						}
					};
				}
			} else {
				/*
				** HTTP status has been already set, don't bypass it. 
				** If it is not found, generate 500 status code.
				*/
				err = elysian_resource_open(server, 0, &client->httpresp.resource_size);
				switch(err){
					case ELYSIAN_ERR_OK:
					{
						client->httpresp.body_size = client->httpresp.resource_size;
						elysian_schdlr_state_set(server, elysian_state_build_http_response);
						return;
					}
					case ELYSIAN_ERR_NOTFOUND:
					{
						elysian_set_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_500);
						elysian_schdlr_state_set(server, elysian_state_http_response_entry);
						return;
					}
					default:
					{
						break;
					}
				};
			}
			
			switch(err){
				case ELYSIAN_ERR_POLL:
				{
					elysian_schdlr_state_poll_backoff(server);
					return;
					break;
				}
				case ELYSIAN_ERR_FATAL:
				{
					elysian_schdlr_state_set(server, elysian_state_fatal_error_entry);
					return;
					break;
				}
				default:
				{
					ELYSIAN_ASSERT(0, "");
					elysian_schdlr_state_set(server, elysian_state_http_disconnect);
					return;
					break;
				}
			};
        }break;
        case elysian_schdlr_EV_ABORT:
        {
			elysian_schdlr_state_set(server, elysian_state_http_disconnect);
			return;
        }break;
        default:
        {
            ELYSIAN_ASSERT(0, "");
        }break;
    };
}



void elysian_state_build_http_response(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	
    ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
    
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timeout_set(server, 5000);
            elysian_schdlr_state_poll_enable(server);
			
			client->httpresp.buf_size = ELYSIAN_HTTP_RESPONSE_BODY_BUF_SZ_MAX;
			
			ELYSIAN_LOG("Resource '%s' opened, size is (%u)", client->mvc.view, client->httpresp.resource_size);
        }break;
        case elysian_schdlr_EV_READ:
        {
        
        }break;
        case elysian_schdlr_EV_POLL:
        {
            /*
            ** Build HTTP response headers
            */
			client->httpresp.buf = elysian_mem_malloc(server, client->httpresp.buf_size, ELYSIAN_MEM_MALLOC_PRIO_HIGH);
			if(!client->httpresp.buf){
				elysian_schdlr_state_poll_backoff(server);
				return;
			}
            err = elysian_http_response_build(server);
			switch(err){
				case ELYSIAN_ERR_OK:
					client->httpresp.headers_size = client->httpresp.buf_len;
					if(client->httpreq.method == ELYSIAN_HTTP_METHOD_HEAD){
						client->httpresp.body_size = 0;
					}
					elysian_schdlr_state_set(server, elysian_state_http_response_send);
					return;
					break;
				case ELYSIAN_ERR_BUF:
					elysian_mem_free(server, client->httpresp.buf);
					client->httpresp.buf = NULL;
					client->httpresp.buf_size += 128; /* The response could not fit, try increasing the buffer */
					elysian_schdlr_state_poll_enable(server); /* This is not a mem issue, reset backoff */
					return;
					break;
				case ELYSIAN_ERR_POLL:
					elysian_mem_free(server, client->httpresp.buf);
					client->httpresp.buf = NULL;
					elysian_schdlr_state_poll_backoff(server);
					return;
					break;
				case ELYSIAN_ERR_FATAL:
					elysian_schdlr_state_set(server, elysian_state_fatal_error_entry);
					return;
					break;
				default:
					ELYSIAN_ASSERT(0, "");
					elysian_schdlr_state_set(server, elysian_state_http_disconnect);
					return;
					break;
			}; 
        }break;
        case elysian_schdlr_EV_ABORT:
        {
			elysian_schdlr_state_set(server, elysian_state_http_disconnect);
			return;
        }break;
        default:
        {
            ELYSIAN_ASSERT(0, "");
        }break;
    };
}

void elysian_state_http_response_send(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
    uint32_t read_size;
    uint32_t read_size_actual;
    uint32_t send_size;
    uint32_t send_size_actual;
    uint32_t i;
	uint32_t packet_count;
	
	//ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
			ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
            elysian_schdlr_state_timeout_set(server, 5000);
            elysian_schdlr_state_poll_enable(server);
            elysian_schdlr_state_priority_set(server, elysian_schdlr_TASK_PRIO_HIGH);
			
            client->httpresp.sent_size = 0;
        }break;
        case elysian_schdlr_EV_READ:
        {

        }break;
        case elysian_schdlr_EV_POLL:
        {
            //elysian_time_sleep(500);
            if(!client->httpresp.buf){
				/*
				** client->httpresp.buf is the only high priority allocation. This is going to ensure that
				** a newly accepted client that allocates the majority of the available memory will not
				** affect the servicing of already accepted connections, and they will be able to continue
				** sending the HTTP response. In different scenario a newly accepted connection could block
				** the servicing of the existing ones by not allowing them to allocate the response buffer.
				*/
				client->httpresp.buf_size = ELYSIAN_HTTP_RESPONSE_BODY_BUF_SZ_MAX;
				if(client->httpresp.buf_size > elysian_mem_available(ELYSIAN_MEM_MALLOC_PRIO_HIGH)){
					client->httpresp.buf_size = elysian_mem_available(ELYSIAN_MEM_MALLOC_PRIO_HIGH);
				}
				if(client->httpresp.buf_size < (ELYSIAN_HTTP_RESPONSE_BODY_BUF_SZ_MIN)){
					elysian_schdlr_state_poll_backoff(server);
					return;
				}
				client->httpresp.buf = elysian_mem_malloc(server, client->httpresp.buf_size, ELYSIAN_MEM_MALLOC_PRIO_HIGH);
				ELYSIAN_ASSERT(client->httpresp.buf, "");
				if(!client->httpresp.buf){
					elysian_schdlr_state_poll_backoff(server);
					return;
				}
				client->httpresp.buf_index = 0;
				client->httpresp.buf_len = 0;
            }

			packet_count = 0;
			do{
				/*
				** Defrag
				*/
				#if 1
				if((client->httpresp.buf_index * 100) / client->httpresp.buf_size >= 25){
					for(i = 0; i < client->httpresp.buf_size - client->httpresp.buf_index; i++){
						ELYSIAN_LOG("Defragging..");
						client->httpresp.buf[i] = client->httpresp.buf[client->httpresp.buf_index + i];
					}
					client->httpresp.buf_index = 0;
				}
				#endif
				/*
				** Read
				*/
				read_size = client->httpresp.headers_size + client->httpresp.body_size - client->httpresp.sent_size - client->httpresp.buf_len;
				read_size = ((read_size) > (client->httpresp.buf_size - client->httpresp.buf_len)) ? client->httpresp.buf_size - client->httpresp.buf_len : read_size;      
				//ELYSIAN_LOG("We have %u bytes, we can read %u more", client->httpresp.buf_len, read_size);
				if(read_size){
					err = elysian_resource_read(server, &client->httpresp.buf[client->httpresp.buf_index + client->httpresp.buf_len], read_size, &read_size_actual);
					switch(err){
						case ELYSIAN_ERR_OK:
							client->httpresp.buf_len += read_size_actual;
							//ELYSIAN_LOG("We just read %u bytes", read_size_actual);
							break;
						case ELYSIAN_ERR_POLL:
							elysian_schdlr_state_poll_backoff(server);
							return;
							break;
						case ELYSIAN_ERR_FATAL:
							elysian_schdlr_state_set(server, elysian_state_http_disconnect);
							return;
							break;
						default:
							ELYSIAN_ASSERT(0, "");
							elysian_schdlr_state_set(server, elysian_state_http_disconnect);
							return;
							break;
					};
				}
				
				/*
				** Send
				*/
				send_size = client->httpresp.buf_len;
				if(send_size){
					err = elysian_socket_write(&client->socket, &client->httpresp.buf[client->httpresp.buf_index], send_size, &send_size_actual);
					switch(err){
						case ELYSIAN_ERR_OK:
							//ELYSIAN_LOG("We just send %u bytes", send_size_actual);
							elysian_schdlr_state_timeout_reset(server);
							client->httpresp.buf_index += send_size_actual;
							client->httpresp.buf_len -= send_size_actual;
							client->httpresp.sent_size += send_size_actual;
							packet_count++;
							break;
						case ELYSIAN_ERR_POLL:
							elysian_schdlr_state_poll_backoff(server); 
							return;
							break;
						default:
							elysian_schdlr_state_set(server, elysian_state_http_disconnect);
							return;
							break;
					};

				}
				
				//ELYSIAN_LOG("Client %u complete ratio is %u [%u bytes left]", client->id, (client->httpresp.sent_size * 100)/(client->httpresp.headers_size + client->httpresp.body_size), client->httpresp.headers_size + client->httpresp.body_size - client->httpresp.sent_size);
				
				/*
				** Check if we are done
				*/
				if(client->httpresp.headers_size + client->httpresp.body_size == client->httpresp.sent_size){
					ELYSIAN_LOG("Client %u: The whole response was sent!!!", client->id);
					if(client->httpresp.keep_alive && client->http_pipelining_enabled){
						elysian_schdlr_state_set(server, elysian_state_http_keepalive);
						return;
					}else{
						elysian_schdlr_state_set(server, elysian_state_http_disconnect);
						return;
					}
				}
            }while(packet_count < 8);
			
            /*
            ** Check if we can release the response buffer so it can be used by other clients
            */
            if(client->httpresp.buf_len == 0){
                elysian_mem_free(server, client->httpresp.buf);
                client->httpresp.buf = NULL;
            }
            
            elysian_schdlr_state_poll_enable(server);
        }break;
        case elysian_schdlr_EV_ABORT:
        {
			elysian_schdlr_state_set(server, elysian_state_http_disconnect);
			return;
        }break;
        default:
        {
            ELYSIAN_ASSERT(0, "");
        }break;
    };
}

void elysian_state_http_keepalive(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {           
            elysian_schdlr_state_timeout_set(server, 10000);
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
			elysian_schdlr_state_timeout_reset(server);
			
			err = elysian_client_cleanup(server);
			if (err != ELYSIAN_ERR_OK) {
				ELYSIAN_LOG("Cleanup failed!");
				return;
			}
			
            /*
            ** This cb should be called after client_cleanup() to make sure
            ** that the web server has closed any file handles and so any files 
            ** created from within controllre can be now safely removed by the application.
            */
            if(client->reqserved_cb){
                client->reqserved_cb(server, client->reqserved_cb_data);
            }

            elysian_schdlr_state_set(server, elysian_state_http_request_headers_receive);
			return;
        }break;
        case elysian_schdlr_EV_ABORT:
        {
			elysian_schdlr_state_set(server, elysian_state_http_disconnect);
			return;
        }break;
        default:
        {
            ELYSIAN_ASSERT(0, "");
        }break;
    };
}

void elysian_state_http_disconnect(elysian_t* server, elysian_schdlr_ev_t ev){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	ELYSIAN_LOG("[event = %s, client %u]", elysian_schdlr_ev_name[ev], client->id);
	
    switch(ev){
        case elysian_schdlr_EV_ENTRY:
        {
            elysian_schdlr_state_timeout_set(server, 10000);
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
			elysian_schdlr_state_timeout_reset(server);
			
			err = elysian_client_cleanup(server);
			if (err != ELYSIAN_ERR_OK) {
				ELYSIAN_LOG("Cleanup failed!");
				return;
			}
			
            /*
            ** This cb should be called after client_cleanup() to make sure
            ** that the web server has closed any file handles and so any files 
            ** created from within controllre can be now safely removed by the application.
            */
            if(client->reqserved_cb){
                client->reqserved_cb(server, client->reqserved_cb_data);
            }
            
            if(client->rcv_cbuf_list){
				elysian_cbuf_list_free(server, client->rcv_cbuf_list );
				client->rcv_cbuf_list  = NULL;
			}
			
			if(client->store_cbuf_list){
				elysian_cbuf_list_free(server, client->store_cbuf_list );
				client->rcv_cbuf_list  = NULL;
			}
			
            elysian_schdlr_state_set(server, NULL);
            return;
        }break;
        case elysian_schdlr_EV_ABORT:
        {
			/*
			** Ignore any abort signals, we should cleanup before exiting
			*/
        }break;
        default:
        {
            ELYSIAN_ASSERT(0, "");
        }break;
    };
}

elysian_err_t elysian_client_cleanup(elysian_t* server){
    elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err = ELYSIAN_ERR_OK;
	
    /*
    ** Clear mvc
    */
    elysian_mvc_clear(server);

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
    
	/*
    ** Release redirection URL
    */
	if(client->httpresp.redirection_url){
		elysian_mem_free(server, client->httpresp.redirection_url);
		client->httpresp.redirection_url = NULL;
	}
	 
    if(client->httpreq.url){
        elysian_mem_free(server, client->httpreq.url);
        client->httpreq.url = NULL;
    }  

	if(client->httpreq.multipart_boundary){
		elysian_mem_free(server, client->httpreq.multipart_boundary);
		client->httpreq.multipart_boundary = NULL;
	}
	
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
    
    if(!(server = elysian_mem_malloc(NULL, sizeof(elysian_t), ELYSIAN_MEM_MALLOC_PRIO_NORMAL))){
        return NULL;
    }
    
    server->controllers = NULL;
	server->rom_fs = NULL;
	server->rom_fs_size = 0;
	
    return server;
}

elysian_err_t elysian_rom_fs(elysian_t* server, const elysian_file_rom_t rom_fs[], uint32_t rom_fs_size){
	server->rom_fs = rom_fs;
	server->rom_fs_size = rom_fs_size;
	return ELYSIAN_ERR_OK;
}
			   
			   
elysian_err_t elysian_start(elysian_t* server, uint16_t port, elysian_authentication_cb_t authentication_cb){
    elysian_err_t err;
	
#if defined(ELYSIAN_OS_ENV_UNIX)
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) { 
		ELYSIAN_LOG("Could not ignore the SIGPIPE signal!")
	}
#endif

	server->listening_port = port;
	server->authentication_cb = authentication_cb;
    err = elysian_schdlr_init(server, port, elysian_state_http_connection_accepted);
    
    
    return err;
}

void elysian_stop(elysian_t* server){
	elysian_schdlr_stop(server);
}

elysian_err_t elysian_poll(elysian_t* server, uint32_t intervalms){
    elysian_schdlr_poll(server, intervalms);
    return ELYSIAN_ERR_OK;
}

elysian_client_t* elysian_current_client(elysian_t* server) {
	return elysian_schdlr_current_client_get(server);
}
