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

/* --------------------------------------------------------------------------------------------------------------------------------
| HTTP request
-------------------------------------------------------------------------------------------------------------------------------- */
elysian_err_t elysian_http_request_headers_received(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    uint32_t index0;
    //elysian_err_t err;

    index0 = elysian_cbuf_strstr(client->rcv_cbuf_list, 0, "\r\n\r\n", 0);
    if(index0 == ELYSIAN_INDEX_OOB32){
		if(elysian_cbuf_list_len(client->rcv_cbuf_list) > 1024){
			return ELYSIAN_ERR_FATAL;
		}
        return ELYSIAN_ERR_READ;
    }

    client->httpreq.headers_len = index0 + 4;
    client->httpreq.body_len = 0;

    return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_http_request_headers_parse(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    char* header_value;
	char* header_value_tmp;
    elysian_err_t err;
	char* multipart_boundary;
	elysian_http_method_e method;
	elysian_mvc_controller_t* controller;
	char* url;
	uint32_t max_http_body_size;
	
	client->httpreq.url = NULL;
	client->httpreq.body_len = 0;
	client->httpreq.expect_status_code = ELYSIAN_HTTP_STATUS_CODE_NA;
	client->httpreq.content_type = ELYSIAN_HTTP_CONTENT_TYPE_NA;
	client->httpreq.method = ELYSIAN_HTTP_METHOD_NA;
	
	
	/*
	** Get any "Expect: 100-continue"
	*/
	err = elysian_http_request_get_header(server, "Expect" , &header_value);
	if(err != ELYSIAN_ERR_OK){
        goto handle_error;
    }
	if(!header_value){
    }else{
		ELYSIAN_LOG("Request with expectation!");
		client->httpreq.expect_status_code = ELYSIAN_HTTP_STATUS_CODE_100;
		elysian_mem_free(server, header_value);
	}
	
	/*
	** Get method
	*/
    err = elysian_http_request_get_method(server, &method);
    if(err != ELYSIAN_ERR_OK){
        goto handle_error;
    }
	client->httpreq.method = method;
	
	/*
	** Get URL
	*/
	err = elysian_http_request_get_uri(server, &url);
	if(err != ELYSIAN_ERR_OK){
        goto handle_error;
    }
	ELYSIAN_LOG("URL = '%s'!", url);
    client->httpreq.url = url;
	

	
	/*
	** Get Transfer-Encoding
	*/
	err = elysian_http_request_get_header(server, "Transfer-Encoding" , &header_value);
	if(err != ELYSIAN_ERR_OK){
        goto handle_error;
    }
	if(!header_value){
		client->httpreq.transfer_encoding = ELYSIAN_HTTP_TRANSFER_ENCODING_RAW;
    }else{
		if(strcmp(header_value, "chunked") == 0){
			ELYSIAN_LOG("Chunked Request!");
			client->httpreq.transfer_encoding = ELYSIAN_HTTP_TRANSFER_ENCODING_CHUNKED;
		} else {
			/*
			** Unknown encoding
			*/
			client->httpreq.transfer_encoding = ELYSIAN_HTTP_TRANSFER_ENCODING_NA;
		}
		elysian_mem_free(server, header_value);
	}

	/*
	** Get Range
	*/
	err = elysian_http_request_get_header(server, "Range" , &header_value);
	header_value_tmp = header_value;
	if(err != ELYSIAN_ERR_OK){
        goto handle_error;
    }
	if(!header_value_tmp){
		client->httpreq.range_start = ELYSIAN_HTTP_RANGE_WF;
		client->httpreq.range_end = ELYSIAN_HTTP_RANGE_WF;
    }else{
		/*
		** bytes=100-
		** bytes=0-1024
		*/
		char* barrier;
		ELYSIAN_LOG("Partial request is '%s'", header_value_tmp);
		barrier = elysian_strstr(header_value_tmp, "=");
		if(barrier){
			header_value_tmp = barrier + 1;
			barrier = elysian_strstr(header_value_tmp, "-");
			if(barrier){
				*barrier = '\0';
                if(strlen(header_value_tmp)){
                    client->httpreq.range_start = atoi(header_value_tmp);
                }else{
                    elysian_mem_free(server, header_value);
                    err = ELYSIAN_ERR_FATAL;
                    goto handle_error;
                }
				ELYSIAN_LOG("Range start is '%u'", client->httpreq.range_start);
				header_value_tmp = barrier + 1;
				//ELYSIAN_LOG("Range header_value is '%s'", header_value);
                if(strlen(header_value_tmp)){
                    client->httpreq.range_end = atoi(header_value_tmp);
                }else{
                    client->httpreq.range_end = ELYSIAN_HTTP_RANGE_EOF;
                }
				ELYSIAN_LOG("Range end is '%u'", client->httpreq.range_end);
				elysian_mem_free(server, header_value);
			}else{
				elysian_mem_free(server, header_value);
				err = ELYSIAN_ERR_FATAL;
				goto handle_error;
			}
		}else{
			elysian_mem_free(server, header_value);
			err = ELYSIAN_ERR_FATAL;
			goto handle_error;
		}
		ELYSIAN_LOG("Partial detected request range is = %u - %u", client->httpreq.range_start, client->httpreq.range_end);
		if((client->httpreq.range_start > client->httpreq.range_end)) {
			elysian_set_fatal_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_400);
			err = ELYSIAN_ERR_FATAL;
			goto handle_error;
		}
		//while(1){}
	}
	
	/*
	** Get content-length
	*/
	err = elysian_http_request_get_header(server, "Content-Length" , &header_value);
	if(err != ELYSIAN_ERR_OK){
        goto handle_error;
    }
	if(!header_value){
		client->httpreq.body_len = 0;
    }else{
		client->httpreq.body_len = (uint32_t) atoi(header_value);
		elysian_mem_free(server, header_value);
        
#if 1
		controller = elysian_mvc_controller_get(server, client->httpreq.url, client->httpreq.method);
		if((controller) && (controller->flags & ELYSIAN_MVC_CONTROLLER_FLAG_SAVE_TO_DISK)) {
			max_http_body_size = ELYSIAN_MAX_HTTP_BODY_SIZE_KB_DISK;
		} else {
			max_http_body_size = ELYSIAN_MAX_HTTP_BODY_SIZE_KB_RAM;
		}
		if(client->httpreq.body_len > max_http_body_size * 1024){
			elysian_set_fatal_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_413);
			err = ELYSIAN_ERR_FATAL;
            goto handle_error;
        }
#else
        if(client->httpreq.body_len > ELYSIAN_MAX_UPLOAD_SIZE_KB * 1024){
			elysian_set_fatal_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_413);
			err = ELYSIAN_ERR_FATAL;
            goto handle_error;
        }
#endif
	}
    ELYSIAN_LOG("Content-length = %u!", client->httpreq.body_len);
	
	/*
	** Get content-type
	*/
	err = elysian_http_request_get_header(server, "Content-Type" , &header_value);
	if(err != ELYSIAN_ERR_OK){
        goto handle_error;
    }
	if(!header_value){
        client->httpreq.content_type = ELYSIAN_HTTP_CONTENT_TYPE_NA;
    }else{
        ELYSIAN_LOG("Content-Type = '%s'!", header_value);
        if(strcmp(header_value, "application/x-www-form-urlencoded") == 0){
            ELYSIAN_LOG("URL ENCODED!");
            client->httpreq.content_type = ELYSIAN_HTTP_CONTENT_TYPE_APPLICATION__X_WWW_FORM_URLENCODED;
        }else if(strncmp(header_value, "multipart/form-data", strlen("multipart/form-data")) == 0){
			/*
			 * multipart/form-data; boundary=----WebKitFormBoundarybSkuzkMYwqxqQudn
			*/
            ELYSIAN_LOG("MULTIPART!");
            client->httpreq.content_type = ELYSIAN_HTTP_CONTENT_TYPE_MULTIPART__FORM_DATA;
			multipart_boundary = elysian_strcasestr(header_value, "boundary=");
			if(!multipart_boundary) {
				err = ELYSIAN_ERR_FATAL;
				goto handle_error;
			}
			multipart_boundary += 9;
			while(*multipart_boundary == ' ') {
				multipart_boundary++;
			}
			if(multipart_boundary == '\0') {
				elysian_set_fatal_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_400);
				err = ELYSIAN_ERR_FATAL;
				goto handle_error;
			}
			
			client->httpreq.multipart_boundary = elysian_mem_malloc(server, strlen(multipart_boundary) + 1, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
			if(!client->httpreq.multipart_boundary){
				err = ELYSIAN_ERR_POLL;
				goto handle_error;
			}
			
			strcpy(client->httpreq.multipart_boundary, multipart_boundary);
			
			ELYSIAN_LOG("MULTIPART Boundary is '%s'!", client->httpreq.multipart_boundary);
        }else{
            ELYSIAN_LOG("NA!");
            client->httpreq.content_type = ELYSIAN_HTTP_CONTENT_TYPE_NA;
        }
        elysian_mem_free(server, header_value);
    }
	

    
	
	return ELYSIAN_ERR_OK;
	
	handle_error:
		if(client->httpreq.url){
		    elysian_mem_free(server, client->httpreq.url);
			client->httpreq.url = NULL;
		}
		if(client->httpreq.multipart_boundary){
		    elysian_mem_free(server, client->httpreq.multipart_boundary);
			client->httpreq.multipart_boundary = NULL;
		}
		
		client->httpreq.body_len = 0;
		client->httpreq.content_type = ELYSIAN_HTTP_CONTENT_TYPE_NA;
		client->httpreq.method = ELYSIAN_HTTP_METHOD_NA;
		
#if 1 // HTTP expectation
		if( (err == ELYSIAN_ERR_FATAL) && (client->httpreq.expect_status_code == ELYSIAN_HTTP_STATUS_CODE_100)) {
			/*
			** Don't close the connection, send the expectation failed msg first
			*/
			err = ELYSIAN_ERR_OK;
			client->httpreq.expect_status_code = ELYSIAN_HTTP_STATUS_CODE_417;
		}
#endif
		
        return err;
}



elysian_err_t elysian_http_request_get_method(elysian_t* server, elysian_http_method_e* method_id){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    char method_name[32];
    elysian_err_t err;
	uint32_t index0, index1;
    
    *method_id = ELYSIAN_HTTP_METHOD_NA;
	err = elysian_strstr_file(server, &client->httpreq.headers_file, 0, " ", "HTTP/1.",  0, &index0, &index1);
    if(err != ELYSIAN_ERR_OK){
        return err;
    }
	if(index0 == ELYSIAN_INDEX_OOB32 || index1 == ELYSIAN_INDEX_OOB32){
        return ELYSIAN_ERR_FATAL;
    }
	if(index0 > sizeof(method_name) - 1){
		return ELYSIAN_ERR_FATAL;
	}
    err = elysian_strncpy_file(server, &client->httpreq.headers_file, 0, method_name, index0);
    if(err != ELYSIAN_ERR_OK){
        return err;
    }
    ELYSIAN_LOG("Request method is %s", method_name);
    *method_id = elysian_http_get_method_id(method_name);
    if(*method_id == ELYSIAN_HTTP_METHOD_NA){
        return ELYSIAN_ERR_FATAL;
    }
    return ELYSIAN_ERR_OK;
}


elysian_err_t elysian_http_request_get_uri(elysian_t* server, char** uri){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    uint32_t index0;
    uint32_t index1;
	uint32_t max_index;
	uint32_t dummy_index;
    elysian_err_t err;
    
    *uri = NULL;
	err = elysian_strstr_file(server, &client->httpreq.headers_file, 0, " HTTP/1.", "",  0, &max_index, &dummy_index);
    if(err != ELYSIAN_ERR_OK){
        return err;
    }
	if(max_index == ELYSIAN_INDEX_OOB32){
        return ELYSIAN_ERR_FATAL;
    }
    err = elysian_strstr_file(server, &client->httpreq.headers_file, 0, " ", "?", 0, &index0, &index1);
    if(err != ELYSIAN_ERR_OK){
        return err;
    }
    if(index0 == ELYSIAN_INDEX_OOB32 || index0 >= max_index){
        return ELYSIAN_ERR_FATAL;
    }else{
		index0++;
	}
	if(index1 == ELYSIAN_INDEX_OOB32 || index1 > max_index){
		index1 = max_index;
	}
	index1--;
    ELYSIAN_LOG("URI indexes [%u,%u]", index0, index1);
    *uri = elysian_mem_malloc(server, (index1 - index0 + 1) + 1 /* '\0' */ + strlen(ELYSIAN_FS_INDEX_HTML_VRT_ROOT) + 10 /* index.html */, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
    if(*uri == NULL){
        return ELYSIAN_ERR_POLL;
    }
    elysian_strncpy_file(server, &client->httpreq.headers_file, index0, *uri, index1 - index0 + 1);
    elysian_str_trim(server, *uri, " ", " ");
    if(strcmp(*uri, "/") == 0){
        strcpy(*uri, ELYSIAN_FS_INDEX_HTML_VRT_ROOT"/index.html");
    }
    ELYSIAN_LOG("Encoded URI = '%s'", *uri);
    elysian_http_decode(*uri);
    ELYSIAN_LOG("Dencoded URI = '%s'", *uri);
    return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_http_request_get_header(elysian_t* server, char* header_name, char** header_value){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    uint32_t index0;
    uint32_t index1;
	char* expanded_header_name;
    elysian_err_t err;
    
    *header_value = NULL;
	expanded_header_name = elysian_mem_malloc(server, 2 /* \r\n */ + strlen(header_name) + 1 /* '\0' */, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
    if(!expanded_header_name){
        return ELYSIAN_ERR_POLL;
    }
	memcpy(expanded_header_name, "\r\n", 2);
	strcpy(&expanded_header_name[2], header_name);
    err = elysian_strstr_file(server, &client->httpreq.headers_file, 0, expanded_header_name, "\r\n", 0, &index0, &index1);
	elysian_mem_free(server, expanded_header_name);
    if(err != ELYSIAN_ERR_OK){
        return err;
    }
    if(index0 == ELYSIAN_INDEX_OOB32 || index1 == ELYSIAN_INDEX_OOB32){
        return ELYSIAN_ERR_OK;
    }
    index0 += 2 + strlen(header_name);
    index1 -= 1;
    *header_value = elysian_mem_malloc(server, (index1 - index0 + 1) + 1 /* '\0' */, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
    if(*header_value == NULL){
        return ELYSIAN_ERR_POLL;
    }
    elysian_strncpy_file(server, &client->httpreq.headers_file, index0, *header_value, index1 - index0 + 1);
    elysian_str_trim(server, *header_value, ": ", " ");
    ELYSIAN_LOG("Header '%s' value is '%s'", header_name, *header_value);      
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
elysian_err_t elysian_http_request_get_params(elysian_t* server){
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
	
	ELYSIAN_ASSERT(client->httpreq.params == NULL , "");
	
	/*
	** First add the HTTP request header/body params
	*/

	/*
	** HTTP headers parameter
	*/
	param = elysian_mem_malloc(server, sizeof(elysian_req_param_t), ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
	if (!param) {
		err = ELYSIAN_ERR_POLL;
		goto handle_error;
	}
	param->next = client->httpreq.params;
	client->httpreq.params = param;
	param->client = client;
	param->filename = NULL; // initialize before trying malloc param->name else we may free unallocated memory..
	param->data_index = 0;
	param->file = &client->httpreq.headers_file;
	param->name = elysian_mem_malloc(server, strlen(ELYSIAN_MVC_PARAM_HTTP_HEADERS) + 1, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
	if (!param->name) {
		err = ELYSIAN_ERR_POLL;
		goto handle_error;
	}
	strcpy(param->name, ELYSIAN_MVC_PARAM_HTTP_HEADERS);

	err = elysian_fs_fsize(server, param->file, &param->data_size);
	if(err != ELYSIAN_ERR_OK){
		ELYSIAN_LOG("ERROR_0");
		goto handle_error;
	}
	
	/*
	** HTTP body parameter
	*/
	param = elysian_mem_malloc(server, sizeof(elysian_req_param_t), ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
	if (!param) {
		err = ELYSIAN_ERR_POLL;
		goto handle_error;
	}
	param->next = client->httpreq.params;
	client->httpreq.params = param;
	param->client = client;
	param->filename = NULL;
	param->data_index = 0;
	param->file = &client->httpreq.body_file;
	param->name = elysian_mem_malloc(server, strlen(ELYSIAN_MVC_PARAM_HTTP_BODY) + 1, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
	if (!param->name) {
		err = ELYSIAN_ERR_POLL;
		goto handle_error;
	}
	strcpy(param->name, ELYSIAN_MVC_PARAM_HTTP_BODY);

	err = elysian_fs_fsize(server, param->file, &param->data_size);
	if(err != ELYSIAN_ERR_OK){
		ELYSIAN_LOG("ERROR_0");
		goto handle_error;
	}
	
	
#if 1
	if (client->httpreq.content_type == ELYSIAN_HTTP_CONTENT_TYPE_MULTIPART__FORM_DATA) {
		if (client->isp.multipart.params) {
			client->isp.multipart.params->next = client->httpreq.params;
			client->httpreq.params = client->isp.multipart.params;
			client->isp.multipart.params = NULL;
		}
		return ELYSIAN_ERR_OK;
	}
#endif
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
			elysian_sprintf(param_search_pattern, "--%s\r\n", client->httpreq.multipart_boundary);
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
			ELYSIAN_LOG("param_search_index = %u + %u + %u + %u = %u", (unsigned int) param_prev->data_index, (unsigned int) param_prev->data_size, (unsigned int) strlen(div2), (unsigned int)strlen(div3), (unsigned int)(param_prev->data_index + param_prev->data_size + strlen(div2) + strlen(div3)));
			param_search_index = param_prev->data_index + param_prev->data_size + strlen(div2) + strlen(div3);
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
					elysian_sprintf(param_search_pattern, "--%s--", client->httpreq.multipart_boundary);
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
		param->next = client->httpreq.params;
		client->httpreq.params = param;
		param->client = client;
		param->file = param_file;
		param->name = NULL;
		param->filename = NULL;
		param->data_index = index0 + strlen(div1);
		param->data_size = (index1 - strlen(div2)) - (param->data_index);
		
		//param->index0 = param->data_index;
		//param->len = param->data_len;
		param->data_index_cur = param->data_index;
		
		ELYSIAN_LOG("param->body_index = %u, param->body_len = %u", param->data_index, param->data_size);
		
		if (param_prev) {
			param_header_index = param_prev->data_index + param_prev->data_size + strlen(div2) + strlen(div3);
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
			char* name = elysian_strcasestr(param_header, "name=\"");
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
		while(client->httpreq.params){
			ELYSIAN_LOG("Releasing param..");
			req_param_next = client->httpreq.params->next;
			if(client->httpreq.params->name){
				ELYSIAN_LOG("Releasing param name");
				elysian_mem_free(server, client->httpreq.params->name);
			}
			if(client->httpreq.params->filename){
				ELYSIAN_LOG("Releasing param filename..");
				elysian_mem_free(server, client->httpreq.params->filename);
			}
			elysian_mem_free(server, client->httpreq.params);
			client->httpreq.params = req_param_next;
		};
	}
	
	return err;
}
	
/* --------------------------------------------------------------------------------------------------------------------------------
| HTTP response
-------------------------------------------------------------------------------------------------------------------------------- */
elysian_err_t elysian_http_response_build(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
    char header_value[64];
	
	client->httpresp.buf_index = 0;
	client->httpresp.buf_len = 0;
				
	/*
	** Initiate HTTP Response
	*/
	err = elysian_http_add_response_status_line(server);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	
	err = elysian_http_add_response_header_line(server, "Server", ELYSIAN_SERVER_NAME);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	
	err = elysian_http_add_response_header_line(server, "Accept-Ranges", "bytes");
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	
	err = elysian_http_add_response_header_line(server, "Connection", (client->httpresp.keep_alive == 1) ? "keep-alive" : "close");
	if(err != ELYSIAN_ERR_OK){
		return err;
	}

	if(client->httpresp.status_code == ELYSIAN_HTTP_STATUS_CODE_401){
		/* WWW-Authenticate: Basic realm="My Server" */
		elysian_sprintf(header_value, "Basic realm=\"%s\"", ELYSIAN_SERVER_NAME);
		err = elysian_http_add_response_header_line(server, "WWW-Authenticate", header_value);
		if(err != ELYSIAN_ERR_OK){
			return err;
		}
	}
	
	if(client->httpresp.status_code == ELYSIAN_HTTP_STATUS_CODE_206){
		/* Content-Range: bytes 0-64657026/64657027 */
		elysian_sprintf(header_value, "bytes %u-%u/%u", client->httpreq.range_start, client->httpreq.range_end, client->httpresp.resource_size);
		err = elysian_http_add_response_header_line(server, "Content-Range", header_value);
		if(err != ELYSIAN_ERR_OK){
			return err;
		}
	}
	
	if(client->httpresp.body_size > 0){
		err = elysian_http_add_response_header_line(server, "Content-Type", elysian_http_get_mime_type(client->mvc.view));
		if(err != ELYSIAN_ERR_OK){
			return err;
		}
	}
	
	elysian_sprintf(header_value, "%u", client->httpresp.body_size);
	err = elysian_http_add_response_header_line(server, "Content-Length", header_value);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	
#if 1 //#ifdef ELYSIAN_HTTP_CACHE_DISABLED
    /*
    ** no-store : don't cache this to disk, caching to RAM is stil perimtted [for security reasons]
    ** no-cache : don't cache, use ETAG for revalidation
    ** max-age  : in seconds, for how long the resource can be cached
    */
	if(client->httpresp.body_size > 0){
		err = elysian_http_add_response_header_line(server, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
		if(err != ELYSIAN_ERR_OK){
			return err;
		}
	}
#else
		
#endif
	
	if(client->httpresp.status_code == ELYSIAN_HTTP_STATUS_CODE_302){
		ELYSIAN_ASSERT(client->httpresp.redirection_url, "");
		err = elysian_http_add_response_header_line(server, "Location", client->httpresp.redirection_url);
		if(err != ELYSIAN_ERR_OK){
			return err;
		}
	}
	
	/*
	** Terminate HTTP Response
	*/
	err = elysian_http_add_response_empty_line(server);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}

    printf("[%s]",client->httpresp.buf);
    
    return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_http_add_response_status_line(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	uint16_t status_line_len = strlen("HTTP/1.1 ") + 3 /* Status code */ + 1 /* Space */ + strlen(elysian_http_get_status_code_msg(client->httpresp.status_code)) + 2 /* \r\n */;
	if(client->httpresp.buf_len + status_line_len + 1 /* '\0' */ < client->httpresp.buf_size){
		elysian_sprintf((char*)client->httpresp.buf, "HTTP/1.1 %u %s\r\n", elysian_http_get_status_code_num(client->httpresp.status_code), elysian_http_get_status_code_msg(client->httpresp.status_code));
		client->httpresp.buf_len = strlen((char*)client->httpresp.buf);
		return ELYSIAN_ERR_OK;
	}else{
		return ELYSIAN_ERR_BUF;
	}
}

elysian_err_t elysian_http_add_response_header_line(elysian_t* server, char* header_name, char* header_value){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    uint16_t header_name_len;
    uint16_t header_value_len;
    
    /*
    ** Add HTTP Header
    */
    header_name_len = strlen(header_name);
    header_value_len = strlen(header_value);
    if(client->httpresp.buf_len + header_name_len + header_value_len + 4 < client->httpresp.buf_size){
        memcpy(&client->httpresp.buf[client->httpresp.buf_len], header_name, header_name_len);
        client->httpresp.buf_len += header_name_len;
        memcpy(&client->httpresp.buf[client->httpresp.buf_len], ": ", 2);
        client->httpresp.buf_len += 2;
        memcpy(&client->httpresp.buf[client->httpresp.buf_len], header_value, header_value_len);
        client->httpresp.buf_len += header_value_len;
        memcpy(&client->httpresp.buf[client->httpresp.buf_len], "\r\n", 2);
        client->httpresp.buf_len += 2;
        return ELYSIAN_ERR_OK;
    }else{
        //ELYSIAN_ASSERT(0,"");
        return ELYSIAN_ERR_BUF;
    } 
}


elysian_err_t elysian_http_add_response_empty_line(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	
	if(client->httpresp.buf_len + 2 + 1 /* '\0' */ < client->httpresp.buf_size){
		memcpy(&client->httpresp.buf[client->httpresp.buf_len], "\r\n", 2);
		client->httpresp.buf_len += 2;
        client->httpresp.buf[client->httpresp.buf_len] = '\0';
		return ELYSIAN_ERR_OK;
	}else{
		//ELYSIAN_ASSERT(0,"");
		return ELYSIAN_ERR_BUF;
	}
}

/* --------------------------------------------------------------------------------------------------------------------------------
| HTTP decoding
-------------------------------------------------------------------------------------------------------------------------------- */
void elysian_http_decode(char *encoded) {
    char* decoded = encoded;
    while (*encoded) {
        if (*encoded == '%') {
            if (encoded[1] && encoded[2]) {
                encoded[1] = (encoded[1] >= '0' && encoded[1] <= '9') ? encoded[1] - '0' : tolower(encoded[1]) - 'a' + 10;
                encoded[2] = (encoded[2] >= '0' && encoded[2] <= '9') ? encoded[2] - '0' : tolower(encoded[2]) - 'a' + 10;
                *decoded++ = encoded[1] << 4 | encoded[2];
                encoded += 2;
            }
        } else if (*encoded == '+') { 
            *decoded++ = ' ';
        } else {
            *decoded++ = *encoded;
        }
        encoded++;
    }
    *decoded = '\0';
}

/* --------------------------------------------------------------------------------------------------------------------------------
| HTTP methods
-------------------------------------------------------------------------------------------------------------------------------- */
typedef struct elysian_http_method_t elysian_http_method_t;
struct elysian_http_method_t{
    elysian_http_method_e id;
	char* name;
};

const elysian_http_method_t elysian_http_methods[] = {
	{
		.id = ELYSIAN_HTTP_METHOD_GET,
		.name = "GET",
    },
    {
		.id = ELYSIAN_HTTP_METHOD_HEAD,
		.name = "HEAD",
    },
    {
		.id = ELYSIAN_HTTP_METHOD_POST,
		.name = "POST",
    },
    {
		.id = ELYSIAN_HTTP_METHOD_PUT,
		.name = "PUT",
    },
    {
		.id = ELYSIAN_HTTP_METHOD_SUBSCRIBE,
		.name = "SUBSCRIBE",
    },
    {
		.id = ELYSIAN_HTTP_METHOD_UNSUBSCRIBE,
		.name = "UNSUBSCRIBE",
    },
};

char* elysian_http_get_method_name(elysian_http_method_e method_id){
    uint32_t i;
    for(i = 0; i < sizeof(elysian_http_methods)/sizeof(elysian_http_methods[0]); i++){
        if(elysian_http_methods[i].id == method_id){
            return elysian_http_methods[i].name;
        }
    }
    return "";
}

elysian_http_method_e elysian_http_get_method_id(char* method_name){
    uint32_t i;
    for(i = 0; i < sizeof(elysian_http_methods)/sizeof(elysian_http_methods[0]); i++){
        if(strcmp(elysian_http_methods[i].name, method_name) == 0){
            return elysian_http_methods[i].id;
        }
    }
    return ELYSIAN_HTTP_METHOD_NA;
}

/* --------------------------------------------------------------------------------------------------------------------------------
| HTTP status codes
-------------------------------------------------------------------------------------------------------------------------------- */
typedef struct elysian_http_status_code_t elysian_http_status_code_t;
struct elysian_http_status_code_t{
	uint16_t code_num;
	char* code_msg;
	char* code_body;
};

const elysian_http_status_code_t elysian_http_status_codes[] = {
	[ELYSIAN_HTTP_STATUS_CODE_100] = {
		.code_num = 100,
		.code_msg = "Continue",
		.code_body = ""
		},
	[ELYSIAN_HTTP_STATUS_CODE_200] = {
		.code_num = 200,
		.code_msg = "OK",
		.code_body = ""
		},
	[ELYSIAN_HTTP_STATUS_CODE_206] = {
		.code_num = 206,
		.code_msg = "Partial Content",
		.code_body = ""
		},
	[ELYSIAN_HTTP_STATUS_CODE_302] = {
		.code_num = 302,
		.code_msg = "Found",
		.code_body = "Error 301: This page was temporary moved."
		},
	[ELYSIAN_HTTP_STATUS_CODE_400] = {
		.code_num = 400,
		.code_msg = "Bad Request",
		.code_body = "Error 400: Bad Request."
	},
	[ELYSIAN_HTTP_STATUS_CODE_401] = {
		.code_num = 401,
		.code_msg = "Unauthorized",
		.code_body = "Error 401: Not authorized!"
		},
	[ELYSIAN_HTTP_STATUS_CODE_404] = {
		.code_num = 404,
		.code_msg = "Not Found",
		.code_body = "Error 404: The requested URL was not found."
	},
	[ELYSIAN_HTTP_STATUS_CODE_405] = {
		.code_num = 405,
		.code_msg = "Method Not Allowed",
		.code_body = "Error 405: Method Not Allowed."
	},
	[ELYSIAN_HTTP_STATUS_CODE_408] = {
		.code_num = 408,
		.code_msg = "Request Timeout",
		.code_body = "Error 408: Request Timeout."
	},
	[ELYSIAN_HTTP_STATUS_CODE_413] = {
		.code_num = 413,
		.code_msg = "Payload Too Large",
		.code_body = "Error 413: Payload Too Large."
	},
	[ELYSIAN_HTTP_STATUS_CODE_417] = {
		.code_num = 417,
		.code_msg = "Expectation Failed",
		.code_body = ""
	},
	[ELYSIAN_HTTP_STATUS_CODE_500] = {
		.code_num = 500,
		.code_msg = "Internal Server Error",
		.code_body = "Error 500: The server encountered an internal error."
		},
	[ELYSIAN_HTTP_STATUS_CODE_MAX] = {
		.code_num = 999,
		.code_msg = "",
		.code_body = ""
		},
	[ELYSIAN_HTTP_STATUS_CODE_NA] = {
		.code_num = 999,
		.code_msg = "",
		.code_body = ""
		},
};

uint16_t elysian_http_get_status_code_num(elysian_http_status_code_e status_code){
	ELYSIAN_ASSERT(status_code < ELYSIAN_HTTP_STATUS_CODE_MAX, "");
	return elysian_http_status_codes[status_code].code_num;
}

char* elysian_http_get_status_code_msg(elysian_http_status_code_e status_code){
	ELYSIAN_ASSERT(status_code < ELYSIAN_HTTP_STATUS_CODE_MAX, "");
	return elysian_http_status_codes[status_code].code_msg;
}

char* elysian_http_get_status_code_body(elysian_http_status_code_e status_code){
	ELYSIAN_ASSERT(status_code < ELYSIAN_HTTP_STATUS_CODE_MAX, "");
	return elysian_http_status_codes[status_code].code_body;
}

/* --------------------------------------------------------------------------------------------------------------------------------
| Basic Access Authentication
-------------------------------------------------------------------------------------------------------------------------------- */
#if 0
static const char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
static const int mod_table[] = {0, 2, 1};

char* elysian_http_base64_encode(char *data) {
	uint32_t input_length;
	uint32_t output_length;
	int i, j;
	
	input_length = strlen(data);
    output_length = 4 * ((input_length + 2) / 3);
    char *encoded_data = elysian_mem_malloc(server, output_length + 1, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
    if (encoded_data == NULL){
		return NULL;
	}

    for (i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for (i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[output_length - 1 - i] = '=';

	encoded_data[output_length] = '\0';
	
    return encoded_data;
}
#endif


static const char decoding_table[256] = {
								0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, //gap: ctrl chars
								0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, //gap: ctrl chars
								0,0,0,0,0,0,0,0,0,0,0,           //gap: spc,!"#$%'()*
								62,                   // +
								 0, 0, 0,             // gap ,-.
								63,                   // /
								52, 53, 54, 55, 56, 57, 58, 59, 60, 61, // 0-9
								 0, 0, 0,             // gap: :;<
								99,                   //  = (end padding)
								 0, 0, 0,             // gap: >?@
								 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,
								17,18,19,20,21,22,23,24,25, // A-Z
								 0, 0, 0, 0, 0, 0,    // gap: [\]^_`
								26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,
								43,44,45,46,47,48,49,50,51, // a-z    
								 0, 0, 0, 0,          // gap: {|}~ (and the rest...)
								0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
								0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
								0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
								0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

elysian_err_t base64_decode(elysian_t* server, const char *encoded, char** decoded) {
	uint32_t input_length;
	uint32_t output_length;
	char* decoded_data;
	int i, j;
	uint32_t sextet_a, sextet_b, sextet_c, sextet_d;
	
	*decoded = "";
	input_length = strlen((char*) encoded);
    if ((input_length % 4 != 0) || (input_length == 0))  {
		return ELYSIAN_ERR_FATAL;
	}

    output_length = input_length / 4 * 3;
    if (encoded[input_length - 1] == '=') {output_length--;}
    if (encoded[input_length - 2] == '=') {output_length--;}

    decoded_data = elysian_mem_malloc(server, output_length + 1, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
    if (decoded_data == NULL) {
		return ELYSIAN_ERR_POLL;
	}

    for (i = 0, j = 0; i < input_length;) {
        sextet_a = encoded[i] == '=' ? 0 & i++ : decoding_table[(unsigned char) encoded[i++]];
        sextet_b = encoded[i] == '=' ? 0 & i++ : decoding_table[(unsigned char) encoded[i++]];
        sextet_c = encoded[i] == '=' ? 0 & i++ : decoding_table[(unsigned char) encoded[i++]];
        sextet_d = encoded[i] == '=' ? 0 & i++ : decoding_table[(unsigned char) encoded[i++]];

        uint32_t triple = (sextet_a << 3 * 6)
        + (sextet_b << 2 * 6)
        + (sextet_c << 1 * 6)
        + (sextet_d << 0 * 6);

        if (j < output_length) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < output_length) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < output_length) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
    }

	decoded_data[j] = '\0';
	*decoded = decoded_data;
    return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_http_authenticate(elysian_t* server){
#if 0
	elysian_err_t err;
	char* header_value;
	char user_pass[64 + 1 + 1 + 1];
	char* user_pass_encoded;
	char* user_pass_encoded_recved;
	

	/*
	** user:pass
	*/
	strcpy(user_pass, "");
	strcpy(&user_pass[strlen(user_pass)], username);
	strcpy(&user_pass[strlen(user_pass)], ":");
	strcpy(&user_pass[strlen(user_pass)], password);
	ELYSIAN_ASSERT(strlen(user_pass) < sizeof(user_pass), "");
	if(strcmp(user_pass, ":") == 0){
		return ELYSIAN_ERR_OK;
	}
		
	user_pass_encoded = elysian_http_base64_encode(user_pass);
	if(!user_pass_encoded){
		return ELYSIAN_ERR_POLL;
	}
    ELYSIAN_LOG("computed auth value = '%s'", user_pass_encoded);
    
	err = elysian_http_request_get_header(client, "Authorization" , &header_value);
	if(err != ELYSIAN_ERR_OK){
		elysian_mem_free(server,user_pass_encoded);
        return err;
    }
    ELYSIAN_LOG("header value = '%s'", header_value);
	if(!header_value){
		elysian_mem_free(server, user_pass_encoded);
		return ELYSIAN_ERR_AUTH;
    }else{
		if(strncmp(header_value, "Basic ", 6) != 0){ /* Todo: case insesitive */
			elysian_mem_free(server, header_value);
			elysian_mem_free(server, user_pass_encoded);
			return ELYSIAN_ERR_AUTH;
		}
	}
	
	user_pass_encoded_recved = header_value + 6;
	while(*user_pass_encoded_recved == ' ' && *user_pass_encoded_recved != '\0'){
		user_pass_encoded_recved++;
	}
	ELYSIAN_LOG("recved auth value = '%s'", user_pass_encoded_recved);
    
	if(strcmp(user_pass_encoded, user_pass_encoded_recved) == 0){
		elysian_mem_free(server, header_value);
		elysian_mem_free(server, user_pass_encoded);
		return ELYSIAN_ERR_OK;
	}else{
		elysian_mem_free(server, header_value);
		elysian_mem_free(server, user_pass_encoded);
		return ELYSIAN_ERR_AUTH;
	}
#else
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	char* username;
	char* password;
	uint8_t authenticated;
	char* header_value;
	char* user_pass_encoded;
	char* user_pass_decoded;
	
	if(!server->authentication_cb){
		 return ELYSIAN_ERR_OK;
	}			
	err = elysian_http_request_get_header(server, "Authorization" , &header_value);
	if(err != ELYSIAN_ERR_OK){
		return err;
    }
    ELYSIAN_LOG("header value = '%s'", header_value);
	if(!header_value){
		/*
		** Can this page be accessed without authentication ?
		*/
		authenticated = server->authentication_cb(server, client->httpreq.url, "", "");
		if(authenticated){
			return ELYSIAN_ERR_OK;
		}else{
			return ELYSIAN_ERR_AUTH;
		}
    }else{
		if(strncmp(header_value, "Basic ", 6) != 0){ /* Todo: case insesitive */
			elysian_mem_free(server, header_value);
			return ELYSIAN_ERR_AUTH;
		}
	}
	user_pass_encoded = header_value + 6;
	while(*user_pass_encoded == ' ' && *user_pass_encoded != '\0'){
		user_pass_encoded++;
	}
	ELYSIAN_LOG("recved encoded auth value = '%s'", user_pass_encoded);
	err = base64_decode(server, user_pass_encoded, &user_pass_decoded);
	if(err != ELYSIAN_ERR_OK){
		elysian_mem_free(server, header_value);
		return err;
    }
	ELYSIAN_LOG("recved decoded auth value = '%s'", user_pass_decoded);
	username = user_pass_decoded;
	password = elysian_strstr(user_pass_decoded, ":");
	if(!password){
		elysian_mem_free(server, header_value);
		elysian_mem_free(server, user_pass_decoded);
		return ELYSIAN_ERR_AUTH;
	}
	*password = '\0';
	password++;
	authenticated = server->authentication_cb(server, client->httpreq.url, username, password);
	if(authenticated){
		elysian_mem_free(server, header_value);
		elysian_mem_free(server, user_pass_decoded);
		return ELYSIAN_ERR_OK;
	}else{
		elysian_mem_free(server, header_value);
		elysian_mem_free(server, user_pass_decoded);
		return ELYSIAN_ERR_AUTH;
	}
#endif
}

/* --------------------------------------------------------------------------------------------------------------------------------
| Mime Types
-------------------------------------------------------------------------------------------------------------------------------- */
typedef struct elysian_http_mime_type_t elysian_http_mime_type_t;
struct elysian_http_mime_type_t{
	char* suffix;
	char* mime_type;
};

#define	MIME_TYPE_ENTRY(suffix_arg, mime_type_arg)	{.suffix = suffix_arg, .mime_type = mime_type_arg}
const elysian_http_mime_type_t elysian_http_mime_types[] = {
	MIME_TYPE_ENTRY(".aif", "audio/aiff"),
	MIME_TYPE_ENTRY(".aifc", "audio/aiff"),
	MIME_TYPE_ENTRY(".aiff", "audio/aiff"),
	MIME_TYPE_ENTRY(".avi", "video/avi"),
	MIME_TYPE_ENTRY(".bmp", "image/bmp"),
	MIME_TYPE_ENTRY(".bz", "application/x-bzip"),
	MIME_TYPE_ENTRY(".bz2", "application/x-bzip2"),
	MIME_TYPE_ENTRY(".c", "text/plain"),
	MIME_TYPE_ENTRY(".cc", "text/plain"),
	MIME_TYPE_ENTRY(".class", "application/java"),
	MIME_TYPE_ENTRY(".cpp", "text/x-c"),
	MIME_TYPE_ENTRY(".css", "text/css"),
	MIME_TYPE_ENTRY(".divx", "video/divx"),
	MIME_TYPE_ENTRY(".doc", "application/msword"),
	MIME_TYPE_ENTRY(".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"),
	MIME_TYPE_ENTRY(".fvl", "video/mp4"),
	MIME_TYPE_ENTRY(".gif", "image/gif"),
	MIME_TYPE_ENTRY(".gz", "application/x-gzip"),
	MIME_TYPE_ENTRY(".gzip", "application/x-gzip"),
	MIME_TYPE_ENTRY(".h", "text/plain"),
	MIME_TYPE_ENTRY(".help", "application/x-helpfile"),
	MIME_TYPE_ENTRY(".htm", "text/html"),
	MIME_TYPE_ENTRY(".html", "text/html"),
	MIME_TYPE_ENTRY(".htmls", "text/html"),
	MIME_TYPE_ENTRY(".ico", "image/x-icon"),
	MIME_TYPE_ENTRY(".java", "text/plain"),
	MIME_TYPE_ENTRY(".jpeg", "image/jpeg"),
	MIME_TYPE_ENTRY(".jpg", "image/jpeg"),
	MIME_TYPE_ENTRY(".js", "text/javascript"),
	MIME_TYPE_ENTRY(".log", "text/plain"),
	MIME_TYPE_ENTRY(".m1v", "video/mpeg"),
	MIME_TYPE_ENTRY(".m2a", "audio/mpeg"),
	MIME_TYPE_ENTRY(".m2v", "video/mpeg"),
	MIME_TYPE_ENTRY(".m3u", "audio/x-mpequrl"),
	MIME_TYPE_ENTRY(".mid", "audio/midi"),
	MIME_TYPE_ENTRY(".midi", "audio/midi"),
	MIME_TYPE_ENTRY(".mjpg", "video/x-motion-jpeg"),
	MIME_TYPE_ENTRY(".mov", "video/quicktime"),
	MIME_TYPE_ENTRY(".mp2", "audio/mpeg"),
	MIME_TYPE_ENTRY(".mp3", "audio/mpeg"),
	MIME_TYPE_ENTRY(".mp4", "video/mp4"),
	MIME_TYPE_ENTRY(".mpeg", "video/mpeg"),
	MIME_TYPE_ENTRY(".mpg", "video/mpeg"),
	MIME_TYPE_ENTRY(".mpga", "audio/mpeg"),
	MIME_TYPE_ENTRY(".ogg", "audio/ogg"),
	MIME_TYPE_ENTRY(".pdf", "application/pdf"),
	MIME_TYPE_ENTRY(".png", "image/png"),
	MIME_TYPE_ENTRY(".ppa", "application/vnd.ms-powerpoint"),
	MIME_TYPE_ENTRY(".pps", "application/vnd.ms-powerpoint"),
	MIME_TYPE_ENTRY(".ppt", "application/vnd.ms-powerpoint"),
	MIME_TYPE_ENTRY(".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"),
	MIME_TYPE_ENTRY(".ps", "application/postscript"),
	MIME_TYPE_ENTRY(".py", "text/x-script.phyton"),
	MIME_TYPE_ENTRY(".pyc", "application/x-bytecode.python"),
	MIME_TYPE_ENTRY(".qif", "image/x-quicktime"),
	MIME_TYPE_ENTRY(".s", "text/x-asm"),
	MIME_TYPE_ENTRY(".sh", "application/x-sh"),
	MIME_TYPE_ENTRY(".shtml", "text/html"),
	MIME_TYPE_ENTRY(".swf", "application/x-shockwave-flash"),
	MIME_TYPE_ENTRY(".tar", "application/x-tar"),
	MIME_TYPE_ENTRY(".tcl", "application/x-tcl"),
	MIME_TYPE_ENTRY(".text", "text/plain"),
	MIME_TYPE_ENTRY(".tgz", "application/x-compressed"),
	MIME_TYPE_ENTRY(".tif", "image/tiff"),
	MIME_TYPE_ENTRY(".tiff", "image/tiff"),
	MIME_TYPE_ENTRY(".txt", "text/plain"),
	MIME_TYPE_ENTRY(".wav", "audio/wav"),
	MIME_TYPE_ENTRY(".wma", "audio/x-ms-wma"),
	MIME_TYPE_ENTRY(".wmv", "video/x-ms-wmv"),
	MIME_TYPE_ENTRY(".word", "application/msword"),
	MIME_TYPE_ENTRY(".xls", "application/vnd.ms-excel"),
	MIME_TYPE_ENTRY(".xlt", "application/vnd.ms-excel"),
	MIME_TYPE_ENTRY(".xml", "text/xml"),
	MIME_TYPE_ENTRY(".zip", "application/zip"),
};
#undef MIME_TYPE_ENTRY

char* elysian_http_get_mime_type(char* uri){
	uint32_t i;
	uint16_t suffix_len;
	uint16_t uri_len = strlen(uri);
	for(i = 0; i < sizeof(elysian_http_mime_types)/ sizeof(elysian_http_mime_types[0]); i++){
		suffix_len = strlen(elysian_http_mime_types[i].suffix);
		if(uri_len > suffix_len){
			if(strcasecmp(&uri[uri_len - suffix_len], elysian_http_mime_types[i].suffix) == 0){
				return elysian_http_mime_types[i].mime_type;
			}
		}
	}
	return "application/octet-stream";
}
