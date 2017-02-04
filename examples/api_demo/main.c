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

#include "fs_rom/fsdata.c"



uint8_t authentication_cb(elysian_t* server, char* url, char* username, char* password){

	ELYSIAN_LOG("[[ authentication_cb ]]");
	ELYSIAN_LOG("URL = %s, username = %s, password = %s", url, username, password);
	
	if(strcmp(url, "/fs_rom/basic_access_auth.html") == 0) {
		if((strcmp(username, "admin") == 0) && (strcmp(password, "admin") == 0)){
			return 1;
		} else {
			return 0; // wrong credentials
		}
	} 
	
	return 1; // all other pages authenticated by default
}

elysian_err_t controller_dynamic_page_html(elysian_t* server){
	//elysian_client_t* client = elysian_mvc_client(server);
	char attr_value[32];
    elysian_err_t err;

    ELYSIAN_LOG("[[ %s ]]", __func__);
    
	elysian_sprintf(attr_value, "%u", elysian_time_now());
	err = elysian_mvc_attribute_set(server, "attr_timestamp", attr_value);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	elysian_sprintf(attr_value, "%u", ELYSIAN_MAX_CLIENTS_NUM);
	err = elysian_mvc_attribute_set(server, "attr_ELYSIAN_MAX_CLIENTS_NUM", attr_value);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	elysian_sprintf(attr_value, "%u", ELYSIAN_MAX_MEMORY_USAGE_KB);
	err = elysian_mvc_attribute_set(server, "attr_ELYSIAN_MAX_MEMORY_USAGE_KB", attr_value);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	elysian_sprintf(attr_value, "%u", ELYSIAN_MAX_HTTP_BODY_SIZE_KB_RAM);
	err = elysian_mvc_attribute_set(server, "attr_ELYSIAN_MAX_HTTP_BODY_SIZE_KB_RAM", attr_value);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	elysian_sprintf(attr_value, "%u", ELYSIAN_MAX_HTTP_BODY_SIZE_KB_EXT);
	err = elysian_mvc_attribute_set(server, "attr_ELYSIAN_MAX_HTTP_BODY_SIZE_KB_DISK", attr_value);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
    err = elysian_mvc_view_set(server, "/fs_rom/dynamic_page.html");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
    return ELYSIAN_ERR_OK;
}

elysian_err_t controller_form_get(elysian_t* server){
	//elysian_client_t* client = elysian_mvc_client(server);
    char* str1;
	char* str2;
	char* str3;
	char* str4;
    uint8_t param_found;
    elysian_err_t err;
    char msg[256];
	
    ELYSIAN_LOG("[[ %s ]]", __func__);
    
    err = elysian_mvc_param_get_str(server, "param1", &str1, &param_found);
    if (err != ELYSIAN_ERR_OK){ 
        return err;
    }
	if (!param_found) {
		/* Parameter was not found */
	} else {
		/* Parameter found, data hold the data, datasize the size */
	}
	
    ELYSIAN_LOG("param1 = %s", str1);
    
    err = elysian_mvc_param_get_str(server, "param2", &str2, &param_found);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
    ELYSIAN_LOG("param2 = %s", str2);
    
	err = elysian_mvc_param_get_str(server, "param3", &str3, &param_found);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
    ELYSIAN_LOG("param3 = %s", str3);
	
	err = elysian_mvc_param_get_str(server, "param4", &str4, &param_found);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
    ELYSIAN_LOG("param3 = %s", str3);
	
	elysian_sprintf(msg, "param1 value was '%s'<br>param2 value was '%s'<br>param3 value was '%s'<br>param4 value was '%s'<br>", str1, str2, str3, str4);
	ELYSIAN_LOG("MSG = %s", msg);
	
	err = elysian_mvc_attribute_set(server, "response_message", msg);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
    err = elysian_mvc_view_set(server, "/fs_rom/form_get.html");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
    
    return ELYSIAN_ERR_OK;
}

elysian_err_t controller_form_get_html(elysian_t* server){
	//elysian_client_t* client = elysian_mvc_client(server);
    elysian_err_t err;
	
    ELYSIAN_LOG("[[ %s ]]", __func__);

	err = elysian_mvc_attribute_set(server, "response_message", "");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
    err = elysian_mvc_view_set(server, "/fs_rom/form_get.html");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
    
    return ELYSIAN_ERR_OK;
}


elysian_err_t controller_form_post(elysian_t* server){
	//elysian_client_t* client = elysian_mvc_client(server);
    char* str1;
	char* str2;
	char* str3;
	char* str4;
    uint8_t param_found;
    elysian_err_t err;
    char msg[256];
	
    ELYSIAN_LOG("[[ %s ]]", __func__);
    
    err = elysian_mvc_param_get_str(server, "param1", &str1, &param_found);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
    ELYSIAN_LOG("param1 = %s", str1);
    
    err = elysian_mvc_param_get_str(server, "param2", &str2, &param_found);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
    ELYSIAN_LOG("param2 = %s", str2);
    
	err = elysian_mvc_param_get_str(server, "param3", &str3, &param_found);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
    ELYSIAN_LOG("param3 = %s", str3);
	
	err = elysian_mvc_param_get_str(server, "param4", &str4, &param_found);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
    ELYSIAN_LOG("param3 = %s", str3);
	
	elysian_sprintf(msg, "param1 value was '%s'<br>param2 value was '%s'<br>param3 value was '%s'<br>param4 value was '%s'<br>", str1, str2, str3, str4);
	ELYSIAN_LOG("MSG = %s", msg);
	
	err = elysian_mvc_attribute_set(server, "response_message", msg);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
    err = elysian_mvc_view_set(server, "/fs_rom/form_post.html");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
    
    return ELYSIAN_ERR_OK;
}

elysian_err_t controller_form_post_html(elysian_t* server){
	//elysian_client_t* client = elysian_mvc_client(server);
    elysian_err_t err;
	
    ELYSIAN_LOG("[[ %s ]]", __func__);

	err = elysian_mvc_attribute_set(server, "response_message", "");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
    err = elysian_mvc_view_set(server, "/fs_rom/form_post.html");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }

    return ELYSIAN_ERR_OK;
}



elysian_err_t controller_file_upload(elysian_t* server){
	//elysian_client_t* client = elysian_mvc_client(server);
	uint32_t param_file1_size;
	char max_upload_size[16];
	char* param1_data;
    uint8_t param_found;
	elysian_req_param_t param_file1;
	uint32_t read_size;
    uint8_t file1_data[128];
	char data[256];
    elysian_err_t err;
	char* requested_url;
	char* param_file1_filename;
	
    ELYSIAN_LOG("[[ %s ]]", __func__);
    
	/*
	** Set the MVC view to be sent to the client
	** Check if this was called from a ROM or DISK page
	*/
	err = elysian_mvc_httpreq_url_get(server, &requested_url);
	if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	/*
	** Set the value to the attr_max_upload_size attribute
	*/
	if (strcmp(requested_url, "/fs_rom/file_upload_ram_controller") == 0) {
		elysian_sprintf(max_upload_size, "%u", ELYSIAN_MAX_HTTP_BODY_SIZE_KB_RAM);
	} else {
		// requested URL was "file_upload_ext_controller"
		elysian_sprintf(max_upload_size, "%u", ELYSIAN_MAX_HTTP_BODY_SIZE_KB_EXT);
	}
	err = elysian_mvc_attribute_set(server, "attr_max_upload_size", max_upload_size);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	/*
	** Read the complete param1 parameter as string and just print it..
	*/
    err = elysian_mvc_param_get_str(server, "param1", &param1_data, &param_found);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
    ELYSIAN_LOG("param1 = %s", param1_data);
    
	/*
	** Retrieve the file1 parameter
	*/
	err = elysian_mvc_param_get(server, "file1", &param_file1);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	/*
	** Read the first bytes of the file1 parameter
	*/
	err = elysian_mvc_param_read(server, &param_file1, 0, file1_data, sizeof(file1_data) - 1, &read_size);
	if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	file1_data[read_size] = '\0';
	
	/*
	** Set the value to the attr_uploaded_file_size attribute
	*/
	err = elysian_mvc_param_size(server, &param_file1, &param_file1_size);
	if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	err = elysian_mvc_param_filename(server, &param_file1, &param_file1_filename);
	if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	elysian_sprintf(data, "<b>The size of the uploaded file (%s) was %u bytes.</b><br>param1 value was '%s'. <br><br>", param_file1_filename, param_file1_size, param1_data);
	err = elysian_mvc_attribute_set(server, "attr_uploaded_file_size", data);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	/*
	** Set the value to the attr_uploaded_file_data attribute
	*/
	elysian_sprintf(data, "<b>The first %u bytes of the uploaded file were:</b><br>'%s'", read_size, file1_data);
	err = elysian_mvc_attribute_set(server, "attr_uploaded_file_data", (char*) data);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	if (strcmp(requested_url, "/fs_rom/file_upload_ram_controller") == 0) {
		err = elysian_mvc_view_set(server, "/fs_rom/file_upload_ram.html");
	} else {
		// requested URL was "file_upload_ext_controller"
		err = elysian_mvc_view_set(server, "/fs_ext/file_upload_ext.html");
	}

    return ELYSIAN_ERR_OK;
}

elysian_err_t controller_file_upload_html(elysian_t* server){
	//elysian_client_t* client = elysian_mvc_client(server);
	char* requested_url;
	char max_upload_size[32];
    elysian_err_t err;

    ELYSIAN_LOG("[[ %s ]]", __func__);
    
	/*
	** Set the MVC view to be sent to the client
	** Check if this was called from a ROM or DISK page
	*/
	err = elysian_mvc_httpreq_url_get(server, &requested_url);
	if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	/*
	** Set the value to the attr_max_upload_size attribute
	*/
	if (strcmp(requested_url, "/fs_rom/file_upload_ram.html") == 0) {
		elysian_sprintf(max_upload_size, "%u", ELYSIAN_MAX_HTTP_BODY_SIZE_KB_RAM);
	} else {
		// requested URL was "file_upload_ext_controller"
		elysian_sprintf(max_upload_size, "%u", ELYSIAN_MAX_HTTP_BODY_SIZE_KB_EXT);
	}
	err = elysian_mvc_attribute_set(server, "attr_max_upload_size", max_upload_size);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }

	err = elysian_mvc_attribute_set(server, "attr_uploaded_file_size", "");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	err = elysian_mvc_attribute_set(server, "attr_uploaded_file_data", "");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	if (strcmp(requested_url, "/fs_rom/file_upload_ram.html") == 0) {
		err = elysian_mvc_view_set(server, "/fs_rom/file_upload_ram.html");
	} else {
		err = elysian_mvc_view_set(server, "/fs_ext/file_upload_ext.html");
	}
	
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
    return ELYSIAN_ERR_OK;
}


elysian_err_t controller_http_request_exposure_html(elysian_t* server){
	//elysian_client_t* client = elysian_mvc_client(server);
    elysian_err_t err;

    ELYSIAN_LOG("[[ %s ]]", __func__);
    
	err = elysian_mvc_attribute_set(server, "attr_http_request_headers", "(Not yet submitted)");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }

	err = elysian_mvc_attribute_set(server, "attr_http_request_body", "(Not yet submitted)");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }

	err = elysian_mvc_attribute_set(server, "attr_http_request_url", "(Not yet submitted)");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	err = elysian_mvc_attribute_set(server, "attr_http_request_header", "(Not yet submitted)");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	err = elysian_mvc_view_set(server, "/fs_rom/http_request_exposure.html");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
    return ELYSIAN_ERR_OK;
}

elysian_err_t controller_http_request_exposure(elysian_t* server){
	//elysian_client_t* client = elysian_mvc_client(server);
	uint8_t param_found;
	char* str1;
	char* str2;
	char* str3;
	char* str4;
    elysian_err_t err;

    ELYSIAN_LOG("[[ %s ]]", __func__);
    
	err = elysian_mvc_param_get_str(server, ELYSIAN_MVC_PARAM_HTTP_HEADERS, &str1, &param_found);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }

	err = elysian_mvc_param_get_str(server, ELYSIAN_MVC_PARAM_HTTP_BODY, &str2, &param_found);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	elysian_mvc_httpreq_url_get(server, &str3);
	if(err != ELYSIAN_ERR_OK){ 
        return err;
    }

	err = elysian_mvc_httpreq_header_get(server, "user-agent", &str4);
	if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	if (str3 == NULL) {
		/* The requested header was not found, add a custom string */
		str3 = "HTTP header not found!";
	}
	
	err = elysian_mvc_attribute_set(server, "attr_http_request_headers", str1);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }

	err = elysian_mvc_attribute_set(server, "attr_http_request_body", str2);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	err = elysian_mvc_attribute_set(server, "attr_http_request_url", str3);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	err = elysian_mvc_attribute_set(server, "attr_http_request_header", str4);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	err = elysian_mvc_view_set(server, "/fs_rom/http_request_exposure.html");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
    return ELYSIAN_ERR_OK;
}



elysian_err_t controller_redirected_page1_html(elysian_t* server){
	//elysian_client_t* client = elysian_mvc_client(server);
	char* str1;
	uint8_t param_found;
    elysian_err_t err;

    ELYSIAN_LOG("[[ %s ]]", __func__);
    
	err = elysian_mvc_param_get_str(server, "redirection_message", &str1, &param_found);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
    ELYSIAN_LOG("param1 = %s", str1);
	
	err = elysian_mvc_attribute_set(server, "redirection_message", str1);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
    err = elysian_mvc_view_set(server, "/fs_rom/redirected_page1.html");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
    return ELYSIAN_ERR_OK;
}

elysian_err_t controller_redirected_page0_html(elysian_t* server){
	//elysian_client_t* client = elysian_mvc_client(server);
	char httpresp_header_value[150];
	char hostname[64];
    elysian_err_t err;

    ELYSIAN_LOG("[[ %s ]]", __func__);
    
	err = elysian_os_hostname_get(hostname);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	
	elysian_sprintf(httpresp_header_value, "http://%s:%u%s", hostname, server->listening_port, "/fs_rom/redirected_page1.html?redirection_message=sample+redirection+message");
	err = elysian_mvc_httpresp_header_add(server, "Location", httpresp_header_value);
	if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
	elysian_mvc_status_code_set(server, ELYSIAN_HTTP_STATUS_CODE_302);
	
	/*
	** Empty HTTP body
	*/
	err = elysian_mvc_view_set(server, NULL);
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
    return ELYSIAN_ERR_OK;
}

void controller_ajax_example_served(elysian_t* server, void* ptr) {
	char* ajax_file_name = (char*) ptr;
	
    ELYSIAN_LOG("[[ %s ]]", __func__);
	
	ELYSIAN_LOG("Removing temporary file '%s'..", ajax_file_name);
	elysian_fs_fremove(server, ajax_file_name);
	elysian_mem_free(server, ajax_file_name);
}

elysian_err_t controller_ajax(elysian_t* server){
	elysian_file_t ajax_file;
	uint32_t actual_write_sz;
	char* ajax_file_name;
	static uint32_t ajax_file_id = 0;
	char * ajax_file_contents = "<%=ajax_attr%>";
	char buffer[128];
	//elysian_client_t* client = elysian_mvc_client(server);
    elysian_err_t err;

    ELYSIAN_LOG("[[ %s ]]", __func__);
    
	ajax_file_name = elysian_mem_malloc(server, 64);
	if(!ajax_file_name) {
		return ELYSIAN_ERR_POLL;
	}
	
	elysian_fs_finit(server, &ajax_file);
	elysian_sprintf(ajax_file_name, "%s%u%s", "/fs_ram/ajax_file_", ajax_file_id++, ".text");
	err = elysian_fs_fopen(server, ajax_file_name, ELYSIAN_FILE_MODE_WRITE, &ajax_file);
	if(err != ELYSIAN_ERR_OK){ 
		elysian_mem_free(server, ajax_file_name);
		return err;
	}
	err = elysian_fs_fwrite(server, &ajax_file, (uint8_t*)ajax_file_contents, strlen(ajax_file_contents), &actual_write_sz);
	if(err != ELYSIAN_ERR_OK){
		elysian_fs_fclose(server, &ajax_file);
		elysian_fs_fremove(server, ajax_file_name);
		elysian_mem_free(server, ajax_file_name);
        return err;
    }
	elysian_fs_fclose(server, &ajax_file);

	//elysian_sprintf(buffer, "[Timestamp %u] RAM usage is <b>%u</b> bytes.", elysian_time_now(), elysian_mem_usage());
	elysian_sprintf(buffer, "%u,%u", elysian_mem_usage(), ELYSIAN_MAX_MEMORY_USAGE_KB * 1024);
	err = elysian_mvc_attribute_set(server, "ajax_attr", buffer);
    if(err != ELYSIAN_ERR_OK){ 
		elysian_fs_fremove(server, ajax_file_name);
		elysian_mem_free(server, ajax_file_name);
        return err;
    }
	
	err = elysian_mvc_view_set(server, ajax_file_name);
    if(err != ELYSIAN_ERR_OK){ 
		elysian_fs_fremove(server, ajax_file_name);
		elysian_mem_free(server, ajax_file_name);
        return err;
    }
	
	elysian_mvc_httpreq_onservice_handler(server, controller_ajax_example_served, ajax_file_name);
	return ELYSIAN_ERR_OK;
}


elysian_err_t controller_virtual_files_html(elysian_t* server){
	//elysian_client_t* client = elysian_mvc_client(server);
	char attr_value[32];
    elysian_err_t err;

    ELYSIAN_LOG("[[ %s ]]", __func__);
    
	elysian_sprintf(attr_value, "%u", elysian_time_now());
	err = elysian_mvc_attribute_set(server, "attr_virtual_file_path", "/fs_hdl/virtual_file.log");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
    err = elysian_mvc_view_set(server, "/fs_rom/virtual_files.html");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }

    return ELYSIAN_ERR_OK;
}

elysian_err_t controller_dynamic_page_disk_html(elysian_t* server){
	//elysian_client_t* client = elysian_mvc_client(server);
	elysian_file_t disk_file;
	uint32_t file_size;
	char attr_value[64];
    elysian_err_t err;

    ELYSIAN_LOG("[[ %s ]]", __func__);
    
	elysian_fs_finit(server, &disk_file);
	err = elysian_fs_fopen(server, "/fs_ext/img1.jpg", ELYSIAN_FILE_MODE_READ, &disk_file);
	if(err != ELYSIAN_ERR_OK){ 
		return err;
	}
	
	err = elysian_fs_fsize(server, &disk_file, &file_size);
	if(err != ELYSIAN_ERR_OK){
		elysian_fs_fclose(server, &disk_file);
        return err;
    }
	
	elysian_fs_fclose(server, &disk_file);
	
	elysian_sprintf(attr_value, "<b>%u</b>", file_size);
	err = elysian_mvc_attribute_set(server, "attr_file_size", attr_value);
    if(err != ELYSIAN_ERR_OK){
        return err;
    }
	
    err = elysian_mvc_view_set(server, "/fs_ext/dynamic_page_disk.html");
    if(err != ELYSIAN_ERR_OK){ 
        return err;
    }
	
    return ELYSIAN_ERR_OK;
}

typedef struct{
	uint32_t size;
	uint32_t read_cursor;
} example_hdl_file_t;

int virtual_file_handler(elysian_t* server, elysian_file_hdl_action_e action,  void** varg, uint8_t* buf, uint32_t buf_size){
	example_hdl_file_t* file_args = (example_hdl_file_t*) *varg;
	uint32_t read_size;
	
	switch(action) {
		case ELYSISIAN_FILE_HDL_ACTION_FOPEN:
		{
			ELYSIAN_LOG("ELYSIAN_FILE_HDL_ACTION_FOPEN");
			file_args = elysian_mem_malloc(server, sizeof(example_hdl_file_t));
			if (file_args) {
				/*
				** Create a 5 mb virtual file
				*/
				*varg = file_args;
				file_args->read_cursor = 0;
				file_args->size = 5 * 1024 * 1024;
			}
			
			return 0;
		}break;
		case ELYSISIAN_FILE_HDL_ACTION_FSEEK0:
		{
			/*
			** Reset the current read cursor
			*/
			file_args->read_cursor = 0;
			
			return 0;
		}break;	
		case ELYSISIAN_FILE_HDL_ACTION_FREAD:
		{
			/*
			** Copy the requested amount of bytes. If EOF reached, copy zero bytes to indicate it.
			*/
			read_size = file_args->size - file_args->read_cursor;
			if (read_size > buf_size) {
				read_size = buf_size;
			}

			/*
			** Increase the current read cursor
			*/
			file_args->read_cursor += read_size;

			memset (buf, 0, read_size);
			
			return read_size;
		}break;	
		case ELYSISIAN_FILE_HDL_ACTION_FCLOSE:
		{
			ELYSIAN_LOG("ELYSISIAN_FILE_HDL_ACTION_FCLOSE");
			elysian_mem_free(server, file_args);
			return 0;
		}break;
	};

	return 0;
}

typedef struct{
	uint32_t timer_count;
} example_websocket_args_t;

elysian_err_t websocket_connected_handler(elysian_t* server, void** vargs) {
	example_websocket_args_t* args;
	
	ELYSIAN_LOG("websocket_connected_handler()");
	
	args = elysian_mem_malloc(server, sizeof(example_websocket_args_t));
	if (!args) {
		return ELYSIAN_ERR_FATAL;
	}
	
	/* Start timer */
	elysian_websocket_timer_config(server, 500);
	
	args->timer_count = 0;
	*vargs = args;
	
	return ELYSIAN_ERR_OK;
}

elysian_err_t websocket_frame_handler(elysian_t* server, uint8_t* frame_data, uint32_t frame_len, void* vargs) {
	//example_websocket_args_t* args = (example_websocket_args_t*) vargs;
	elysian_err_t err;
	
	ELYSIAN_LOG("websocket_frame_handler()");
	
	err = elysian_websocket_send_text(server, (char*) frame_data, frame_len);
	if (err != ELYSIAN_ERR_OK){
		/* Frame transmission failed */
	}
	
	return ELYSIAN_ERR_OK;
}

elysian_err_t websocket_timer_handler(elysian_t* server, void* vargs) {
	example_websocket_args_t* args = (example_websocket_args_t*) vargs;
	elysian_err_t err;
	char frame_data[64];
	
	ELYSIAN_LOG("websocket_timer_handler()");
	
	/* Restart timer */
	elysian_websocket_timer_config(server, 1500);
	
	elysian_sprintf(frame_data, "Timer handler #%u", args->timer_count++);
	err = elysian_websocket_send_text(server, frame_data, strlen(frame_data));
	if (err != ELYSIAN_ERR_OK){
		/* Frame transmission failed */
	}
	
	return ELYSIAN_ERR_OK;
}

elysian_err_t websocket_disconnected_handler(elysian_t* server, void* vargs) {
	example_websocket_args_t* args = (example_websocket_args_t*) vargs;

	if (args) {
		elysian_mem_free(server, args);
	}
	
	return ELYSIAN_ERR_OK;
}



const elysian_file_hdl_t hdl_fs[] = {
	{.name = (char*) "/virtual_file.log", .handler = virtual_file_handler},
	{.name = NULL, .handler = NULL},
};

const elysian_mvc_controller_t mvc_controllers[] = {
	/*
	** Controllers for files stored to ROM memory device
	*/
	{.url = "/fs_rom/dynamic_page.html", .handler = controller_dynamic_page_html, 
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_GET},
	{.url = "/fs_rom/form_get.html", .handler = controller_form_get_html, 
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_GET},
	{.url = "/fs_rom/form_get_controller", .handler = controller_form_get, 
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_GET},
	{.url = "/fs_rom/form_post.html", .handler = controller_form_post_html, 
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_GET},
	{.url = "/fs_rom/form_post_controller", .handler = controller_form_post, 
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_POST},
	{.url = "/fs_rom/file_upload_ram.html", .handler = controller_file_upload_html, 
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_GET},
	{.url = "/fs_rom/file_upload_ram_controller", .handler = controller_file_upload, 
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_POST | ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_PUT},
	{.url = "/fs_rom/http_request_exposure.html", .handler = controller_http_request_exposure_html, 
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_GET},
	{.url = "/fs_rom/http_request_exposure_controller", .handler = controller_http_request_exposure, 
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_GET | ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_POST | ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_PUT},
	{.url = "/fs_rom/redirected_page0.html", .handler = controller_redirected_page0_html, 
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_GET | ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_POST},
	{.url = "/fs_rom/redirected_page1.html", .handler = controller_redirected_page1_html, 
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_GET},
	{.url = "/fs_rom/ajax_controller", .handler = controller_ajax, 
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_GET},
	{.url = "/fs_rom/virtual_files.html", .handler = controller_virtual_files_html, 
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_GET},
	
	/*
	** Controllers for files stored to the EXT(ernal) memory device
	*/
	{.url = "/fs_ext/dynamic_page_disk.html", .handler = controller_dynamic_page_disk_html, 
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_GET},
	{.url = "/fs_ext/file_upload_ext.html", .handler = controller_file_upload_html, 
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_GET},
	{.url = "/fs_ext/file_upload_ext_controller", .handler = controller_file_upload, 
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_POST | ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_PUT | ELYSIAN_MVC_CONTROLLER_FLAG_USE_EXT_FS},

	/*
	* End of list
	*/
	{.url = NULL, .handler = NULL,  
		.flags = ELYSIAN_MVC_CONTROLLER_FLAG_NONE},
};

const elysian_websocket_controller_t webosocket_controllers[] = {
	{.url = (char*) "/websockets_example", 
	.timer_interval_ms = 500,
	.connected_handler = websocket_connected_handler, 
	.frame_handler = websocket_frame_handler, 
	.timer_handler = websocket_timer_handler, 
	.disconnected_handler = websocket_disconnected_handler},
	
	/* End of list */																	
	{.url = (char*) NULL, 
	.timer_interval_ms = 0,
	.connected_handler = NULL, 
	.frame_handler = NULL, 
	.timer_handler = NULL, 
	.disconnected_handler = NULL},
};



int main(){
	uint8_t stop = 0;
    elysian_t* server;
	ELYSIAN_LOG("Starting web server. SERVER %u, CLIENT %u, RESOURCE %u, TOTAL %u", (unsigned int) sizeof(elysian_t), (unsigned int) sizeof(elysian_client_t), (unsigned int) sizeof(elysian_resource_t), (unsigned int) (sizeof(elysian_t) + sizeof(elysian_client_t) + sizeof(elysian_resource_t)));
	
    server = elysian_new();

    elysian_start(server, 9000, mvc_controllers, rom_fs, hdl_fs, webosocket_controllers, authentication_cb);
    
    while(!stop){
        elysian_poll(server, 10000);
    }
    
	elysian_stop(server);
	
    return 0;
}
