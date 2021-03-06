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
	elysian_mvc_controller_def_t* controller_def;
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
	
	controller_def = elysian_mvc_controller_def_get(server, client->httpreq.url, client->httpreq.method);
	if (!controller_def) {
		if ((client->httpreq.method != ELYSIAN_HTTP_METHOD_GET) && (client->httpreq.method != ELYSIAN_HTTP_METHOD_HEAD)) {
			/* Only GET/HEAD requests can be served without controller */
			elysian_set_fatal_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_405);
			err = ELYSIAN_ERR_FATAL;
			goto handle_error;
		}
	}
	
	/*
	** Get Transfer-Encoding
	*/
	err = elysian_http_request_get_header(server, "Transfer-Encoding" , &header_value);
	if(err != ELYSIAN_ERR_OK){
		goto handle_error;
	}
	if(!header_value){
		client->httpreq.transfer_encoding = ELYSIAN_HTTP_TRANSFER_ENCODING_IDENTITY;
	}else{
		if(elysian_strcasecmp(header_value, "chunked") == 0){
			ELYSIAN_LOG("Chunked Request!");
			client->httpreq.transfer_encoding = ELYSIAN_HTTP_TRANSFER_ENCODING_CHUNKED;
		} else {
			/*
			** Unknown encoding. Normally we should respond with status 501 and close the connection
			*/
			client->httpreq.transfer_encoding = ELYSIAN_HTTP_TRANSFER_ENCODING_IDENTITY;
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
				if (strlen(header_value_tmp)) {
					err = elysian_str2uint(header_value_tmp, &client->httpreq.range_start);
					if (err != ELYSIAN_ERR_OK) {
						elysian_mem_free(server, header_value);
						err = ELYSIAN_ERR_FATAL;
						goto handle_error;
					}
				} else {
					elysian_mem_free(server, header_value);
					err = ELYSIAN_ERR_FATAL;
					goto handle_error;
				}
				ELYSIAN_LOG("Range start is '%u'", client->httpreq.range_start);
				header_value_tmp = barrier + 1;
				if(strlen(header_value_tmp)){
					err = elysian_str2uint(header_value_tmp, &client->httpreq.range_end);
					if (err != ELYSIAN_ERR_OK) {
						elysian_mem_free(server, header_value);
						err = ELYSIAN_ERR_FATAL;
						goto handle_error;
					}
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
		
		if((controller_def) && (controller_def->flags & ELYSIAN_MVC_CONTROLLER_FLAG_USE_EXT_FS)) {
			max_http_body_size = ELYSIAN_MAX_HTTP_BODY_SIZE_KB_EXT;
		} else {
			max_http_body_size = ELYSIAN_MAX_MEMORY_USAGE_KB > ELYSIAN_MAX_HTTP_BODY_SIZE_KB_RAM ? ELYSIAN_MAX_HTTP_BODY_SIZE_KB_RAM : ELYSIAN_MAX_MEMORY_USAGE_KB;
		}
		if(client->httpreq.body_len > max_http_body_size * 1024){
			elysian_set_fatal_http_status_code(server, ELYSIAN_HTTP_STATUS_CODE_413);
			err = ELYSIAN_ERR_FATAL;
			goto handle_error;
		}
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
		if(elysian_strcasecmp(header_value, "application/x-www-form-urlencoded") == 0){
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
			
			client->httpreq.multipart_boundary = elysian_mem_malloc(server, strlen(multipart_boundary) + 1);
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
	
	/*
	** Check for connection type
	*/
	err = elysian_http_request_get_header(server, "Connection" , &header_value);
	if (err != ELYSIAN_ERR_OK) {
		goto handle_error;
	}
	if (!header_value) {
		client->httpreq.connection = ELYSIAN_HTTP_CONNECTION_CLOSE;
	} else {
		if (elysian_strcasecmp(header_value, "Keep-Alive") == 0) {
			client->httpreq.connection = ELYSIAN_HTTP_CONNECTION_KEEPALIVE;
		} else if (elysian_strcasecmp(header_value, "Upgrade") == 0) {
			client->httpreq.connection = ELYSIAN_HTTP_CONNECTION_UPGRADE;
		} else {
			client->httpreq.connection = ELYSIAN_HTTP_CONNECTION_CLOSE;
		}
		elysian_mem_free(server, header_value);
	}
	
	/*
	** Check for connection upgrade
	*/
	if (client->httpreq.connection == ELYSIAN_HTTP_CONNECTION_UPGRADE) {
		err = elysian_http_request_get_header(server, "Upgrade" , &header_value);
		if (err != ELYSIAN_ERR_OK) {
			goto handle_error;
		}
		if (!header_value) {
			client->httpreq.connection_upgrade = ELYSIAN_HTTP_CONNECTION_UPGRADE_NO;
		} else {
			if (elysian_strcasecmp(header_value, "websocket") == 0) {
				client->httpreq.connection_upgrade = ELYSIAN_HTTP_CONNECTION_UPGRADE_WEBSOCKET;
			}
			elysian_mem_free(server, header_value);
		}
	} else {
		client->httpreq.connection_upgrade = ELYSIAN_HTTP_CONNECTION_UPGRADE_NO;
	}
	
	/*
	** Get WebSocket version
	*/
	if ((client->httpreq.connection == ELYSIAN_HTTP_CONNECTION_UPGRADE) && (client->httpreq.connection_upgrade == ELYSIAN_HTTP_CONNECTION_UPGRADE_WEBSOCKET)) {
		err = elysian_http_request_get_header(server, "Sec-WebSocket-Version" , &header_value);
		if (err != ELYSIAN_ERR_OK) {
			goto handle_error;
		}
		if (!header_value) {
			client->httpreq.websocket_version = ELYSIAN_WEBSOCKET_VERSION_NA;
		} else {
			if (elysian_strcasecmp(header_value, "13") == 0) {
				client->httpreq.websocket_version = ELYSIAN_WEBSOCKET_VERSION_13;
			} else {
				client->httpreq.websocket_version = ELYSIAN_WEBSOCKET_VERSION_NA;
			}
			elysian_mem_free(server, header_value);
		}
	} else {
		client->httpreq.websocket_version = ELYSIAN_WEBSOCKET_VERSION_NA;
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
	*uri = elysian_mem_malloc(server, (index1 - index0 + 1) + 1 /* '\0' */ + strlen(ELYSIAN_FS_INDEX_HTML_VRT_ROOT) + 10 /* index.html */);
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
	expanded_header_name = elysian_mem_malloc(server, 2 /* \r\n */ + strlen(header_name) + 1 /* '\0' */);
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
	*header_value = elysian_mem_malloc(server, (index1 - index0 + 1) + 1 /* '\0' */);
	if(*header_value == NULL){
		return ELYSIAN_ERR_POLL;
	}
	elysian_strncpy_file(server, &client->httpreq.headers_file, index0, *header_value, index1 - index0 + 1);
	elysian_str_trim(server, *header_value, ": ", " ");
	ELYSIAN_LOG("Header '%s' value is '%s'", header_name, *header_value);	  
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_http_request_get_params(elysian_t* server){
	elysian_err_t err;
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_req_param_t* param;
	elysian_req_param_t* param_prev;
	elysian_req_param_t* req_param_next;
	char* param_header;
	uint32_t param_search_index;
	uint32_t index0;
	uint32_t index1;
	elysian_file_t* param_file;
	uint32_t param_header_index;
	uint32_t param_header_len;
	uint32_t max_index;
	char* div1;
	char* div2;
	char* div3;
	uint8_t stop;
		
	param_header = NULL;
	param_prev = NULL;
	
	ELYSIAN_ASSERT(client->httpreq.params == NULL);
	
	/* ------------------------------------------------------------------------------------------------------------------
	** First add the HTTP request header/body params
	** ---------------------------------------------------------------------------------------------------------------- */

	/*
	** HTTP headers parameter
	*/
	param = elysian_mem_malloc(server, sizeof(elysian_req_param_t));
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
	param->name = elysian_mem_malloc(server, strlen(ELYSIAN_MVC_PARAM_HTTP_HEADERS) + 1);
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
	param = elysian_mem_malloc(server, sizeof(elysian_req_param_t));
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
	param->name = elysian_mem_malloc(server, strlen(ELYSIAN_MVC_PARAM_HTTP_BODY) + 1);
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
	
	
	/* ------------------------------------------------------------------------------------------------------------------
	** If this is a multipart request, read the parameters from multipart ISP 
	** (analyzed on the fly during HTTP body reception)
	** ---------------------------------------------------------------------------------------------------------------- */
	if (client->httpreq.content_type == ELYSIAN_HTTP_CONTENT_TYPE_MULTIPART__FORM_DATA) {
		/*
		** Append multipart params
		*/
		param = client->httpreq.params;
		if (param) {
			while (param) {
				if(!param->next) {
					param->next = client->isp.multipart.params;
					break;
				}
				param = param->next;
			}
		} else {
			client->httpreq.params = client->isp.multipart.params;
		}
		client->isp.multipart.params = NULL;
		return ELYSIAN_ERR_OK;
	}

	/* ------------------------------------------------------------------------------------------------------------------
	** Not a multipart request, but rather an HTTP POST (params on the HTTP body) or GET (params on the HTTP headers)
	** Analaze the parameters now.
	** ---------------------------------------------------------------------------------------------------------------- */
	
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
	if(client->httpreq.content_type == ELYSIAN_HTTP_CONTENT_TYPE_APPLICATION__X_WWW_FORM_URLENCODED){
		param_file = &client->httpreq.body_file;
		err = elysian_fs_fsize(server, param_file, &max_index);
		err = elysian_fs_fsize(server, param_file, &max_index);
		if(err != ELYSIAN_ERR_OK){
			goto handle_error;
		}
		
		if(max_index == 0) {
			/*
			** No parameters exist
			*/
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
		param_file = &client->httpreq.headers_file;
		err = elysian_strstr_file(server, param_file, 0, " HTTP/", "", 0, &index0, &index1);
		if(err != ELYSIAN_ERR_OK){
			goto handle_error;
		}
		if(index0 == ELYSIAN_INDEX_OOB32){
			err = ELYSIAN_ERR_FATAL;
			goto handle_error;
		} else {
			max_index = index0;
			if(max_index == 0) {
				/*
				** No parameters exist
				*/
				err = ELYSIAN_ERR_OK;
				goto handle_error;
			} else {
				max_index--;
				err = elysian_strstr_file(server, param_file, 0, "?", "", 0, &index0, &index1);
				if(err != ELYSIAN_ERR_OK){
					goto handle_error;
				}
				if((index0 == ELYSIAN_INDEX_OOB32) || (index0 > max_index)){
					/*
					** Not found, no parts exist
					*/
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

	stop = 0;
	while (!stop) {
		if (!param_prev) {
			/*
			** No parameters yet detected
			*/
		} else {
			/*
			** At least one parameter detected
			*/
			//ELYSIAN_LOG("param_search_index = %u + %u + %u + %u = %u", (unsigned int) param_prev->data_index, (unsigned int) param_prev->data_size, (unsigned int) strlen(div2), (unsigned int)strlen(div3), (unsigned int)(param_prev->data_index + param_prev->data_size + strlen(div2) + strlen(div3)));
			param_search_index = param_prev->data_index + param_prev->data_size + strlen(div2) + strlen(div3);
		}
		err = elysian_strstr_file(server, param_file, param_search_index, div1, div3, 0, &index0, &index1);
		if(err != ELYSIAN_ERR_OK){
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
			** No more parameters
			*/
			err = ELYSIAN_ERR_OK;
			goto handle_error;
		}else{
			if(index1 == ELYSIAN_INDEX_OOB32){
				/*
				** This is the last param
				*/
				if(client->httpreq.content_type == ELYSIAN_HTTP_CONTENT_TYPE_APPLICATION__X_WWW_FORM_URLENCODED){
					err = elysian_fs_fsize(server, param_file, &index1);
				} else {
					err = elysian_strstr_file(server, param_file, param_search_index, div1, " ", 0, &index0, &index1);
				}
				if(err != ELYSIAN_ERR_OK){
					goto handle_error;
				}
				if(index0 > max_index + 1) {
					index0 = ELYSIAN_INDEX_OOB32;
				}
				if(index1 > max_index + 1) {
					index1 = ELYSIAN_INDEX_OOB32;
				}
				if ((index0 == ELYSIAN_INDEX_OOB32) || (index1 == ELYSIAN_INDEX_OOB32)){
					err = ELYSIAN_ERR_FATAL;
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
		
		/*
		** Proces new param
		*/
		param = elysian_mem_malloc(server, sizeof(elysian_req_param_t));
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
		
		if (param_prev) {
			param_header_index = param_prev->data_index + param_prev->data_size + strlen(div2) + strlen(div3);
		} else {
			param_header_index = param_search_index;
		}
		param_header_len = (index0) - (param_header_index);
		
		/*
		** Read the header block 
		*/
		param_header = elysian_mem_malloc(server, param_header_len + 1);
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
		param->name = elysian_mem_malloc(server, strlen(param_header) + 1);
		if(!param->name){
			err = ELYSIAN_ERR_POLL;
			goto handle_error;
		}
		strcpy(param->name, param_header);

		elysian_mem_free(server, param_header);
		param_header = NULL;

		param_prev = param;
	};

handle_error:
	
	if(err != ELYSIAN_ERR_OK){
		if(param_header) {
			elysian_mem_free(server, param_header);
		}
		while(client->httpreq.params){
			req_param_next = client->httpreq.params->next;
			if(client->httpreq.params->name){
				elysian_mem_free(server, client->httpreq.params->name);
			}
			if(client->httpreq.params->filename){
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
	uint32_t resource_size;
	
	client->httpresp.cbuf_index = 0;
				
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
	
	if (client->mvc.connection == ELYSIAN_HTTP_CONNECTION_KEEPALIVE) {
		err = elysian_http_add_response_header_line(server, "Connection", "keep-alive");
	} else if (client->mvc.connection == ELYSIAN_HTTP_CONNECTION_UPGRADE) {
		err = elysian_http_add_response_header_line(server, "Connection", "upgrade");
			if (client->mvc.connection_upgrade == ELYSIAN_HTTP_CONNECTION_UPGRADE_WEBSOCKET) {
				err = elysian_http_add_response_header_line(server, "Upgrade", "websocket");
			}
	} else {
		err = elysian_http_add_response_header_line(server, "Connection", "close");
	}
	if(err != ELYSIAN_ERR_OK){
		return err;
	}

	if(client->mvc.status_code == ELYSIAN_HTTP_STATUS_CODE_401){
		/* WWW-Authenticate: Basic realm="My Server" */
		elysian_sprintf(header_value, "Basic realm=\"%s\"", ELYSIAN_SERVER_NAME);
		err = elysian_http_add_response_header_line(server, "WWW-Authenticate", header_value);
		if(err != ELYSIAN_ERR_OK){
			return err;
		}
	}
	
	if (client->mvc.transfer_encoding == ELYSIAN_HTTP_TRANSFER_ENCODING_IDENTITY) {
		err = elysian_http_add_response_header_line(server, "Accept-Ranges", "bytes");
		if(err != ELYSIAN_ERR_OK){
			return err;
		}
		
		if (client->mvc.status_code == ELYSIAN_HTTP_STATUS_CODE_206) {
			err = elysian_resource_size(server, &resource_size);
			if(err != ELYSIAN_ERR_OK){
				return err;
			}
			
			/* Content-Range: bytes 0-64657026/64657027 */
			elysian_sprintf(header_value, "bytes %u-%u/%u", client->mvc.range_start, client->mvc.range_end, resource_size);
			err = elysian_http_add_response_header_line(server, "Content-Range", header_value);
			if(err != ELYSIAN_ERR_OK){
				return err;
			}
		} 
		
		elysian_sprintf(header_value, "%u", client->mvc.content_length);
		err = elysian_http_add_response_header_line(server, "Content-Length", header_value);
		if(err != ELYSIAN_ERR_OK){
			return err;
		}
	} else if (client->mvc.transfer_encoding == ELYSIAN_HTTP_TRANSFER_ENCODING_CHUNKED) {
		err = elysian_http_add_response_header_line(server, "Transfer-Encoding", "chunked");
		if(err != ELYSIAN_ERR_OK){
			return err;
		}
	} else {
		return ELYSIAN_ERR_FATAL;
	}
	
	//if(client->httpresp.body_size > 0){
		err = elysian_http_add_response_header_line(server, "Content-Type", elysian_http_get_mime_type(client->mvc.view));
		if(err != ELYSIAN_ERR_OK){
			return err;
		}
	//}
	
#if 1 //#ifdef ELYSIAN_HTTP_CACHE_DISABLED
	/*
	** no-store : don't cache this to disk, caching to RAM is stil perimtted [for security reasons]
	** no-cache : don't cache, use ETAG for revalidation
	** max-age  : in seconds, for how long the resource can be cached
	*/
	//if(client->httpresp.body_size > 0){
		err = elysian_http_add_response_header_line(server, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
		if(err != ELYSIAN_ERR_OK){
			return err;
		}
	//}
#else
		
#endif

	/*
	** Set the HTTP headers set by the application layer
	*/
	elysian_mvc_httpresp_header_t* httpresp_header;
	httpresp_header = client->mvc.httpresp_headers;
	while (httpresp_header) {
		err = elysian_http_add_response_header_line(server, httpresp_header->header, httpresp_header->value);
		if(err != ELYSIAN_ERR_OK){
			return err;
		}
		httpresp_header = httpresp_header->next;
	};
	
	/*
	** Terminate HTTP Response
	*/
	err = elysian_http_add_response_empty_line(server);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}

	printf("[%s]",client->httpresp.cbuf->data);
	
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_http_add_response_status_line(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	char* cbuf_data = (char*) elysian_cbuf_data(client->httpresp.cbuf);
	uint16_t status_line_len = strlen("HTTP/1.1 ") + 3 /* Status code */ + 1 /* Space */ + strlen(elysian_http_get_status_code_msg(client->mvc.status_code)) + 2 /* \r\n */;
	
	if(client->httpresp.cbuf_index + status_line_len + 1 /* '\0' */ < elysian_cbuf_len(client->httpresp.cbuf)){
		elysian_sprintf((char*)cbuf_data, "HTTP/1.1 %u %s\r\n", elysian_http_get_status_code_num(client->mvc.status_code), elysian_http_get_status_code_msg(client->mvc.status_code));
		client->httpresp.cbuf_index = strlen((char*)cbuf_data);
		return ELYSIAN_ERR_OK;
	}else{
		return ELYSIAN_ERR_BUF;
	}
}

elysian_err_t elysian_http_add_response_header_line(elysian_t* server, char* header_name, char* header_value){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	uint16_t header_name_len;
	uint16_t header_value_len;
	char* cbuf_data = (char*) elysian_cbuf_data(client->httpresp.cbuf);
	
	/*
	** Add HTTP Header
	*/
	header_name_len = strlen(header_name);
	header_value_len = strlen(header_value);
	if(client->httpresp.cbuf_index + header_name_len + header_value_len + 4 < elysian_cbuf_len(client->httpresp.cbuf)){
		memcpy(&cbuf_data[client->httpresp.cbuf_index], header_name, header_name_len);
		client->httpresp.cbuf_index += header_name_len;
		memcpy(&cbuf_data[client->httpresp.cbuf_index], ": ", 2);
		client->httpresp.cbuf_index += 2;
		memcpy(&cbuf_data[client->httpresp.cbuf_index], header_value, header_value_len);
		client->httpresp.cbuf_index += header_value_len;
		memcpy(&cbuf_data[client->httpresp.cbuf_index], "\r\n", 2);
		client->httpresp.cbuf_index += 2;
		return ELYSIAN_ERR_OK;
	}else{
		return ELYSIAN_ERR_BUF;
	} 
}


elysian_err_t elysian_http_add_response_empty_line(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	char* cbuf_data = (char*) elysian_cbuf_data(client->httpresp.cbuf);
	
	if(client->httpresp.cbuf_index + 2 + 1 /* '\0' */ < elysian_cbuf_len(client->httpresp.cbuf)){
		memcpy(&cbuf_data[client->httpresp.cbuf_index], "\r\n", 2);
		client->httpresp.cbuf_index += 2;
		cbuf_data[client->httpresp.cbuf_index] = '\0';
		return ELYSIAN_ERR_OK;
	}else{
		return ELYSIAN_ERR_BUF;
	}
}

/* --------------------------------------------------------------------------------------------------------------------------------
| HTTP character escape
-------------------------------------------------------------------------------------------------------------------------------- */
typedef struct elysian_html_escape_chars_t elysian_html_escape_chars_t;
struct elysian_html_escape_chars_t {
	char symbol;
	char* code;
};

const elysian_html_escape_chars_t escape_chars[] = {
	{.symbol = '�', .code= "&#8482;" },
	{.symbol = '�', .code= "&euro;" },
	{.symbol = ' ', .code= "&#160;" /* We use non-braking space. Braking alternative is "&#32;" */ },
	{.symbol = '!', .code= "&#33;" },
	{.symbol = '"', .code= "&#34;" },
	{.symbol = '#', .code= "&#35;" },
	{.symbol = '$', .code= "&#36;" },
	{.symbol = '%', .code= "&#37;" },
	{.symbol = '&', .code= "&#38;" },
	{.symbol = '\'', .code= "&#39;" },
	{.symbol = '(', .code= "&#40;" },
	{.symbol = ')', .code= "&#41;" },
	{.symbol = '*', .code= "&#42;" },
	{.symbol = '+', .code= "&#43;" },
	{.symbol = ',', .code= "&#44;" },
	{.symbol = '-', .code= "&#45;" },
	{.symbol = '.', .code= "&#46;" },
	{.symbol = '/', .code= "&#47;" },
	
	{.symbol = ':', .code= "&#58;" },
	{.symbol = ';', .code= "&#59;" },
	{.symbol = '<', .code= "&#60;" },
	{.symbol = '=', .code= "&#61;" },
	{.symbol = '>', .code= "&#62;" },
	{.symbol = '?', .code= "&#63;" },
	{.symbol = '@', .code= "&#64;" },
	
	{.symbol = '[', .code= "&#91;" },
	{.symbol = '\\', .code= "&#92;" },
	{.symbol = ']', .code= "&#93;" },
	{.symbol = '^', .code= "&#94;" },
	{.symbol = '_', .code= "&#95;" },
	{.symbol = '`', .code= "&#96;" },
	
	{.symbol = '{', .code= "&#123;" },
	{.symbol = '|', .code= "&#124;" },
	{.symbol = '}', .code= "&#125;" },
	{.symbol = '~', .code= "&#126;" },

	{.symbol = '�', .code= "&#169;" },
	{.symbol = '�', .code= "&#171;" },
	{.symbol = '�', .code= "&#187;" },
};

char* elysian_html_escape(elysian_t* server, char* str) {
	uint32_t i, k;
	uint32_t str_len;
	uint32_t escaped_str_len;
	char* escaped_str;
	
	escaped_str = NULL;
	str_len = strlen(str);

again:
	escaped_str_len = 0;
	for (i = 0; i < str_len; i++) {
		if (escaped_str) {
			escaped_str[escaped_str_len] = str[i];
		}
		escaped_str_len++;
		for (k = 0; k < sizeof(escape_chars)/sizeof(escape_chars[0]); k++) {
			if (str[i] == escape_chars[k].symbol) {
				ELYSIAN_LOG(">>>>>>>>>>>>>>>>>>>>> Escaping character %c", str[i]);
				escaped_str_len--;
				if (escaped_str) {
					strcpy(&escaped_str[escaped_str_len], escape_chars[k].code);
				}
				escaped_str_len += strlen(escape_chars[k].code);
				break;
			}
		}
	}
	
	if (escaped_str) {
		escaped_str[escaped_str_len] = '\0';
		return escaped_str;
	}
	
	escaped_str = elysian_mem_malloc(server, escaped_str_len + 1);
	if (!escaped_str) {
		return NULL;
	} else {
		goto again;
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
	const elysian_http_method_e id;
	const char* name;
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
			return (char*) elysian_http_methods[i].name;
		}
	}
	return "";
}

elysian_http_method_e elysian_http_get_method_id(char* method_name){
	uint32_t i;
	for(i = 0; i < sizeof(elysian_http_methods)/sizeof(elysian_http_methods[0]); i++){
		if(strcmp(elysian_http_methods[i].name, method_name) == 0){
			return (elysian_http_method_e) elysian_http_methods[i].id;
		}
	}
	return ELYSIAN_HTTP_METHOD_NA;
}

/* --------------------------------------------------------------------------------------------------------------------------------
| HTTP status codes
-------------------------------------------------------------------------------------------------------------------------------- */
const elysian_http_status_code_t elysian_http_status_codes[] = {
	[ELYSIAN_HTTP_STATUS_CODE_100] = {
		.code_num = 100,
		.code_msg = "Continue",
		.code_body = ""
		},
	[ELYSIAN_HTTP_STATUS_CODE_101] = {
			.code_num = 101,
			.code_msg = "Switching Protocols",
			.code_body = ""
		},	
	[ELYSIAN_HTTP_STATUS_CODE_200] = {
		.code_num = 200,
		.code_msg = "OK",
		.code_body = ""
		},
	[ELYSIAN_HTTP_STATUS_CODE_201] = {
			.code_num = 201,
			.code_msg = "Created",
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
		.code_body = ""
		},
	[ELYSIAN_HTTP_STATUS_CODE_400] = {
		.code_num = 400,
		.code_msg = "Bad Request",
		.code_body = "Error 400: Bad Request"
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
	ELYSIAN_ASSERT(status_code < ELYSIAN_HTTP_STATUS_CODE_MAX);
	return elysian_http_status_codes[status_code].code_num;
}

char* elysian_http_get_status_code_msg(elysian_http_status_code_e status_code){
	ELYSIAN_ASSERT(status_code < ELYSIAN_HTTP_STATUS_CODE_MAX);
	return (char*) elysian_http_status_codes[status_code].code_msg;
}

#if 0
char* elysian_http_get_status_code_body(elysian_http_status_code_e status_code){
	ELYSIAN_ASSERT(status_code < ELYSIAN_HTTP_STATUS_CODE_MAX);
	return elysian_http_status_codes[status_code].code_body;
}
#endif
/* --------------------------------------------------------------------------------------------------------------------------------
| Basic Access Authentication
-------------------------------------------------------------------------------------------------------------------------------- */
#if 1
static const char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
								'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
								'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
								'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
								'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
								'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
								'w', 'x', 'y', 'z', '0', '1', '2', '3',
								'4', '5', '6', '7', '8', '9', '+', '/'};
static const int mod_table[] = {0, 2, 1};

char* elysian_http_base64_encode(elysian_t* server, char *data) {
	uint32_t input_length;
	uint32_t output_length;
	int i, j;
	
	input_length = strlen(data);
	output_length = 4 * ((input_length + 2) / 3);
	char *encoded_data = elysian_mem_malloc(server, output_length + 1);
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
								0,0,0,0,0,0,0,0,0,0,0,		   //gap: spc,!"#$%'()*
								62,				   // +
								 0, 0, 0,			 // gap ,-.
								63,				   // /
								52, 53, 54, 55, 56, 57, 58, 59, 60, 61, // 0-9
								 0, 0, 0,			 // gap: :;<
								99,				   //  = (end padding)
								 0, 0, 0,			 // gap: >?@
								 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,
								17,18,19,20,21,22,23,24,25, // A-Z
								 0, 0, 0, 0, 0, 0,	// gap: [\]^_`
								26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,
								43,44,45,46,47,48,49,50,51, // a-z	
								 0, 0, 0, 0,		  // gap: {|}~ (and the rest...)
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

	decoded_data = elysian_mem_malloc(server, output_length + 1);
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
			if(elysian_strcasecmp(&uri[uri_len - suffix_len], elysian_http_mime_types[i].suffix) == 0){
				return elysian_http_mime_types[i].mime_type;
			}
		}
	}
	return "application/octet-stream";
}
