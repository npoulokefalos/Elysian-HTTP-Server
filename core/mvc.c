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
elysian_err_t elysian_mvc_add_alloc(elysian_t* server, void* data);

void elysian_mvc_init(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    client->mvc.view = NULL;
    client->mvc.attributes = NULL;
	client->mvc.allocs = NULL;
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
	
    return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_mvc_configure(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
	elysian_mvc_alloc_t* alloc_next;
    elysian_mvc_controller_t* controller;
	elysian_req_param_t* param_next;
	
	ELYSIAN_ASSERT(client->mvc.view == NULL, "");
	ELYSIAN_ASSERT(client->mvc.attributes == NULL, "");
		
	controller = elysian_mvc_controller_get(server, client->httpreq.url, client->httpreq.method);
	if(controller){
		ELYSIAN_LOG("Calling user defined controller..");

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

elysian_err_t elysian_mvc_view_set(elysian_t* server, char* view){
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


elysian_err_t elysian_mvc_httpreq_url_get(elysian_t* server, char** url) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	*url = client->httpreq.url;
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_mvc_httpreq_header_get(elysian_t* server, char* header_name, char** header_value) {
	elysian_err_t err;
	
	err = elysian_http_request_get_header(server, header_name, header_value);
	if (err != ELYSIAN_ERR_OK) {
		return err;
	}
	
	if (*header_value) {
		elysian_mvc_add_alloc(server, *header_value);
	}
	
	return err;
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

	elysian_sprintf(status_code_page_name, ELYSIAN_FS_WS_VRT_ROOT"/%u.html", elysian_http_get_status_code_num(ELYSIAN_HTTP_STATUS_CODE_302));
	err = elysian_mvc_view_set(server, status_code_page_name);
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
	
	elysian_sprintf(client->httpresp.redirection_url, "http://%s:%u%s", hostname, server->listening_port, redirection_url);

	elysian_set_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_302);
	
    ELYSIAN_LOG("Redirection URL set to '%s'", client->httpresp.redirection_url);
    
    return ELYSIAN_ERR_OK;
}

/* --------------------------------------------------------------------------------------------------------------------------------
| Controllers
-------------------------------------------------------------------------------------------------------------------------------- */
elysian_err_t elysian_mvc_controller_add(elysian_t* server, const char* url, elysian_mvc_controller_cb_t cb, elysian_mvc_controller_flag_e flags){
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
	controller->flags = flags;
    controller->next = server->controllers;
    server->controllers = controller;
    
    return ELYSIAN_ERR_OK;
}



elysian_mvc_controller_t* elysian_mvc_controller_get(elysian_t* server, char* url, elysian_http_method_e method_id){
    elysian_mvc_controller_t* controller = server->controllers;
    ELYSIAN_LOG("Searching user defined controller for url '%s' and method '%s'", url, elysian_http_get_method_name(method_id));
    while(controller){
		ELYSIAN_LOG("Trying to match with controller %s", controller->url);
        if (strcmp(controller->url, url) == 0) {
			ELYSIAN_LOG("flag = %u, method id = %u", controller->flags, method_id);
			if ( (controller->flags & ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_GET) && ((method_id == ELYSIAN_HTTP_METHOD_GET) || (method_id == ELYSIAN_HTTP_METHOD_HEAD))) {
				return controller;
			} else if ((controller->flags & ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_POST) && (method_id == ELYSIAN_HTTP_METHOD_POST)) {
				return controller;
			} else if ((controller->flags & ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_PUT) && (method_id == ELYSIAN_HTTP_METHOD_PUT)) {
				return controller;
			}else {
				return NULL;
			}
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
elysian_err_t elysian_mvc_add_alloc(elysian_t* server, void* data) {
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
elysian_err_t elysian_mvc_param_get(elysian_t* server, char* param_name, elysian_req_param_t* req_param){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_req_param_t* param_next;

	ELYSIAN_LOG("elysian_mvc_param_get(%s)", param_name);
	
#if 1
	param_next = client->httpreq.params;
	while(param_next){
		ELYSIAN_LOG("Comparing with '%s'", param_next->name);

		param_next = param_next->next;
	};
#endif
	
    //req_param->client = client;
    
	param_next = client->httpreq.params;
	while(param_next){
		ELYSIAN_LOG("Comparing with '%s'", param_next->name);
		if (strcmp(param_name, param_next->name) == 0) {
			*req_param = *param_next;
			ELYSIAN_LOG("param '%s' found!", param_name);
			return ELYSIAN_ERR_OK;
		}
		param_next = param_next->next;
	};
	
	/*
	** Parameter not found
	*/
	req_param->client = NULL;
	req_param->file = NULL;
	req_param->data_index = ELYSIAN_INDEX_OOB32;
	req_param->data_size = 0;
	
	/*
	** Don't return an error here, let the app layer decide if 
	** the missing param is an error or not.
	*/
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_mvc_param_size(elysian_t* server, elysian_req_param_t* req_param, uint32_t* param_size) {
	if ((!req_param) || (!req_param->client) || (!req_param->file) || (req_param->data_index == ELYSIAN_INDEX_OOB32)) {
		/*
		** elysian_mvc_param_get() was not used, or was used but returned "parameter not found"
		*/
		return ELYSIAN_ERR_FATAL;
	}
	
	*param_size = req_param->data_size;
	
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_mvc_param_filename(elysian_t* server, elysian_req_param_t* req_param, char** filename) {
	if ((!req_param) || (!req_param->client) || (!req_param->file) || (req_param->data_index == ELYSIAN_INDEX_OOB32)) {
		/*
		** elysian_mvc_param_get() was not used, or was used but returned "parameter not found"
		*/
		return ELYSIAN_ERR_FATAL;
	}
	
	*filename = req_param->filename;
	
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_mvc_param_read(elysian_t* server, elysian_req_param_t* req_param, uint32_t offset, uint8_t* buf, uint32_t buf_size, uint32_t* read_size) {
    uint32_t current_offset;
    elysian_err_t err;
     
    ELYSIAN_ASSERT(buf, "");
    
    *read_size = 0;
    
	if ((!req_param) || (!req_param->client) || (!req_param->file) || (req_param->data_index == ELYSIAN_INDEX_OOB32)) {
		/*
		** elysian_mvc_param_get() was not used, or was used but returned "parameter not found"
		*/
		return ELYSIAN_ERR_FATAL;
	}
	
	/*
	** This could happen in POST request, where the last param index 
	** could be equal to filesize, if for example the last param is empty.
	** 'param1=test+data+%231%21&param2=test+data+%232%21&param3='
	** We are not allowed to seek to a file with size 0, or to a position equal to fsize
	*/
	if (req_param->data_size == 0) {
		return ELYSIAN_ERR_OK;
	}
	
	if (offset >= req_param->data_size) {
		return ELYSIAN_ERR_OK;
	}
	
	if (buf_size > req_param->data_size - offset) {
		buf_size = req_param->data_size - offset;
	}
	
    err = elysian_fs_ftell(server, req_param->file, &current_offset);
    if(err != ELYSIAN_ERR_OK){
        return err;
    }
	
	ELYSIAN_LOG("current_offset is %u", current_offset);
	
	if(current_offset != req_param->data_index + offset){
		err = elysian_fs_fseek(server, req_param->file, req_param->data_index + offset);
        if(err != ELYSIAN_ERR_OK){
            return err;
        }
	}
    err = elysian_fs_fread(server, req_param->file, buf, buf_size, read_size);
	if(err != ELYSIAN_ERR_OK){
        return err;
    }

    return err;
}

elysian_err_t elysian_mvc_param_get_raw(elysian_t* server, char* param_name, uint8_t decode, uint8_t** param_value,  uint32_t* param_size,  uint8_t* param_found) {
	//elysian_client_t* client = elysian_schdlr_current_client_get(server);
	uint32_t actual_read_size;
	elysian_req_param_t param;
	uint8_t* buf;
	elysian_err_t err;
	
	*param_value = NULL;
	*param_found = 0;
	*param_size = 0;
	err = elysian_mvc_param_get(server, param_name, &param);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	if(param.data_index == ELYSIAN_INDEX_OOB32){
		/*
		** Parameter not found
		*/
		return ELYSIAN_ERR_OK;
	}
	
	/*
	** Parameter found
	*/
	*param_found = 1;

	/*
	** Allocate size +1 so it can be used as raw bytes or string
	*/
	buf = elysian_mem_malloc(server, param.data_size + 1, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
	if(buf == NULL){
		return ELYSIAN_ERR_POLL;
	}
	
	err = elysian_mvc_param_read(server, &param, 0, buf, param.data_size, &actual_read_size);
	if(err != ELYSIAN_ERR_OK){
		elysian_mem_free(server, buf);
		return err;
	}
	
	ELYSIAN_ASSERT(param.data_size == actual_read_size , "");
	
	buf[actual_read_size] = '\0';
	
	if (decode) {
		elysian_http_decode((char*) buf);
	}
	
	err = elysian_mvc_add_alloc(server, buf);
	if(err != ELYSIAN_ERR_OK){
		elysian_mem_free(server, buf);
		return err;
	}
	
	*param_value = buf;
	*param_size = param.data_size;
	
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_mvc_param_get_bytes(elysian_t* server, char* param_name, uint8_t** param_value,  uint32_t* param_size, uint8_t* param_found) {
	//elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	
	err = elysian_mvc_param_get_raw(server, param_name, 0, param_value, param_size, param_found);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}

	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_mvc_param_get_str(elysian_t* server, char* param_name, char** param_value, uint8_t* param_found){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	uint8_t decode = 0;
	uint32_t param_size;
	elysian_err_t err;
	
	/*
	** Decode only if it is a parameter of HTML form submission with POST or GET method.
	*/
	if ((strcmp(param_name, ELYSIAN_MVC_PARAM_HTTP_HEADERS) != 0) && (strcmp(param_name, ELYSIAN_MVC_PARAM_HTTP_BODY) != 0)) {
		if ((client->httpreq.method == ELYSIAN_HTTP_METHOD_GET) || 
				(client->httpreq.method == ELYSIAN_HTTP_METHOD_HEAD) || 
				(client->httpreq.content_type == ELYSIAN_HTTP_CONTENT_TYPE_APPLICATION__X_WWW_FORM_URLENCODED)) {
			decode = 1;
		}
	}

	err = elysian_mvc_param_get_raw(server, param_name, decode, (uint8_t**) param_value, &param_size, param_found);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_mvc_param_get_uint(elysian_t* server, char* param_name, uint32_t* param_value, uint8_t* param_found){
	elysian_err_t err;
	char* param_value_str;
	
	*param_value = 0;
	err = elysian_mvc_param_get_str(server, param_name, &param_value_str, param_found);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	
	elysian_str2uint(param_value_str, param_value);

	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_mvc_param_get_int(elysian_t* server, char* param_name, int32_t* param_value, uint8_t* param_found){
	elysian_err_t err;
	char* param_value_str;
	
	*param_value = 0;
	err = elysian_mvc_param_get_str(server, param_name, &param_value_str, param_found);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	
	*param_value = atoi(param_value_str);
	
	return ELYSIAN_ERR_OK;
}




