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

elysian_err_t elysian_mvc_read_req_params(elysian_t* server);

void elysian_mvc_init(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    client->mvc.view = NULL;
    client->mvc.attributes = NULL;
	client->mvc.allocs = NULL;
	client->mvc.req_params = NULL;
}

uint8_t elysian_mvc_isconfigured(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	if((client->mvc.attributes) || (client->mvc.view) || (client->mvc.allocs)){
		return 1;
	}else{
		return 0;
	}
}

elysian_err_t elysian_mvc_clear(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_mvc_attribute_t* attribute_next;
    elysian_mvc_alloc_t* alloc_next;
	elysian_req_param_t* req_param_next;
	
    if(client->mvc.view){
        elysian_mem_free(server, client->mvc.view);
        client->mvc.view = NULL;
    }
    
    while(client->mvc.attributes){
        attribute_next = client->mvc.attributes->next;
        if(client->mvc.attributes->name){
            elysian_mem_free(server, client->mvc.attributes->name);
        }
        if(client->mvc.attributes->value){
            elysian_mem_free(server, client->mvc.attributes->value);
        }
        elysian_mem_free(server, client->mvc.attributes);
        client->mvc.attributes = attribute_next;
    };

	while(client->mvc.allocs){
        alloc_next = client->mvc.allocs->next;
		elysian_mem_free(server, client->mvc.allocs->data);
        elysian_mem_free(server, client->mvc.allocs);
        client->mvc.allocs = alloc_next;
    };
	
	while(client->mvc.req_params){
		req_param_next = client->mvc.req_params->next;
		if(client->mvc.req_params->name){
			elysian_mem_free(server, client->mvc.req_params->name);
		}
		if(client->mvc.req_params->filename){
			elysian_mem_free(server, client->mvc.req_params->filename);
		}
		elysian_mem_free(server, client->mvc.req_params);
		client->mvc.req_params = req_param_next;
	};
	
    return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_mvc_configure(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
	elysian_mvc_alloc_t* alloc_next;
    elysian_mvc_controller_t* controller;
    elysian_req_param_t* req_param_next;
	
	ELYSIAN_ASSERT(client->mvc.view == NULL, "");
	ELYSIAN_ASSERT(client->mvc.attributes == NULL, "");
		
	controller = elysian_mvc_controller_get(server, client->httpreq.url, client->httpreq.method);
	if(controller){
		ELYSIAN_LOG("Calling user defined controller..");

		/*
		** Try to get request params
		*/
		err = elysian_mvc_read_req_params(server);
		if(err != ELYSIAN_ERR_OK){
			elysian_mvc_clear(server);
			return err;
		}
		
		err = controller->cb(server);
		//ELYSIAN_ASSERT(err == ELYSIAN_ERR_OK || err == ELYSIAN_ERR_POLL || err == ELYSIAN_ERR_FATAL, "");
		if((err != ELYSIAN_ERR_OK) && (err != ELYSIAN_ERR_POLL) && (err != ELYSIAN_ERR_FATAL)) {
			/*
			** Any none permitted errors are converted to ELYSIAN_ERR_FATAL (etc ELYSIAN_ERR_NOTFOUND)
			*/
			err = ELYSIAN_ERR_FATAL;
		}
		
		if(err != ELYSIAN_ERR_OK){
			elysian_mvc_clear(server);
			return err;
		}
		
		/*
		** Remove request params
		*/
		while(client->mvc.req_params){
			req_param_next = client->mvc.req_params->next;
			if(client->mvc.req_params->name){
				elysian_mem_free(server, client->mvc.req_params->name);
			}
			if(client->mvc.req_params->filename){
				elysian_mem_free(server, client->mvc.req_params->filename);
			}
			elysian_mem_free(server, client->mvc.req_params);
			client->mvc.req_params = req_param_next;
		};
		
		/*
		** Remove any hidden user allocations
		*/
		while(client->mvc.allocs){
			alloc_next = client->mvc.allocs->next;
			elysian_mem_free(server, client->mvc.allocs->data);
			elysian_mem_free(server, client->mvc.allocs);
			client->mvc.allocs = alloc_next;
		};
		
		if(client->mvc.view){
			/*
			** User set a new view during controller cb execution
			*/
			if(strcmp(client->httpreq.url, client->mvc.view) == 0){
				/*
				** User controller set the same view as the one requested by the HTTP client
				*/
				elysian_mem_free(server, client->mvc.view);
				client->mvc.view = NULL;
			}else{
				/*
				** User controller set a view different than the one requested by the HTTP client
				*/
				elysian_mem_free(server, client->httpreq.url);
				client->httpreq.url = client->mvc.view;
				client->mvc.view = NULL;
			}
		}else{
			/*
			** No view set from user controller, use the one requested by the HTTP client
			*/
		}
	}else{
		/*
		** No controller has been assigned for the specific URL
		*/
	}

	ELYSIAN_ASSERT(client->mvc.view == NULL, "");
	ELYSIAN_ASSERT(client->httpreq.url != NULL, "");
	
	client->mvc.view = client->httpreq.url;
	client->httpreq.url = NULL;

    return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_mvc_set_view(elysian_t* server, char* view){
    elysian_client_t* client = elysian_schdlr_current_client_get(server);
    if(client->mvc.view){
        elysian_mem_free(server, client->mvc.view);
        client->mvc.view = NULL;
    }
    
	if(view == NULL){
		/* Special case for an empty-bodied file */
		view = ELYSIAN_FS_EMPTY_FILE_VRT_ROOT ELYSIAN_FS_EMPTY_FILE_NAME;
	}
	
    if(strcmp(view, "/") == 0){
		/* Special case for index.html page */
        view = ELYSIAN_FS_INDEX_HTML_VRT_ROOT"/index.html";
    }
    
    client->mvc.view = elysian_mem_malloc(server, strlen(view) + 1, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
    if(!client->mvc.view){
        return ELYSIAN_ERR_POLL;
    }
    
    strcpy(client->mvc.view, view);
    
    ELYSIAN_LOG("MVC view set to '%s'", client->mvc.view);
    
    return ELYSIAN_ERR_OK;
}

/* --------------------------------------------------------------------------------------------------------------------------------
| Redirection
-------------------------------------------------------------------------------------------------------------------------------- */
elysian_err_t elysian_mvc_redirect(elysian_t* server, char* redirection_url){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
    char hostname[64];
	
	if(client->httpresp.redirection_url){
		elysian_mem_free(server, client->httpresp.redirection_url);
		client->httpresp.redirection_url = NULL;
	}
	
	if(client->mvc.view){
        elysian_mem_free(server, client->mvc.view);
        client->mvc.view = NULL;
    }
	
	char status_code_page_name[32];

	sprintf(status_code_page_name, ELYSIAN_FS_WS_VRT_ROOT"/%u.html", elysian_http_get_status_code_num(ELYSIAN_HTTP_STATUS_CODE_302));
	err = elysian_mvc_set_view(server, status_code_page_name);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	
	err = elysian_os_hostname_get(hostname);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	
	/*
	** http://www.example.org/index.asp
	*/
	client->httpresp.redirection_url = elysian_mem_malloc(server, strlen(redirection_url) + strlen(hostname) + 16 /* http:// */ + 1, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
	if(!client->httpresp.redirection_url){
		 return ELYSIAN_ERR_POLL;
	}
	
	sprintf(client->httpresp.redirection_url, "http://%s:%u%s", hostname, server->listening_port, redirection_url);

	elysian_set_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_302);
	
    ELYSIAN_LOG("Redirection URL set to '%s'", client->httpresp.redirection_url);
    
    return ELYSIAN_ERR_OK;
}

/* --------------------------------------------------------------------------------------------------------------------------------
| Controllers
-------------------------------------------------------------------------------------------------------------------------------- */
elysian_err_t elysian_mvc_controller_add(elysian_t* server, const char* url, elysian_mvc_controller_cb_t cb, uint8_t http_methods_mask){
    elysian_mvc_controller_t* controller;
    
    ELYSIAN_ASSERT(server != NULL, "");
    ELYSIAN_ASSERT(url != NULL, "");
    ELYSIAN_ASSERT(cb != NULL, "");
    
    controller = elysian_mem_malloc(server, sizeof(elysian_mvc_controller_t), ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
    if(!controller){
        return ELYSIAN_ERR_POLL;
    }
    
    controller->url = url;
    controller->cb = cb;
    controller->http_methods_mask = http_methods_mask;
    controller->next = server->controllers;
    server->controllers = controller;
    
    return ELYSIAN_ERR_OK;
}

elysian_mvc_controller_t* elysian_mvc_controller_get(elysian_t* server, char* url, elysian_http_method_e method_id){
    elysian_mvc_controller_t* controller = server->controllers;
    ELYSIAN_LOG("Searching user defined controller for url '%s' and method '%s'", url, elysian_http_get_method_name(method_id));
    while(controller){
        if((strcmp(controller->url, url) == 0) && (controller->http_methods_mask & method_id)){
            return controller;
        }
        controller = controller->next;
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------------------------------------------
| Attributes
-------------------------------------------------------------------------------------------------------------------------------- */
elysian_err_t elysian_mvc_attribute_set(elysian_t* server, char* name, char* value){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_mvc_attribute_t* attribute;
    
    ELYSIAN_ASSERT(client != NULL, "");
    ELYSIAN_ASSERT(name != NULL, "");
    ELYSIAN_ASSERT(strlen(name) > 0, "");
    
    attribute = elysian_mem_malloc(server, sizeof(elysian_mvc_attribute_t), ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
    if(!attribute){
        return ELYSIAN_ERR_POLL;
    }
    
    attribute->name = elysian_mem_malloc(server, strlen(name) + 1, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
    if(!attribute->name){
        elysian_mem_free(server, attribute);
        return ELYSIAN_ERR_POLL;
    }
    
    strcpy(attribute->name, name);
    
    attribute->value = elysian_mem_malloc(server, strlen(value) + 1, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
    if(!attribute->value){
        elysian_mem_free(server, attribute->name);
        elysian_mem_free(server, attribute);
        return ELYSIAN_ERR_POLL;
    }
    
    strcpy(attribute->value, value);
    
	ELYSIAN_LOG("ATTRIBUTE VALUE SET TO <%s>\r\n", attribute->value);
	
    attribute->next = client->mvc.attributes;
    client->mvc.attributes = attribute;
    
    return ELYSIAN_ERR_OK;
}

elysian_mvc_attribute_t* elysian_mvc_attribute_get(elysian_t* server, char* name){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_mvc_attribute_t* attribute;
    
    ELYSIAN_LOG("Searching user defined attribute with name '%s'", name);
    
    attribute = client->mvc.attributes;
    while(attribute){
        ELYSIAN_LOG("Attribute is '%s'!?", attribute->name)
        if(strcmp(attribute->name, name) == 0){
            ELYSIAN_LOG("Attribute found!");
            return attribute;
        }
        attribute = attribute->next;
    }
    
    return NULL;
}

/* --------------------------------------------------------------------------------------------------------------------------------
| Allocations
-------------------------------------------------------------------------------------------------------------------------------- */
elysian_err_t elysian_mvc_add_alloc(elysian_t* server, void* data){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_mvc_alloc_t* alloc;
    
    ELYSIAN_ASSERT(client != NULL, "");
    ELYSIAN_ASSERT(data != NULL, "");

	alloc = elysian_mem_malloc(server, sizeof(elysian_mvc_alloc_t), ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
    if(!alloc){
        return ELYSIAN_ERR_POLL;
    }
	
    alloc->data = data;
    alloc->next = client->mvc.allocs;
    client->mvc.allocs = alloc;
    
    return ELYSIAN_ERR_OK;
}

/* --------------------------------------------------------------------------------------------------------------------------------
| Callbacks
-------------------------------------------------------------------------------------------------------------------------------- */
elysian_err_t elysian_mvc_set_reqserved_cb(elysian_t* server, elysian_reqserved_cb_t cb, void* data){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    client->reqserved_cb = cb;
    client->reqserved_cb_data = data;
    return ELYSIAN_ERR_OK;
}

/* --------------------------------------------------------------------------------------------------------------------------------
| Parameters
-------------------------------------------------------------------------------------------------------------------------------- */
elysian_err_t elysian_mvc_get_param(elysian_t* server, char* param_name, elysian_req_param_t* req_param){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_req_param_t* req_param_next;

    req_param->client = client;
    
	req_param_next = client->mvc.req_params;
	while(req_param_next){
		if (strcmp(param_name, req_param_next->name) == 0) {
			*req_param = *req_param_next;
			ELYSIAN_LOG("param '%s' found!", param_name);
			return ELYSIAN_ERR_OK;
		}
		req_param_next = req_param_next->next;
	};
	
	/*
	** Parameter not found
	*/
	req_param->client = NULL;
	req_param->file = NULL;
	req_param->data_index = ELYSIAN_INDEX_OOB32;
	req_param->data_len = 0;
	req_param->data_index_cur = 0;
	
	/*
	** Don't return an error here, let the app layer decide if 
	** the missing param is an error or not.
	*/
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_mvc_read_param(elysian_t* server, elysian_req_param_t* req_param, uint8_t* buf, uint32_t buf_size, uint32_t* read_size){
    uint32_t current_offset;
    elysian_err_t err;
     
    ELYSIAN_ASSERT(req_param, "");
    ELYSIAN_ASSERT(req_param->client, "");
    ELYSIAN_ASSERT(req_param->file, "");
    ELYSIAN_ASSERT(buf, "");
    
    *read_size = 0;
    
    if(!req_param->file){
        return ELYSIAN_ERR_FATAL;
    }

	/*
	** This could happen in POST request, where the last param index 
	** could be equal to filesize, if for example the last param is empty.
	** 'param1=test+data+%231%21&param2=test+data+%232%21&param3='
	** We are not to seek to a file with size 0..
	*/
	if (req_param->data_len == 0) {
		return ELYSIAN_ERR_OK;
	}
	
    buf_size = buf_size > req_param->data_len - (req_param->data_index_cur - req_param->data_index) ? req_param->data_len - (req_param->data_index_cur - req_param->data_index) : buf_size;
    
    err = elysian_fs_ftell(server, req_param->file, &current_offset);
    if(err != ELYSIAN_ERR_OK){
        return err;
    }
	
	ELYSIAN_LOG("current_offset is %u", current_offset);
	ELYSIAN_LOG("req_param->indexis %u", req_param->data_index_cur);
	
	
	if(current_offset != req_param->data_index_cur){
		err = elysian_fs_fseek(server, req_param->file, req_param->data_index_cur);
        if(err != ELYSIAN_ERR_OK){
            return err;
        }
	}
    err = elysian_fs_fread(server, req_param->file, buf, buf_size, read_size);
	if(err != ELYSIAN_ERR_OK){
        return err;
    }
    req_param->data_index_cur += *read_size;
	
    return err;
}

elysian_err_t elysian_mvc_get_param_bytes(elysian_t* server, char* param_name, uint8_t** param_value, uint8_t* param_found) {
	//elysian_client_t* client = elysian_schdlr_current_client_get(server);
	uint32_t actual_read_size;
	elysian_req_param_t param;
	uint8_t* buf;
	elysian_err_t err;
	
	*param_value = NULL;
	*param_found = 0;
	err = elysian_mvc_get_param(server, param_name, &param);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	if(param.data_index == ELYSIAN_INDEX_OOB32){
		return ELYSIAN_ERR_OK;
	}else{
		*param_found = 1;
	}
	
	ELYSIAN_LOG("Parameters len is %u", param.data_len);
	
	buf = elysian_mem_malloc(server, param.data_len, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
	if(buf == NULL){
		return ELYSIAN_ERR_POLL;
	}
	
	err = elysian_mvc_read_param(server, &param, buf, param.data_len, &actual_read_size);
	if(err != ELYSIAN_ERR_OK){
		elysian_mem_free(server, buf);
		return err;
	}
	
	ELYSIAN_ASSERT(param.data_len == actual_read_size , "");
	
	err = elysian_mvc_add_alloc(server, buf);
	if(err != ELYSIAN_ERR_OK){
		elysian_mem_free(server, buf);
		return err;
	}
	
	*param_value = buf;
	
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_mvc_get_param_str(elysian_t* server, char* param_name, char** param_value, uint8_t* param_found){
	//elysian_client_t* client = elysian_schdlr_current_client_get(server);
	uint32_t actual_read_size;
	elysian_req_param_t param;
	uint8_t* buf;
	elysian_err_t err;
	

	*param_value = "";
	*param_found = 0;
	err = elysian_mvc_get_param(server, param_name, &param);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	if(param.data_index == ELYSIAN_INDEX_OOB32){
		return ELYSIAN_ERR_OK;
	}else{
		*param_found = 1;
	}
	
	ELYSIAN_LOG("Parameters len is %u", param.data_len);
	
	buf = elysian_mem_malloc(server, param.data_len + 1, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
	if(buf == NULL){
		return ELYSIAN_ERR_POLL;
	}
	
	err = elysian_mvc_read_param(server, &param, buf, param.data_len, &actual_read_size);
	if(err != ELYSIAN_ERR_OK){
		elysian_mem_free(server, buf);
		return err;
	}
	
	ELYSIAN_ASSERT(param.data_len == actual_read_size , "");
	
	buf[actual_read_size] = '\0';
	
	err = elysian_mvc_add_alloc(server, buf);
	if(err != ELYSIAN_ERR_OK){
		elysian_mem_free(server, buf);
		return err;
	}
	
	elysian_http_decode((char*) buf);
	
	*param_value = (char*) buf;
	
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_mvc_get_param_uint(elysian_t* server, char* param_name, uint32_t* param_value, uint8_t* param_found){
	elysian_err_t err;
	char* param_value_str;
	
	*param_value = 0;
	err = elysian_mvc_get_param_str(server, param_name, &param_value_str, param_found);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	
	*param_value = atoi(param_value_str);
	
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_mvc_get_param_int(elysian_t* server, char* param_name, int32_t* param_value, uint8_t* param_found){
	elysian_err_t err;
	char* param_value_str;
	
	*param_value = 0;
	err = elysian_mvc_get_param_str(server, param_name, &param_value_str, param_found);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	
	*param_value = atoi(param_value_str);
	
	return ELYSIAN_ERR_OK;
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
elysian_err_t elysian_mvc_read_req_params(elysian_t* server){
	elysian_err_t err;
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_req_param_t* param;
	elysian_req_param_t* param_prev;
	elysian_req_param_t* req_param_next;
	char* param_header;
	char param_search_pattern[2 /* dashes */ + 70 /* rfc boundary len */ + 2 /* \r\n */ + 1];
	uint32_t param_search_index;
	
	//uint32_t boundary_len;
	uint32_t index0;
	uint32_t index1;

	elysian_file_t* param_file;
	uint32_t param_header_index;
	uint32_t param_header_len;
	//uint32_t param_prev_body_index;
	
	uint32_t max_index;
	
	ELYSIAN_LOG("***************************************************************************************");
	
	char* div1;
	char* div2;
	char* div3;
	uint8_t stop;
		
	param_header = NULL;
	param_prev = NULL;
	
	/*
	** MULTIPART: 
	** START: 		index0=indexof("--BOUNDARY\r\n") + strlen("--BOUNDARY\r\n"), on body file
	** CONTINUE:	[HEADER][divider1="\r\n\r\n"][DATA][divider2="\r\n"][divider3="--BOUNDARY\r\n"]
	** STOP: 		index1=indexof(divider3), divider3="--BOUNDARY--"
	**
	** GET: 
	** START: 		index0 = indexof('?') + strlen('?'), on header file
	** CONTINUE:	[HEADER][divider1="="][DATA][divider2=""][divider3="&" or max_index]
	** STOP: 		index1=indexof(divider3), divider3=" "
	**
	** POST: 
	** START: 		index0 = 0, on body file
	** CONTINUE:	[HEADER][divider1="="][DATA][divider2=""][divider3="&" or max_index]
	** STOP: 		index1=filesize
	**
	*/
	if(client->httpreq.content_type == ELYSIAN_HTTP_CONTENT_TYPE_MULTIPART__FORM_DATA){
		ELYSIAN_LOG("Parameters located in HTTP body");
		param_file = &client->httpreq.body_file;
		err = elysian_fs_fsize(server, param_file, &max_index);
		if(err != ELYSIAN_ERR_OK){
			ELYSIAN_LOG("ERROR_0");
			goto handle_error;
		}
		if(max_index == 0) {
			/*
			** no parts exist
			*/
			ELYSIAN_LOG("NO_PARTS");
			err = ELYSIAN_ERR_OK;
			goto handle_error;
		} else {
			max_index--;
			sprintf(param_search_pattern, "--%s\r\n", client->httpreq.multipart_boundary);
			err = elysian_strstr_file(server, param_file, 0, param_search_pattern, "", 0, &index0, &index1);
			if(err != ELYSIAN_ERR_OK){
				ELYSIAN_LOG("ERROR_1");
				goto handle_error;
			}
			if (index0 == ELYSIAN_INDEX_OOB32) {
				/*
				** Not found, no parts exist
				*/
				ELYSIAN_LOG("NO_PARTS");
				err = ELYSIAN_ERR_OK;
				goto handle_error;
			} else {
				div1 = "\r\n\r\n";
				div2 = "\r\n";
				div3 = param_search_pattern;
				param_search_index = index0 + strlen(param_search_pattern);
			}
		}
	}else {
		if(client->httpreq.content_type == ELYSIAN_HTTP_CONTENT_TYPE_APPLICATION__X_WWW_FORM_URLENCODED){
			ELYSIAN_LOG("Parameters located in HTTP body");
			param_file = &client->httpreq.body_file;
			err = elysian_fs_fsize(server, param_file, &max_index);
			err = elysian_fs_fsize(server, param_file, &max_index);
			if(err != ELYSIAN_ERR_OK){
				ELYSIAN_LOG("ERROR_0");
				goto handle_error;
			}
			
			if(max_index == 0) {
				/*
				** no parts exist
				*/
				ELYSIAN_LOG("NO_PARTS");
				err = ELYSIAN_ERR_OK;
                goto handle_error;
			} else {
				max_index--;
				div1 = "=";
				div2 = "";
				div3 = "&";
				param_search_index = 0;
			}
		} else {
			ELYSIAN_LOG("Parameters located in HTTP header");
			param_file = &client->httpreq.headers_file;
            err = elysian_strstr_file(server, param_file, 0, " HTTP/", "", 0, &index0, &index1);
            if(err != ELYSIAN_ERR_OK){
				ELYSIAN_LOG("ERROR_0");
                goto handle_error;
            }
            if(index0 == ELYSIAN_INDEX_OOB32){
				ELYSIAN_LOG("ERROR_1");
				err = ELYSIAN_ERR_FATAL;
                goto handle_error;
            } else {
				max_index = index0;
				if(max_index == 0) {
					/*
					** no parts exist
					*/
					ELYSIAN_LOG("NO_PARTS");
					err = ELYSIAN_ERR_OK;
					goto handle_error;
				} else {
					max_index--;
					err = elysian_strstr_file(server, param_file, 0, "?", "", 0, &index0, &index1);
					if(err != ELYSIAN_ERR_OK){
						ELYSIAN_LOG("ERROR_2");
						goto handle_error;
					}
					if((index0 == ELYSIAN_INDEX_OOB32) || (index0 > max_index)){
						/*
						** Not found, no parts exist
						*/
						ELYSIAN_LOG("NO_PARTS");
						err = ELYSIAN_ERR_OK;
						goto handle_error;
					} else {
						div1 = "=";
						div2 = "";
						div3 = "&";
						param_search_index = index0 + 1;
					}
				}
			}
		}
	}
	
	ELYSIAN_LOG("Searching params from index [%u]..", param_search_index);
	stop = 0;
	while (!stop) {
		if (!param_prev) {
			/*
			** No params yet detected
			*/
		} else {
			/*
			** At least one param detected
			*/
			ELYSIAN_LOG("param_search_index = %u + %u + %u + %u = %u", (unsigned int) param_prev->data_index, (unsigned int) param_prev->data_len, (unsigned int) strlen(div2), (unsigned int)strlen(div3), (unsigned int)(param_prev->data_index + param_prev->data_len + strlen(div2) + strlen(div3)));
			param_search_index = param_prev->data_index + param_prev->data_len + strlen(div2) + strlen(div3);
		}
		ELYSIAN_LOG("Searching '%s'->'%s' from index %u", div1, div3, param_search_index);
		err = elysian_strstr_file(server, param_file, param_search_index, div1, div3, 0, &index0, &index1);
		if(err != ELYSIAN_ERR_OK){
			ELYSIAN_LOG("ERROR_0");
			goto handle_error;
		}
		if(index0 > max_index) {
			index0 = ELYSIAN_INDEX_OOB32;
		}
		if(index1 > max_index) {
			index1 = ELYSIAN_INDEX_OOB32;
		}
		if(index0 == ELYSIAN_INDEX_OOB32){
			/*
			** no more params
			*/
			ELYSIAN_LOG("ERROR_1");
			err = ELYSIAN_ERR_OK;
			goto handle_error;
		}else{
			if(index1 == ELYSIAN_INDEX_OOB32){
				/*
				** This is the last param
				*/
				ELYSIAN_LOG("MAYBE_LAST_1");
				if(client->httpreq.content_type == ELYSIAN_HTTP_CONTENT_TYPE_MULTIPART__FORM_DATA){
					sprintf(param_search_pattern, "--%s--", client->httpreq.multipart_boundary);
					ELYSIAN_LOG("Searching '%s'->'%s' from index %u", div1, param_search_pattern, param_search_index);
					err = elysian_strstr_file(server, param_file, param_search_index, div1, param_search_pattern, 0, &index0, &index1);
				} else {
					if(client->httpreq.content_type == ELYSIAN_HTTP_CONTENT_TYPE_APPLICATION__X_WWW_FORM_URLENCODED){
						err = elysian_fs_fsize(server, param_file, &index1);
					} else {
						err = elysian_strstr_file(server, param_file, param_search_index, div1, " ", 0, &index0, &index1);
					}
				}
				if(err != ELYSIAN_ERR_OK){
					ELYSIAN_LOG("ERROR_2");
					goto handle_error;
				}
				ELYSIAN_LOG("index0 = %u, index1 = %u, max_index = %u", index0, index1, max_index);
				if(index0 > max_index + 1) {
					index0 = ELYSIAN_INDEX_OOB32;
				}
				if(index1 > max_index + 1) {
					index1 = ELYSIAN_INDEX_OOB32;
				}
				if ((index0 == ELYSIAN_INDEX_OOB32) || (index1 == ELYSIAN_INDEX_OOB32)){
					err = ELYSIAN_ERR_FATAL;
					ELYSIAN_LOG("ERROR_3");
					goto handle_error;
				} else {
					/*
					** [DATA][divider2][divider3]:[HEADER][divider1][DATA][divider2][terminal]
					** param_name=param_value
					** index0 pointes to divider1
					** index1 pointes to terminal
					*/
					stop = 1;
				}
			} else {
				/*
				** New param found
				** [DATA][divider2][divider3]:[HEADER][divider1][DATA][divider2][divider3]
				** param_name=param_value&
				** param_search_index points to header
				** index0 pointes to divider1
				** index1 pointes to divider3
				** param_search_index points to HEADER
				*/
			}
		}
		
		ELYSIAN_LOG("index0 = %u, index1 = %u", index0, index1);
		/*
		** Proces new param
		*/
		param = elysian_mem_malloc(server, sizeof(elysian_req_param_t), ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
		if (!param) {
			err = ELYSIAN_ERR_POLL;
			goto handle_error;
		}
		param->next = client->mvc.req_params;
		client->mvc.req_params = param;
		param->client = client;
		param->file = param_file;
		param->name = NULL;
		param->filename = NULL;
		param->data_index = index0 + strlen(div1);
		param->data_len = (index1 - strlen(div2)) - (param->data_index);
		
		//param->index0 = param->data_index;
		//param->len = param->data_len;
		param->data_index_cur = param->data_index;
		
		ELYSIAN_LOG("param->body_index = %u, param->body_len = %u", param->data_index, param->data_len);
		
		if (param_prev) {
			param_header_index = param_prev->data_index + param_prev->data_len + strlen(div2) + strlen(div3);
		} else {
			param_header_index = param_search_index;
		}
		param_header_len = (index0) - (param_header_index);
		
		/*
		** Read the header block 
		*/
		param_header = elysian_mem_malloc(server, param_header_len + 1, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
		if(!param_header){
			err = ELYSIAN_ERR_POLL;
			goto handle_error;
		}
		err = elysian_strncpy_file(server, param_file, param_header_index, param_header, param_header_len);
		if(err != ELYSIAN_ERR_OK){
			goto handle_error;
		}

		/*
		** Retrieve any usefull info
		*/
		ELYSIAN_LOG("This is part header: %s", param_header);
		
		/* get the name */
		if(client->httpreq.content_type == ELYSIAN_HTTP_CONTENT_TYPE_MULTIPART__FORM_DATA){
			int i = 0;
			char* name = strstr(param_header, "name=\"");
			if(name){
				name += strlen("name=\"");
				while(name[i] !='"'){
					i++;
				}
				param->name = elysian_mem_malloc(server, i + 1, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
				if(!param->name){
					err = ELYSIAN_ERR_POLL;
					goto handle_error;
				}
				memcpy(param->name, name, i);
				param->name[i] = '\0';
				ELYSIAN_LOG("This is part NAME: %s", param->name);
			}
		} else {
			param->name = elysian_mem_malloc(server, strlen(param_header) + 1, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
			if(!param->name){
				err = ELYSIAN_ERR_POLL;
				goto handle_error;
			}
			strcpy(param->name, param_header);
			ELYSIAN_LOG("This is part NAME: %s", param->name);
		}
		
		elysian_mem_free(server, param_header);
		param_header = NULL;

		param_prev = param;
	}; // while(1)

handle_error:
	
	if(err != ELYSIAN_ERR_OK){
		if(param_header) {
			elysian_mem_free(server, param_header);
		}
		while(client->mvc.req_params){
			req_param_next = client->mvc.req_params->next;
			if(client->mvc.req_params->name){
				elysian_mem_free(server, client->mvc.req_params->name);
			}
			if(client->mvc.req_params->filename){
				elysian_mem_free(server, client->mvc.req_params->filename);
			}
			elysian_mem_free(server, client->mvc.req_params);
			client->mvc.req_params = req_param_next;
		};
	}
	
	return err;
}

