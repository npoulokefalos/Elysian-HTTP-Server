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

#ifndef __ELYSIAN_INTERNAL_H
#define __ELYSIAN_INTERNAL_H

#include <stdio.h>
#include <sys/time.h> //timeval
#include <stdint.h> //uint32_t
#include <unistd.h> //usleep
#include <stdlib.h> //free
#include <string.h> //memcopy
#include <ctype.h> //toupper



#define ELYSIAN_SERVER_NAME							"Elysian Web Server"

/*
** Virtual and absolute root paths for the RAM, ROM, EXT and internal Web Server's storage devices. 
**
** After receiving a new HTTP request requesting the resource {ELYSIAN_FS_xxx_VRT_ROOT}resource, 
** the web server is going to open the file from the absolute path {ELYSIAN_FS_xxx_ABS_ROOT}resource as follows:
**
** - ELYSIAN_FS_xxx_VROOT is ELYSIAN_FS_RAM_VRT_VROOT:
**	 The file is located into RAM, elysian_fs_ram_fopen will be called with path = {ELYSIAN_FS_RAM_ABS_ROOT}resource
**
** - ELYSIAN_FS_xxx_VROOT is ELYSIAN_FS_ROM_VRT_ROOT:
**	 The file is located into ROM, elysian_port_fs_rom_fopen() will be called with path = {ELYSIAN_FS_ROM_ABS_ROOT}resource
**
** - ELYSIAN_FS_xxx_VROOT is ELYSIAN_FS_EXT_VRT_ROOT:
**	 The file is located into an external storage device, elysian_port_fs_ext_fopen() will be called with path = {ELYSIAN_FS_EXT_ABS_ROOT}resource
**
** All web pages should refer to the virtual resource paths which provide a hint to the web server for the storage device (RAM, ROM, EXT, WS)
** of the particular resource. The Web Server transforms the virtual path to an absolute path according to user preferences, and calls the appropriate
** fopen function.
*/

/* 
** RAM partition which provides an abstraction so data stored in either RAM/ROM/EXT can be handled in the same manner.
** It is mostly used internally by the web server (for example to store small POST requests), but it can be used from user too.
*/
#define ELYSIAN_FS_RAM_VRT_ROOT						"/fs_ram"
#define ELYSIAN_FS_RAM_ABS_ROOT						""

/* 
** ROM partition which refers to resources stored as "const" variables in the .text area of the program
** (For example microcontroller's internal FLASH memory)
*/
#define ELYSIAN_FS_ROM_VRT_ROOT						"/fs_rom" 
#define ELYSIAN_FS_ROM_ABS_ROOT						""

/* 
** EXT partition which refers to resources stored to an external storage device (SD Card, USB, Hard Disk, ..).
*/
#define ELYSIAN_FS_EXT_VRT_ROOT						"/fs_ext"
//#define ELYSIAN_FS_EXT_ABS_ROOT					Defined by app

/* 
** Web Server's internal partition for sending default content, for example default error pages (404, 500, ..)
** when user-defined error pages are not provided in any other user partition.
*/
#define ELYSIAN_FS_HDL_VRT_ROOT						"/fs_hdl"
#define ELYSIAN_FS_HDL_ABS_ROOT						""


/*
** Specify the partition for the "index.html" web page. 
**
** When an HTTP request arrives and the requested resource is "/", the web server does not know the 
** location of the index.html page (there is no ELYSIAN_FSROM_VRT_ROOT / ELYSIAN_FSDISK_VRT_ROOT prefix)
** and it should be specified here:
**
** - ELYSIAN_FS_INDEX_HTML_VRT_ROOT == ELYSIAN_FS_ROM_VRT_ROOT : 
**	 index.html is located to ROM, and will be requested as {ELYSIAN_FSROM_ABS_ROOT}index.html from elysian_port_fsrom_fopen()
**
** - ELYSIAN_FS_INDEX_HTML_VRT_ROOT == ELYSIAN_FS_EXT_VRT_ROOT : 
**	 index.html is located to EXT, and will be requested as {ELYSIAN_FSDISK_ABS_ROOT}index.html from elysian_port_fsext_fopen()
*/
#define ELYSIAN_FS_INDEX_HTML_VRT_ROOT				ELYSIAN_FS_ROM_VRT_ROOT

/*
** Special file that may be used in responses with empty body. It is located to the ELYSIAN_FS_WS_VRT_ROOT partition.
** CAUTION: Do not change the partition of this file, unless it is guaranteed that this file will always exist to
**			the specified partition.
*/
#define ELYSIAN_FS_EMPTY_FILE_NAME 					"/empty.file"
#define ELYSIAN_FS_EMPTY_FILE_VRT_ROOT 				ELYSIAN_FS_ROM_VRT_ROOT


#define ELYSIAN_CBUF_LEN (384)


#define ELYSIAN_TIME_INFINITE  			(uint32_t)(-1)

#define ELYSIAN_MVC_PARAM_HTTP_HEADERS		"__elysian_mvc_param_http_headers__"
#define ELYSIAN_MVC_PARAM_HTTP_BODY			"__elysian_mvc_param_http_body__"

//typedef struct elysian_t elysian_t;


typedef struct elysian_client_t elysian_client_t;
typedef struct elysian_t elysian_t;
typedef struct elysian_schdlr_t elysian_schdlr_t;

/* 
** @brief Check if the requested URL can be accessed using the specific credentials.
**
** @param[in] server 	The server instance
** @param[in] url 		Requested URL
** @param[in] username 	Username specified into the HTTP request
** @param[in] password 	Password specified into the HTTP request
**
** @retval 1 	Credentials correct, grant access to the requested resource
** @retval 0 	Credentials not valid, deny access to the requested resource
 */
typedef uint8_t (*elysian_authentication_cb_t)(elysian_t* server, char* url, char* username, char* password);

/* 
** cbuf
*/
typedef struct elysian_cbuf_t elysian_cbuf_t;
struct elysian_cbuf_t{
	elysian_cbuf_t* next;
	uint16_t len;
	uint8_t* data;
	uint8_t wa[0];
};

elysian_client_t* elysian_current_client(elysian_t* server);

elysian_cbuf_t* elysian_cbuf_alloc(elysian_t* server, uint8_t* data, uint32_t len);
void elysian_cbuf_free(elysian_t* server, elysian_cbuf_t* cbuf);
void elysian_cbuf_list_append(elysian_cbuf_t** cbuf_list, elysian_cbuf_t* cbuf_new);
void elysian_cbuf_list_free(elysian_t* server, elysian_cbuf_t* cbuf_list);
uint32_t elysian_cbuf_list_len(elysian_cbuf_t* cbuf_list);
elysian_err_t elysian_cbuf_list_split(elysian_t* server, elysian_cbuf_t** cbuf_list0, uint32_t size, elysian_cbuf_t** cbuf_list1);
elysian_err_t elysian_cbuf_rechain(elysian_t* server, elysian_cbuf_t** cbuf_list, uint32_t size);
void elysian_cbuf_strget(elysian_cbuf_t* cbuf, uint32_t cbuf_index, char* buf, uint32_t buf_len);
uint8_t elysian_cbuf_strcmp(elysian_cbuf_t* cbuf, uint32_t index, char* str, uint8_t matchCase);
void elysian_cbuf_strcpy(elysian_cbuf_t* cbuf, uint32_t index0, uint32_t index1, char* str);
void elysian_cbuf_memcpy(elysian_cbuf_t* cbuf, uint32_t index0, uint32_t index1, uint8_t* buf);
uint32_t elysian_cbuf_strstr(elysian_cbuf_t* cbuf0, uint32_t index, char* str, uint8_t matchCase);
elysian_cbuf_t* elysian_cbuf_list_dropn(elysian_cbuf_t* cbuf, uint32_t n);
void cbuf_list_print(elysian_cbuf_t* cbuf);

/* 
** HTTP
*/
typedef enum{
    ELYSIAN_HTTP_METHOD_GET = 1 << 1,
    ELYSIAN_HTTP_METHOD_HEAD = 1 << 2,
    ELYSIAN_HTTP_METHOD_POST = 1 << 3,
    ELYSIAN_HTTP_METHOD_PUT = 1 << 4,
    ELYSIAN_HTTP_METHOD_SUBSCRIBE = 1 << 5,
    ELYSIAN_HTTP_METHOD_UNSUBSCRIBE = 1 << 6,
    ELYSIAN_HTTP_METHOD_MAX,
	ELYSIAN_HTTP_METHOD_NA
}elysian_http_method_e;

typedef enum{
	ELYSIAN_HTTP_STATUS_CODE_100 = 0,
	ELYSIAN_HTTP_STATUS_CODE_101,
	ELYSIAN_HTTP_STATUS_CODE_200,
	ELYSIAN_HTTP_STATUS_CODE_201,
	ELYSIAN_HTTP_STATUS_CODE_206,
	ELYSIAN_HTTP_STATUS_CODE_302,
	ELYSIAN_HTTP_STATUS_CODE_400,
	ELYSIAN_HTTP_STATUS_CODE_401,
	ELYSIAN_HTTP_STATUS_CODE_404,
	ELYSIAN_HTTP_STATUS_CODE_405,
	ELYSIAN_HTTP_STATUS_CODE_408,
	ELYSIAN_HTTP_STATUS_CODE_413,
	ELYSIAN_HTTP_STATUS_CODE_417,
	ELYSIAN_HTTP_STATUS_CODE_500,
	ELYSIAN_HTTP_STATUS_CODE_MAX,
    ELYSIAN_HTTP_STATUS_CODE_NA,
}elysian_http_status_code_e;

/* 
** HTTP Request
*/
typedef enum{
    ELYSIAN_HTTP_RANGE_SOF = 0, /* Start of file */
    ELYSIAN_HTTP_RANGE_EOF = 0xfffffffe, /* End of file */
    ELYSIAN_HTTP_RANGE_WF = 0xffffffff, /* Whole file */
}elysian_httpreq_range_t;

typedef enum{
    ELYSIAN_HTTP_CONTENT_TYPE_APPLICATION__X_WWW_FORM_URLENCODED = 0,
    ELYSIAN_HTTP_CONTENT_TYPE_MULTIPART__FORM_DATA,
    ELYSIAN_HTTP_CONTENT_TYPE_NA,
}elysian_http_content_type_t;

typedef enum{
	ELYSIAN_HTTP_TRANSFER_ENCODING_IDENTITY = 0,
    ELYSIAN_HTTP_TRANSFER_ENCODING_CHUNKED,
}elysian_http_transfer_encoding_t;

typedef enum{
    ELYSIAN_WEBSOCKET_VERSION_13,
	ELYSIAN_WEBSOCKET_VERSION_NA,
}elysian_websocket_version_t;

typedef enum{
    ELYSIAN_HTTP_CONNECTION_CLOSE,
    ELYSIAN_HTTP_CONNECTION_KEEPALIVE,
	ELYSIAN_HTTP_CONNECTION_UPGRADE
}elysian_http_connection_t;

typedef enum{
	ELYSIAN_HTTP_CONNECTION_UPGRADE_NO,
    ELYSIAN_HTTP_CONNECTION_UPGRADE_WEBSOCKET,
}elysian_http_connection_upgrade_t;

elysian_err_t elysian_http_request_headers_received(elysian_t* server);
elysian_err_t elysian_http_request_headers_parse(elysian_t* server);
elysian_err_t elysian_http_request_get_uri(elysian_t* server, char** uri);
elysian_err_t elysian_http_request_get_method(elysian_t* server, elysian_http_method_e* method_id);
elysian_err_t elysian_http_request_get_header(elysian_t* server, char* header_name, char** header_value);
elysian_err_t elysian_http_request_get_params(elysian_t* server);

elysian_err_t elysian_http_response_build(elysian_t* server);
elysian_err_t elysian_http_add_response_status_line(elysian_t* server);
elysian_err_t elysian_http_add_response_header_line(elysian_t* server, char* header_name, char* header_value);
elysian_err_t elysian_http_add_response_empty_line(elysian_t* server);

void elysian_http_decode(char *encoded);

char* elysian_http_get_method_name(elysian_http_method_e method_id);
elysian_http_method_e elysian_http_get_method_id(char* method_name);
    
uint16_t elysian_http_get_status_code_num(elysian_http_status_code_e status_code);
char* elysian_http_get_status_code_msg(elysian_http_status_code_e status_code);
char* elysian_http_get_status_code_body(elysian_http_status_code_e status_code);

elysian_err_t elysian_http_authenticate(elysian_t* server);

char* elysian_http_get_mime_type(char* uri);
/*
****************************************************************************************************************************
** Time
****************************************************************************************************************************
*/

void elysian_time_sleep(uint32_t ms);
uint32_t elysian_time_elapsed(uint32_t tic_ms);

/*
****************************************************************************************************************************
** Thread
****************************************************************************************************************************
*/
void elysian_thread_yield(void);

/*
****************************************************************************************************************************
** Memory
****************************************************************************************************************************
*/
elysian_err_t elysian_os_hostname_get(char hostname[64]);

/*
****************************************************************************************************************************
** Sockets
****************************************************************************************************************************
*/
typedef struct elysian_socket_t elysian_socket_t;
struct elysian_socket_t{
    uint8_t passively_closed;
    uint8_t actively_closed;
    
#if	defined(ELYSIAN_TCPIP_ENV_UNIX)
    int fd;
#elif defined(ELYSIAN_TCPIP_ENV_LWIP)
    int fd;
#elif defined(ELYSIAN_TCPIP_ENV_WINDOWS)
	int fd;
#else

#endif
    
};

void elysian_socket_close(elysian_socket_t* socket);
elysian_err_t elysian_socket_listen(uint16_t port, elysian_socket_t* server_socket);
elysian_err_t elysian_socket_accept(elysian_socket_t* server_socket, uint32_t timeout_ms, elysian_socket_t* client_socket);
elysian_err_t elysian_socket_read(elysian_socket_t* client_socket, uint8_t* data, uint16_t datalen, uint32_t* received);
elysian_err_t elysian_socket_write(elysian_socket_t* client_socket, uint8_t* data, uint16_t datalen, uint32_t* sent);
elysian_err_t elysian_socket_select(elysian_socket_t* socket_readset[], uint32_t socket_readset_sz, uint32_t timeout_ms, uint8_t socket_readset_status[]);

/*
****************************************************************************************************************************
** Filesystem
****************************************************************************************************************************
*/
#define ELYSIAN_FILE_SEEK_START	(0)
#define ELYSIAN_FILE_SEEK_END	(0xFFFFFFFF)

typedef enum{
	ELYSIAN_FILE_STATUS_OPENED = 0,
	ELYSIAN_FILE_STATUS_CLOSED,
}elysian_file_status_t;

typedef enum{
	ELYSIAN_FILE_MODE_NA = 0,
	ELYSIAN_FILE_MODE_READ,
	ELYSIAN_FILE_MODE_WRITE,
}elysian_file_mode_t;


typedef struct elysian_fs_ram_file_t elysian_fs_ram_file_t;
struct elysian_fs_ram_file_t{
	char* name;
	uint8_t read_handles;
	uint8_t write_handles;
    elysian_cbuf_t* cbuf;
	elysian_fs_ram_file_t* next;
};

typedef struct elysian_file_ram_t elysian_file_ram_t;
struct elysian_file_ram_t{
	elysian_fs_ram_file_t* fd;
	//elysian_file_mode_t mode;
	//char* name;
	//uint8_t handles;
    uint32_t pos;
    //elysian_cbuf_t* cbuf;
};

typedef struct elysian_file_rom_t elysian_file_rom_t;
struct elysian_file_rom_t{
	const char* name;
    const uint8_t* ptr;
    uint32_t pos;
    uint32_t size;
};

typedef struct elysian_file_ext_t elysian_file_ext_t;
struct elysian_file_ext_t{
#if defined(ELYSIAN_FS_ENV_UNIX)
    FILE* fd;
#elif defined(ELYSIAN_ENV_WINDOWS)
	FILE* fd;
#elif defined(ELYSIAN_FS_ENV_FATAFS)
    FIL* fd;
#else

#endif
};

typedef struct elysian_file_def_hdl_t elysian_file_def_hdl_t;
struct elysian_file_def_hdl_t{
	const char* name;

	elysian_err_t 	(*open_handler)(elysian_t* server, void** varg);
	int 			(*read_handler)(elysian_t* server, void* varg, uint8_t* buf, uint32_t buf_size);
	elysian_err_t 	(*seekreset_handler)(elysian_t* server, void* varg);
	elysian_err_t 	(*close_handler)(elysian_t* server, void* varg);
};

typedef struct elysian_file_hdl_t elysian_file_hdl_t;
struct elysian_file_hdl_t{
	const elysian_file_def_hdl_t* def;
    void* varg;
    uint32_t pos;
};

typedef struct elysian_fs_memdev_t elysian_fs_memdev_t;

typedef struct elysian_file_t elysian_file_t;
struct elysian_file_t{
	elysian_fs_memdev_t* memdev;
    elysian_file_status_t status;
	elysian_file_mode_t mode;
	
    union{
        elysian_file_ram_t ram;
        elysian_file_rom_t rom;
        elysian_file_ext_t ext;
		elysian_file_hdl_t hdl;
    }descriptor;
};


//typedef struct elysian_fs_memdev_t elysian_fs_memdev_t;
struct elysian_fs_memdev_t{
	const char* vrt_root;
	const char* abs_root;
	elysian_err_t (*fopen)(elysian_t* server, char* abs_path, elysian_file_mode_t mode, elysian_file_t* file);
	elysian_err_t (*fsize)(elysian_t* server, elysian_file_t* file, uint32_t* filesize);
	elysian_err_t (*fseek)(elysian_t* server, elysian_file_t* file, uint32_t seekpos);
    elysian_err_t (*ftell)(elysian_t* server, elysian_file_t* file, uint32_t* seekpos);
	int (*fread)(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size);
	int (*fwrite)(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size);
	elysian_err_t (*fclose)(elysian_t* server, elysian_file_t* file);
	elysian_err_t (*fremove)(elysian_t* server, char* abs_path);
};

elysian_err_t elysian_fs_rom_fopen(elysian_t* server, char* abs_path, elysian_file_mode_t mode, elysian_file_t* file);
elysian_err_t elysian_fs_rom_fsize(elysian_t* server, elysian_file_t* file, uint32_t* filesize);
elysian_err_t elysian_fs_rom_fseek(elysian_t* server, elysian_file_t* file, uint32_t seekpos);
elysian_err_t elysian_fs_rom_ftell(elysian_t* server, elysian_file_t* file, uint32_t* seekpos);
int elysian_fs_rom_fread(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size);
int elysian_fs_rom_fwrite(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size);
elysian_err_t elysian_fs_rom_fclose(elysian_t* server, elysian_file_t* file);
elysian_err_t elysian_fs_rom_fremove(elysian_t* server, char* abs_path);

elysian_err_t elysian_fs_ram_fopen(elysian_t* server, char* abs_path, elysian_file_mode_t mode, elysian_file_t* file);
elysian_err_t elysian_fs_ram_fsize(elysian_t* server, elysian_file_t* file, uint32_t* filesize);
elysian_err_t elysian_fs_ram_fseek(elysian_t* server, elysian_file_t* file, uint32_t seekpos);
elysian_err_t elysian_fs_ram_ftell(elysian_t* server, elysian_file_t* file, uint32_t* seekpos);
int elysian_fs_ram_fread(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size);
int elysian_fs_ram_fwrite(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size);
elysian_err_t elysian_fs_ram_fclose(elysian_t* server, elysian_file_t* file);
elysian_err_t elysian_fs_ram_fremove(elysian_t* server, char* vrt_path);

elysian_err_t elysian_fs_hdl_fopen(elysian_t* server, char* abs_path, elysian_file_mode_t mode, elysian_file_t* file);
elysian_err_t elysian_fs_hdl_fsize(elysian_t* server, elysian_file_t* file, uint32_t* filesize);
elysian_err_t elysian_fs_hdl_fseek(elysian_t* server, elysian_file_t* file, uint32_t seekpos);
elysian_err_t elysian_fs_hdl_ftell(elysian_t* server, elysian_file_t* file, uint32_t* seekpos);
int elysian_fs_hdl_fread(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size);
int elysian_fs_hdl_fwrite(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size);
elysian_err_t elysian_fs_hdl_fclose(elysian_t* server, elysian_file_t* file);
elysian_err_t elysian_fs_hdl_fremove(elysian_t* server, char* vrt_path);



//uint32_t elysian_mem_usage(void);

/*
** Strings
*/
elysian_err_t elysian_strstr_file(elysian_t* server, elysian_file_t* file, uint32_t offset, char* pattern1, char* pattern2, uint8_t match_case, uint32_t* index1, uint32_t* index2);
elysian_err_t elysian_strncpy_file(elysian_t* server, elysian_file_t* file, uint32_t offset, char* str, uint32_t n);
void elysian_str_trim(elysian_t* server, char* str, char* ignore_prefix_chars, char* ignore_suffix_chars);

/* 
** Websockets
*/
typedef struct elysian_websocket_controller_t elysian_websocket_controller_t;
struct elysian_websocket_controller_t{
    const char* url;
    elysian_err_t (*connected_handler)(elysian_t* server, void** varg);
	elysian_err_t (*frame_handler)(elysian_t* server, void* varg, uint8_t* frame_data, uint32_t frame_len);
	elysian_err_t (*timer_handler)(elysian_t* server, void* varg);
	elysian_err_t (*disconnected_handler)(elysian_t* server, void* varg);
};

typedef enum {
	ELYSIAN_WEBSOCKET_FRAME_OPCODE_CONTINUATION = 0x0,
	ELYSIAN_WEBSOCKET_FRAME_OPCODE_TEXT = 0x1,
	ELYSIAN_WEBSOCKET_FRAME_OPCODE_BINARY = 0x2,
	ELYSIAN_WEBSOCKET_FRAME_OPCODE_CLOSE = 0x8,
	ELYSIAN_WEBSOCKET_FRAME_OPCODE_PING = 0x9,
	ELYSIAN_WEBSOCKET_FRAME_OPCODE_PONG = 0xA,
} elysian_websocket_opcode_e;

typedef enum {
	ELYSIAN_WEBSOCKET_FLAG_PING_PENDING = 1 << 0,
	ELYSIAN_WEBSOCKET_FLAG_PONG_RECEIVED = 1 << 1,
	ELYSIAN_WEBSOCKET_FLAG_CLOSE_PENDING = 1 << 2,
	ELYSIAN_WEBSOCKET_FLAG_CLOSE_RECEIVED = 1 << 3,
	ELYSIAN_WEBSOCKET_FLAG_CLOSE_REQUESTED = 1 << 4,
	ELYSIAN_WEBSOCKET_FLAG_DISCONNECTING = 1 << 5,
} elysian_websocket_flag_e;

/* 
** MVC
*/
typedef struct elysian_mvc_attribute_t elysian_mvc_attribute_t;
struct elysian_mvc_attribute_t{
    char* name;
    char* value;
    elysian_mvc_attribute_t* next;
};

typedef struct elysian_mvc_httpresp_header_t elysian_mvc_httpresp_header_t;
struct elysian_mvc_httpresp_header_t {
    char* header;
	char* value;
    elysian_mvc_httpresp_header_t* next;
};

/* 
** @brief 		Registers a MVC handler (callback)
**
** @param[in]	server 		The server instance
**
** @retval  	ELYSIAN_ERR_OK		Indicates success. Web Server will serve the HTTP request using the
**									MVC configuration set by the user.
** @retval  	ELYSIAN_ERR_FATAL	Indicates a none-recoverable error. Web Server will send a HTTP response
**									with "HTTP 500 Internal Server Error" status code. MVC configuration set
**									by user will be ignored.
** @retval  	ELYSIAN_ERR_POLL	Indicates temporary but recoverable error. The HTTP request will not be dropped.
**									Web Server will trigger the specific handler later with exponential backoff until
**									succesfull handling (ELYSIAN_ERR_OK) or until timeout. The exponential backoff
**									mechanism will trigger the handler using the following delay sequence:
**									{1ms, 2ms, 4ms, 8ms, ..., 256ms, 512ms, 512ms, 512ms, .. }. When the total delay
**									between the fist handler triggering and the current timestamp reaches the timeout
**									value (depending on HTTP state the timeout will be between 5.000 and 10.000 msec)
**									the Web Server will automatically generate a HTTP response using the 
**									"HTTP 500 Internal Server Error" status code. This polling mechanism makes the
**									Web Server extremely robust, since temporary failed memory allocations do not
**									prevent HTTP request servicing and also increases the multi-client performance
**									without requiring extra memory. Due to the exponsentially increased delay periods
**									the power consumption and CPU usage are not affected.
**
**									It si important to note that, since the MVC handler might be called more than once
**									for a specific HTTP request, all user memory allocations should be freed before the
**									handler returns the ELYSIAN_ERR_POLL code, to prevent memory leaks. Below is a
**									demonstrative part of code:

									elysian_err_t test_controller_handler(elysian_t* server) {
										elysian_err_t err;
										char* param_value;
										uint8_t param_found;
										char* buf;
										
										buf = elysian_mem_malloc(server, 512); // user allocated memory block
										if (!buf) {
											return ELYSIAN_ERR_POLL;
										}
										
										err = elysian_mvc_param_get_str(server, "test_parameter", &param_value, &param_found);
										if(err != ELYSIAN_ERR_OK){ 
											elysian_mem_free(server, buf); 	// Since ELYSIAN_ERR_POLL or ELYSIAN_ERR_FATAL will be returned, the
																			// user allocated "buf" should be freed.
											return err;
										}
										
										//	...
										//	Use "param_value" / "buf" variables
										//	...
										
										// Free user allocated memory if needed
										elysian_mem_free(server, buf);
										
										return ELYSIAN_ERR_OK;
									}
**
**									Consider for example that at a specific time, the Web Server is servicing 5 HTTP requests
**									and the free memory will be limited for 1200ms (etc until the 3rd HTTP request will be served
**									and the associated allocated memory will be freed), preventing a 6th HTTP request to be serviced. 
** 									If the 6th HTTP request is received, the corresponding MVC handler is highly likely to return
**									an ELYSIAN_ERR_POLL error code. The behavior of the exponsential backoff mechanism will be
**									as follows:
**			
**									[Time 0 ms] 	MVC handler for the 6th HTTP request is triggered for first time, but
**													due to temporary limited memory resources returns ELYSIAN_ERR_POLL
**									[Time 1 ms] 	MVC handler called again, returns ELYSIAN_ERR_POLL
**									[Time 3 ms] 	MVC handler called again, returns ELYSIAN_ERR_POLL
**									[Time 7 ms] 	MVC handler called again, returns ELYSIAN_ERR_POLL
**									[Time 15 ms] 	MVC handler called again, returns ELYSIAN_ERR_POLL
**									[Time 31 ms] 	MVC handler called again, returns ELYSIAN_ERR_POLL
**									[Time 63 ms] 	MVC handler called again, returns ELYSIAN_ERR_POLL
**									[Time 127 ms] 	MVC handler called again, returns ELYSIAN_ERR_POLL
**									[Time 255 ms] 	MVC handler called again, returns ELYSIAN_ERR_POLL
**									[Time 511 ms] 	MVC handler called again, returns ELYSIAN_ERR_POLL
**									[Time 1023 ms] 	MVC handler called again, returns ELYSIAN_ERR_POLL
**									[Time 1535 ms] 	MVC handler called again, 3rd HTTP request has been served, 6th HTTP request will 
**													be served normally since the required memory is now available.
 */
typedef elysian_err_t (*elysian_mvc_controller_handler_t)(elysian_t* server);
	


typedef struct elysian_mvc_controller_t elysian_mvc_controller_t;
struct elysian_mvc_controller_t{
    const char* url;
    elysian_mvc_controller_handler_t handler;
    //uint8_t http_methods_mask;
	elysian_mvc_controller_flag_e flags;

    //elysian_mvc_controller_t* next;
};


typedef struct elysian_mvc_alloc_t elysian_mvc_alloc_t;
struct elysian_mvc_alloc_t{
	void* data;
    elysian_mvc_alloc_t* next;
};

typedef struct elysian_req_param_t elysian_req_param_t;

typedef struct elysian_mvc_t elysian_mvc_t;
struct elysian_mvc_t{
	char* view;
	elysian_http_status_code_e status_code;
	elysian_http_transfer_encoding_t transfer_encoding;
	uint32_t range_start;
	uint32_t range_end;
	
	/*
	** Flag indicating if connection will be kept alive or closed after
	** the current HTTP response is sent.
	*/
	elysian_http_connection_t connection;
	elysian_http_connection_upgrade_t connection_upgrade;
	
	uint32_t content_length;
	
	/*
	** Holds the redirection URL specified from a controller
	*/
    //char* redirection_url;
	
	elysian_mvc_httpresp_header_t* httpresp_headers;
	
    elysian_mvc_attribute_t* attributes;
	elysian_mvc_alloc_t* allocs;
};

typedef void (*elysian_httpreq_onservice_handler_t)(elysian_t* server, void* ptr);

void elysian_mvc_init(elysian_t* server);
elysian_err_t elysian_mvc_pre_configure(elysian_t* server);
elysian_err_t elysian_mvc_post_configure(elysian_t* server);

uint8_t elysian_mvc_isconfigured(elysian_t* server);
elysian_err_t elysian_mvc_clear(elysian_t* server);

elysian_mvc_controller_t* elysian_mvc_controller_get(elysian_t* server, char* url, elysian_http_method_e method_id);
elysian_mvc_attribute_t* elysian_mvc_attribute_get(elysian_t* server, char* name);


struct elysian_req_param_t{
	elysian_req_param_t* next;
    elysian_client_t* client;
	elysian_file_t* file;
	char* name;
	char* filename;
    //uint32_t index0;
    //uint32_t len;
    
	
	uint32_t data_size;
	
	/*
	** Initial data index
	*/
	uint32_t data_index;
	
	/*
	** Current data index (next read index)
	*/
	//uint32_t data_index_cur;
};


/* 
** Pre-processors
*/
typedef struct elysian_resource_t elysian_resource_t;
struct elysian_resource_t{
    uint8_t openned;
	elysian_err_t (*open)(elysian_t* server);
	elysian_err_t (*size)(elysian_t* server, uint32_t* size);
	elysian_err_t (*seek)(elysian_t* server, uint32_t seekpos);
    elysian_err_t (*read)(elysian_t* server, uint8_t* readbuf, uint32_t readbufsz, uint32_t* readbufszactual);
    elysian_err_t (*close)(elysian_t* server);
    elysian_file_t file;
	//uint32_t pos;
	uint32_t calculated_size;
    void* priv;
	//uint8_t err;
};

void elysian_resource_init(elysian_t* server);
elysian_err_t elysian_resource_open(elysian_t* server);
uint8_t elysian_resource_isopened(elysian_t* server);
elysian_err_t elysian_resource_size(elysian_t* server, uint32_t * resource_size);
elysian_err_t elysian_resource_seek(elysian_t* server, uint32_t seekpos);
elysian_err_t elysian_resource_read(elysian_t* server, uint8_t* readbuf, uint32_t readbufsz, uint32_t* readbufszactual);
elysian_err_t elysian_resource_close(elysian_t* server);
    

/* 
** Scheduler
*/
typedef enum{
    elysian_schdlr_EV_ENTRY,
    elysian_schdlr_EV_READ, 
    elysian_schdlr_EV_POLL,
    elysian_schdlr_EV_TIMER1,
	elysian_schdlr_EV_TIMER2,
    elysian_schdlr_EV_ABORT,
}elysian_schdlr_ev_t;

typedef enum{
	elysian_schdlr_TASK_PRIO_LOW = 0,
    elysian_schdlr_TASK_PRIO_NORMAL,
    elysian_schdlr_TASK_PRIO_HIGH, 
}elysian_schdlr_task_prio_t;


typedef void (*elysian_schdlr_state_t)(elysian_t* server, elysian_schdlr_ev_t event);

typedef struct elysian_schdlr_task_t elysian_schdlr_task_t;
struct elysian_schdlr_task_t{
	elysian_schdlr_state_t state;
	elysian_schdlr_state_t new_state;
	elysian_schdlr_task_prio_t priority;

	uint32_t poll_delta;
	uint32_t poll_delta_init;
	uint32_t timer1_delta;
	uint32_t timer1_delta_init;
	uint32_t timer2_delta;
	uint32_t timer2_delta_init;

	elysian_client_t* client;
	elysian_cbuf_t* cbuf_list;

	elysian_schdlr_task_t* next;
	elysian_schdlr_task_t* prev;
};

//typedef struct elysian_schdlr_t elysian_schdlr_t;
struct elysian_schdlr_t{
    elysian_t* server;
	uint32_t prev_yield_timestamp;
	uint8_t disabled_acceptor_delta;
	uint8_t disabled_acceptor_delta_init;
	uint8_t disabled_reader_delta;
	uint8_t disabled_reader_delta_init;
	uint32_t non_poll_tic_ms;
    
    elysian_socket_t socket;
    
    elysian_schdlr_task_t tasks;
	
	elysian_schdlr_task_t* current_task; /* Current task */
    
    /*
    ** select() readset for server + clients
    */
    elysian_socket_t* socket_readset[ELYSIAN_MAX_CLIENTS_NUM + 1];
    uint8_t socket_readset_status[ELYSIAN_MAX_CLIENTS_NUM + 1];
    
    elysian_schdlr_state_t client_connected_state;
};

elysian_schdlr_task_t* elysian_schdlr_current_task_get(elysian_t* server);
elysian_client_t* elysian_schdlr_current_client_get(elysian_t* server);
void elysian_schdlr_state_set(elysian_t* server, elysian_schdlr_state_t state);
elysian_schdlr_state_t elysian_schdlr_state_get(elysian_t* server);
void elysian_schdlr_state_poll_set(elysian_t* server, uint32_t poll_delta);
void elysian_schdlr_state_poll_enable(elysian_t* server);
void elysian_schdlr_state_poll_disable(elysian_t* server);
void elysian_schdlr_state_poll_backoff(elysian_t* server);
void elysian_schdlr_state_timer1_set(elysian_t* server, uint32_t timer_delta);
void elysian_schdlr_state_timer1_reset(elysian_t* server);
void elysian_schdlr_state_timer2_set(elysian_t* server, uint32_t timer_delta);
void elysian_schdlr_state_timer2_reset(elysian_t* server);
void elysian_schdlr_state_priority_set(elysian_t* server, elysian_schdlr_task_prio_t priority);
elysian_cbuf_t* elysian_schdlr_state_socket_read(elysian_t* server);




#if 0
#define elysian_schdlr_state_enter(schdlr, state)                     	elysian_schdlr_state_set(schdlr, state); return;
#define elysian_schdlr_poll_enable(schdlr)                 				elysian_schdlr_state_poll_enable(elysian_schdlr_current_task(schdlr));
#define elysian_schdlr_poll_disable(schdlr)                 			elysian_schdlr_state_poll_disable(elysian_schdlr_current_task(schdlr), ELYSIAN_TIME_INFINITE);
#define elysian_schdlr_state_poll_backoff(schdlr)                   	elysian_schdlr_state_poll_backoff(elysian_schdlr_current_task(schdlr));return;
#define elysian_schdlr_state_timeout_set(schdlr, timeout_delta)         elysian_schdlr_state_timeout_set(elysian_schdlr_current_task(schdlr), timeout_delta);
#define elysian_schdlr_state_timeout_reset(schdlr)						elysian_schdlr_state_timeout_reset(elysian_schdlr_current_task(schdlr));
#define elysian_schdlr_state_priority_set(schdlr, priority) 			elysian_schdlr_state_priority_set(elysian_schdlr_current_task(schdlr), priority);
#endif

void elysian_schdlr_poll(elysian_t* server, uint32_t intervalms);
elysian_err_t elysian_schdlr_init(elysian_t* server, uint16_t port, elysian_schdlr_state_t client_connected_state);
void elysian_schdlr_stop(elysian_t* server);

/* 
** HTTP Stats
*/
typedef enum{
    //elysian_stats_RES_RX = 0,
    //elysian_stats_RES_TX, 
    //elysian_stats_RES_CLIENTS,
    elysian_stats_RES_MEM,
    elysian_stats_RES_SLEEP,
    elysian_stats_RES_NUM,
}elysian_stats_res_t;

typedef enum{
    elysian_stats_RES_TYPE_MAXVALUE = 0,
    elysian_stats_RES_TYPE_INTERVAL,
}elysian_stats_res_type_t;

void elysian_stats_update(elysian_stats_res_t recource_id, uint32_t value);
void elysian_stats_get(elysian_stats_res_t recource_id);






#if 0
typedef struct elysian_httpreq_param_t elysian_httpreq_param_t;
struct elysian_httpreq_param_t{
	elysian_httpreq_param_t* next;
	uint32_t header_index0;
	uint32_t header_index1;
	uint32_t body_index0;
	uint32_t body_index1;
	char* name;
	char* filename;
};
#endif

typedef struct elysian_httpreq_t elysian_httpreq_t;
struct elysian_httpreq_t{
    char* url;
	char* multipart_boundary;
	elysian_http_method_e method;
    elysian_http_content_type_t content_type;
	elysian_http_transfer_encoding_t transfer_encoding;
	
	elysian_http_connection_t connection;
	elysian_http_connection_upgrade_t connection_upgrade;
	elysian_websocket_version_t websocket_version;
	
    uint8_t expect_status_code;
    uint16_t headers_len;
    uint32_t body_len;
    
	elysian_file_t headers_file;
    char headers_filename[24 + 14];  // max strlen(ELYSIAN_FS_ROM_x_ROOT) + strlen("/*_") + strlen(max uint32) + \0
    
	elysian_file_t body_file;
    char body_filename[24 + 14];  // max strlen(ELYSIAN_FS_ROM_x_ROOT) + strlen("/*_") + strlen(max uint32) + \0
	
    uint32_t range_start;
    uint32_t range_end;
	
	elysian_req_param_t* params;
};




/* 
** HTTP Response
*/
typedef struct elysian_httpresp_t elysian_httpresp_t;
struct elysian_httpresp_t{
    uint8_t* buf;
    uint16_t buf_index;
    uint16_t buf_len; /* Current length, <= buf_size */
    uint16_t buf_size; /* Total allocation size */
    

	/*
	** Status code used for the HTTP response
	*/
    elysian_http_status_code_e current_status_code;
	
	elysian_http_status_code_e fatal_status_code;
	
	uint8_t attempts;
	
	/*
	** Holds the redirection URL specified from a controller
	*/
    //char* redirection_url;
	/* 
	** The size of the whole resource that requested by the HTTP Client or specified by the Controller.
	** This size or part of it (Partial or HEAD HTTP request) will be transmitted in the HTTP body.
	*/
	//uint32_t resource_size; 
	/* 
	** Size of the HTTP headers to be transmitted. 
	*/
	uint16_t headers_size;
	/* 
	** Actual size of the resource to be transmitted. body_size <= resource_size.
	** - Initially it is set to resource_size.
	** - In case of HTTP HEAD request it will be set to 0.
	** - In case of an HTTP Partial request it will be set to the desired value specified by the client.
	*/
	uint32_t body_size;
	
	/* 
	** The size of the already transmitted part of the HTTP Header and HTTP Body.
	** When sent_size == headers_size + body_size the whole HTTP response will
	** have been sent to the HTTP Client.
	*/
	uint32_t sent_size;
};

void elysian_set_fatal_http_status_code(elysian_t* server, elysian_http_status_code_e status_code);
void elysian_set_http_status_code(elysian_t* server, elysian_http_status_code_e status_code);

/*
** ISP
*/
typedef struct elysian_isp_multipart_t elysian_isp_multipart_t;
struct elysian_isp_multipart_t{
	uint8_t state;
	uint32_t index;
	elysian_req_param_t* params;
};

typedef struct elysian_isp_raw_t elysian_isp_raw_t;
struct elysian_isp_raw_t{
	uint8_t state;
	uint32_t index;
};

typedef struct elysian_isp_chunked_t elysian_isp_chunked_t;
struct elysian_isp_chunked_t{
	uint8_t state;
	uint32_t index;
	uint32_t chunkSz;
	uint32_t chunkSzProcessed;
};

typedef struct elysian_isp_websocket_t elysian_isp_websocket_t;
struct elysian_isp_websocket_t{
	uint8_t state;
	//uint32_t index;
	uint8_t header[10];
	uint8_t masking_key[4];
	uint32_t payload_len;
	//uint8_t* payload;
};

elysian_err_t elysian_isp_http_headers(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream);
elysian_err_t elysian_isp_http_body_raw(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream);
elysian_err_t elysian_isp_http_body_chunked(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream);
elysian_err_t elysian_isp_http_body_chunked_multipart(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream);
elysian_err_t elysian_isp_http_body_raw_multipart(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream);
elysian_err_t elysian_isp_websocket(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream);

void elysian_isp_cleanup(elysian_t* server);

#if 0
typedef struct elysian_isp_raw_multipart_t elysian_isp_raw_multipart_t;
struct elysian_isp_raw_multipart_t{
	elysian_isp_raw_t raw;
	elysian_isp_multipart_t multipart;
};

typedef struct elysian_isp_chunked_multipart_t elysian_isp_chunked_multipart_t;
struct elysian_isp_chunked_multipart_t{
	elysian_isp_raw_t raw;
	elysian_isp_multipart_t multipart;
};
#endif

/* 
** Client
*/

typedef struct elysian_isp_t elysian_isp_t;
struct elysian_isp_t{
	elysian_isp_raw_t raw;
	elysian_isp_chunked_t chunked;
	elysian_isp_multipart_t multipart;
	elysian_isp_websocket_t websocket;
	
	elysian_cbuf_t* cbuf_list;
	elysian_err_t (*func)(elysian_t* server, elysian_cbuf_t** cbuf_list_in, elysian_cbuf_t** cbuf_list_out, uint8_t end_of_stream);
};

typedef struct elysian_websocket_frame_t elysian_websocket_frame_t;
struct elysian_websocket_frame_t {
	uint8_t opcode;
	uint8_t* data;
	uint32_t len;
	uint32_t sent_len;
	elysian_websocket_frame_t* next;
};

elysian_websocket_frame_t* elysian_websocket_frame_allocate(elysian_t* server, uint32_t len);
void elysian_websocket_frame_deallocate(elysian_t* server, elysian_websocket_frame_t* frame);

elysian_err_t elysian_websocket_init(elysian_t* server);
elysian_err_t elysian_websocket_connected(elysian_t* server);
elysian_err_t elysian_websocket_cleanup(elysian_t* server);
elysian_err_t elysian_websocket_process (elysian_t* server);
elysian_err_t elysian_websocket_app_timer(elysian_t* server);
elysian_err_t elysian_websocket_ping_timer(elysian_t* server);

typedef struct elysian_websocket_t elysian_websocket_t;
struct elysian_websocket_t {
	const elysian_websocket_controller_t* controller;
	void* handler_args;
	
	elysian_websocket_frame_t* rx_frames;
	elysian_websocket_frame_t* tx_frames;
	
	elysian_websocket_flag_e flags;
	
	uint32_t rx_path_healthy_ms;
	uint32_t timer_interval_ms;
	
	//uint8_t pong_received;
	//uint16_t timer_ms;
	//uint16_t ping_timer_ms;
	//uint16_t rx_timer_ms;
	//uint32_t prev_timer_ms;
};

//typedef struct elysian_client_t elysian_client_t;
struct elysian_client_t{
    uint32_t id;
    elysian_socket_t socket;
    
	/*
	** Raw received stream
	*/
	elysian_cbuf_t* rcv_cbuf_list;
	
	elysian_isp_t isp;
  
	/*
	** Stream to be save into header/body file
	*/
	elysian_cbuf_t* store_cbuf_list;
    uint32_t store_cbuf_list_offset;
    uint32_t store_cbuf_list_size;
    
    elysian_httpreq_t httpreq;
    elysian_httpresp_t httpresp;
    elysian_mvc_t mvc;
    elysian_resource_t* resource;
	elysian_websocket_t websocket;
	
	uint8_t http_pipelining_enabled;
	
	//elysian_websocket_frame_t* websocket_frame;
	
	/*
	** Callback function/data set by user to notify that the current request
	** was processed. Can be used to clean any allocations made for the particular
	** request using controllers, for example to remove any files that where created
	** from the controller to serve the particular request.
	*/
	elysian_httpreq_onservice_handler_t httpreq_onservice_handler;
    void* httpreq_onservice_handler_data;
};


int elysian_strhex2uint(char* hexstr, uint32_t* dec);

#endif
