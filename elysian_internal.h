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


#define ELYSIAN_CBUF_LEN (256)


#define ELYSIAN_TIME_INFINITE  (uint32_t)(-1)


//typedef struct elysian_t elysian_t;


typedef struct elysian_client_t elysian_client_t;
typedef struct elysian_t elysian_t;
typedef struct elysian_schdlr_t elysian_schdlr_t;

typedef uint8_t (*elysian_authentication_cb_t)(elysian_t* server, char* url, char* username, char* password);

typedef enum {
	ELYSIAN_MEM_MALLOC_PRIO_NORMAL = 0,
	ELYSIAN_MEM_MALLOC_PRIO_HIGH,
}elysian_mem_malloc_prio_t;

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


elysian_cbuf_t* elysian_cbuf_alloc(elysian_t* server, uint8_t* data, uint32_t len);
void elysian_cbuf_free(elysian_t* server, elysian_cbuf_t* cbuf);
void elysian_cbuf_list_append(elysian_cbuf_t** cbuf_list, elysian_cbuf_t* cbuf_new);
void elysian_cbuf_list_free(elysian_t* server, elysian_cbuf_t* cbuf_list);
uint32_t elysian_cbuf_list_len(elysian_cbuf_t* cbuf_list);
elysian_err_t elysian_cbuf_list_split(elysian_cbuf_t** cbuf_list0, uint32_t size, elysian_cbuf_t** cbuf_list1);
elysian_err_t elysian_cbuf_rechain(elysian_t* server, elysian_cbuf_t** cbuf_list, uint32_t size);
uint8_t elysian_cbuf_strcmp(elysian_cbuf_t* cbuf, uint32_t index, char* str, uint8_t matchCase);
void elysian_cbuf_strcpy(elysian_cbuf_t* cbuf, uint32_t index0, uint32_t index1, char* str);
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
	ELYSIAN_HTTP_STATUS_CODE_200,
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

elysian_err_t elysian_http_request_headers_received(elysian_t* server);
elysian_err_t elysian_http_request_headers_parse(elysian_t* server);
elysian_err_t elysian_http_request_get_uri(elysian_t* server, char** uri);
elysian_err_t elysian_http_request_get_method(elysian_t* server, elysian_http_method_e* method_id);
elysian_err_t elysian_http_request_get_header(elysian_t* server, char* header_name, char** header_value);

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
uint32_t elysian_mem_available(elysian_mem_malloc_prio_t prio);

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
	char* name;
    uint8_t* ptr;
    uint32_t pos;
    uint32_t size;
};

typedef struct elysian_file_disk_t elysian_file_disk_t;
struct elysian_file_disk_t{
#ifdef ELYSIAN_FS_ENV_UNIX
    FILE* fd;
#elif ELYSIAN_FS_ENV_FATAFS
    FIL* fd;
#else

#endif
};


typedef struct elysian_fs_partition_t elysian_fs_partition_t;

typedef struct elysian_file_t elysian_file_t;
struct elysian_file_t{
	elysian_fs_partition_t* partition;
    elysian_file_status_t status;
	elysian_file_mode_t mode;
	
    union{
        elysian_file_ram_t ram;
        elysian_file_rom_t rom;
        elysian_file_disk_t disk;
    }descriptor;
};


typedef struct elysian_fs_partition_t elysian_fs_partition_t;
struct elysian_fs_partition_t{
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

elysian_err_t elysian_fs_ws_fopen(elysian_t* server, char* abs_path, elysian_file_mode_t mode, elysian_file_t* file);
elysian_err_t elysian_fs_ws_fsize(elysian_t* server, elysian_file_t* file, uint32_t* filesize);
elysian_err_t elysian_fs_ws_fseek(elysian_t* server, elysian_file_t* file, uint32_t seekpos);
elysian_err_t elysian_fs_ws_ftell(elysian_t* server, elysian_file_t* file, uint32_t* seekpos);
int elysian_fs_ws_fread(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size);
int elysian_fs_ws_fwrite(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size);
elysian_err_t elysian_fs_ws_fclose(elysian_t* server, elysian_file_t* file);
elysian_err_t elysian_fs_ws_fremove(elysian_t* server, char* vrt_path);



//uint32_t elysian_mem_usage(void);

/*
** Strings
*/
elysian_err_t elysian_strstr_file(elysian_t* server, elysian_file_t* file, uint32_t offset, char* pattern1, char* pattern2, uint8_t match_case, uint32_t* index1, uint32_t* index2);
elysian_err_t elysian_strncpy_file(elysian_t* server, elysian_file_t* file, uint32_t offset, char* str, uint32_t n);
void elysian_str_trim(elysian_t* server, char* str, char* ignore_prefix_chars, char* ignore_suffix_chars);


/* 
** MVC
*/
typedef struct elysian_mvc_attribute_t elysian_mvc_attribute_t;
struct elysian_mvc_attribute_t{
    char* name;
    char* value;
    elysian_mvc_attribute_t* next;
};

typedef elysian_err_t (*elysian_mvc_controller_cb_t)(elysian_t* server);
	
typedef struct elysian_mvc_controller_t elysian_mvc_controller_t;
struct elysian_mvc_controller_t{
    const char* url;
    elysian_mvc_controller_cb_t cb;
    //uint8_t http_methods_mask;
	elysian_mvc_controller_flag_e flags;

    elysian_mvc_controller_t* next;
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
    elysian_mvc_attribute_t* attributes;
	elysian_mvc_alloc_t* allocs;
	elysian_req_param_t* req_params;
};

typedef void (*elysian_reqserved_cb_t)(elysian_t* server, void* ptr);

void elysian_mvc_init(elysian_t* server);
elysian_err_t elysian_mvc_configure(elysian_t* server);
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
	uint32_t data_index_cur;
};


/* 
** Pre-processors
*/
typedef struct elysian_resource_t elysian_resource_t;
struct elysian_resource_t{
    uint8_t openned;
	elysian_err_t (*open)(elysian_t* server, uint32_t seekpos, uint32_t* filesz);
    elysian_err_t (*read)(elysian_t* server, uint8_t* readbuf, uint32_t readbufsz, uint32_t* readbufszactual);
    elysian_err_t (*close)(elysian_t* server);
    elysian_file_t file;
    void* priv;
};

void elysian_resource_init(elysian_t* server);
elysian_err_t elysian_resource_open(elysian_t* server, uint32_t seekpos, uint32_t* filesz);
uint8_t elysian_resource_isopened(elysian_t* server);
elysian_err_t elysian_resource_read(elysian_t* server, uint8_t* readbuf, uint32_t readbufsz, uint32_t* readbufszactual);
elysian_err_t elysian_resource_close(elysian_t* server);
    

/* 
** Scheduler
*/
typedef enum{
    elysian_schdlr_EV_ENTRY,
    elysian_schdlr_EV_READ, 
    elysian_schdlr_EV_POLL,
    //elysian_schdlr_EV_CLOSED,
    //elysian_schdlr_EV_TIMER,
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
    uint32_t timeout_delta;
	uint32_t timeout_delta_init;
    uint32_t poll_delta;
    uint32_t poll_delta_init;
    
    elysian_client_t* client;
    elysian_cbuf_t* cbuf_list;
    
	elysian_schdlr_task_t* next;
    elysian_schdlr_task_t* prev;
	
	uint8_t last_malloc_succeed;
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
void elysian_schdlr_state_timeout_set(elysian_t* server, uint32_t timeout_delta);
void elysian_schdlr_state_timeout_reset(elysian_t* server);
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




/* 
** HTTP Request
*/
typedef enum{
    ELYSIAN_HTTP_RANGE_SOF = 0, /* Start of file */
    ELYSIAN_HTTP_RANGE_EOF = 0xfffffffe, /* End of file */
    ELYSIAN_HTTP_RANGE_WF = 0xffffffff, /* Whole file */
}elysian_httpreq_range_t;

typedef enum{
    ELYSIAN_HTTP_CONTENT_TYPE_APPLICATION__X_WWW_FORM_URLENCODED,
    ELYSIAN_HTTP_CONTENT_TYPE_MULTIPART__FORM_DATA,
    ELYSIAN_HTTP_CONTENT_TYPE_NA,
}elysian_http_content_type_t;

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
    uint8_t expect_status_code;
	
    uint16_t headers_len;
    uint32_t body_len;
    
	elysian_file_t headers_file;
    char headers_filename[24 + 14];  // max strlen(ELYSIAN_FS_ROM_x_ROOT) + strlen("/*_") + strlen(max uint32) + \0
    
	elysian_file_t body_file;
    char body_filename[24 + 14];  // max strlen(ELYSIAN_FS_ROM_x_ROOT) + strlen("/*_") + strlen(max uint32) + \0
	
    uint32_t range_start;
    uint32_t range_end;
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
	** Flag indicating if connection will be kept alive or closed after
	** the current HTTP response is sent.
	*/
    uint8_t keep_alive;
	/*
	** Status code used for the HTTP response
	*/
    elysian_http_status_code_e status_code;
	
	elysian_http_status_code_e fatal_status_code;
	
	uint8_t attempts;
	
	/*
	** Holds the redirection URL specified from a controller
	*/
    char* redirection_url;
	/* 
	** The size of the whole resource that requested by the HTTP Client or specified by the Controller.
	** This size or part of it (Partial or HEAD HTTP request) will be transmitted in the HTTP body.
	*/
	uint32_t resource_size; 
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
** Client
*/



//typedef struct elysian_client_t elysian_client_t;
struct elysian_client_t{
    uint32_t id;
    elysian_socket_t socket;
    
	elysian_cbuf_t* rcv_cbuf_list;
    uint32_t rcv_cbuf_list_offset0;
    uint32_t rcv_cbuf_list_offset1;
    
    elysian_httpreq_t httpreq;
    elysian_httpresp_t httpresp;
    elysian_mvc_t mvc;
    elysian_resource_t* resource;
	uint8_t http_pipelining_enabled;
	
	/*
	** Callback function/data set by user to notify that the current request
	** was processed. Can be used to clean any allocations made for the particular
	** request using controllers, for example to remove any files that where created
	** from the controller to serve the particular request.
	*/
	elysian_reqserved_cb_t reqserved_cb;
    void* reqserved_cb_data;
};




#endif
