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

#ifndef __ELYSIAN_H
#define __ELYSIAN_H

#include "elysian_config.h"

#define ELYSIAN_PRINTF                printf
#define ELYSIAN_LOG(msg, ...)         do{ELYSIAN_PRINTF("[%u] LOG %s():%d: " msg "\r\n", elysian_time_now(), __func__, __LINE__, ##__VA_ARGS__);}while(0);
#define ELYSIAN_LOG_ERR(msg, ...)     do{ELYSIAN_PRINTF("[%u] **** LOG %s():%d: " msg "\r\n", elysian_time_now(), __func__, __LINE__, ##__VA_ARGS__);}while(0);
#define ELYSIAN_ASSERT(cond,msg,...)  if(!(cond)){ELYSIAN_PRINTF("[%u] **** LOG %s():%d: " msg "\r\n", elysian_time_now(), __func__, __LINE__, ##__VA_ARGS__);while(1){}}
#define ELYSIAN_UNUSED_ARG(arg)       (void)(arg)

#define ELYSIAN_INDEX_OOB32			((uint32_t)0xffffffff) /* Index out of bounds */

typedef enum{
    ELYSIAN_ERR_OK = 0,
    ELYSIAN_ERR_POLL, /* Operation had a temporary memory error and requested to be called again */
    ELYSIAN_ERR_READ, /* Operation needs more input data to complete processing */
	ELYSIAN_ERR_BUF,
    ELYSIAN_ERR_FATAL,
    ELYSIAN_ERR_NOTFOUND,
	ELYSIAN_ERR_AUTH,
    //ELYSIAN_ERR_EOF,
    //ELYSIAN_ERR_FILENOTFOUND,
}elysian_err_t;



typedef enum{
	ELYSIAN_MVC_CONTROLLER_FLAG_NONE 			= 0,
	ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_GET		= 1 << 0, // HTTP GET and HEAD
	ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_POST 		= 1 << 1,
	ELYSIAN_MVC_CONTROLLER_FLAG_HTTP_PUT 		= 1 << 2,
	ELYSIAN_MVC_CONTROLLER_FLAG_SAVE_TO_DISK 	= 1 << 3, // Save HTTP body to disk
}elysian_mvc_controller_flag_e;


#include "elysian_internal.h"

struct elysian_t{
	uint16_t listening_port;
    elysian_schdlr_t scheduler;
    elysian_mvc_controller_t* controllers;
	elysian_authentication_cb_t authentication_cb;
	const elysian_file_rom_t* rom_fs;
	uint32_t rom_fs_size;
	uint32_t starvation_detection_t0;
};

/*======================================================================================================================================
 Setup                                                      															
 ======================================================================================================================================*/
elysian_t* elysian_new(void);

elysian_err_t elysian_rom_fs(elysian_t* server, const elysian_file_rom_t rom_fs[], uint32_t rom_fs_size);

elysian_err_t elysian_start(elysian_t* server, uint16_t port, elysian_authentication_cb_t authentication_cb);
elysian_err_t elysian_poll(elysian_t* server, uint32_t intervalms);
void elysian_stop(elysian_t* server);

/*======================================================================================================================================
 Controllers and MVC                                                       															
 ======================================================================================================================================*/
elysian_err_t elysian_mvc_controller_add(elysian_t* server, const char* url, elysian_mvc_controller_cb_t cb, elysian_mvc_controller_flag_e flags);
elysian_err_t elysian_mvc_attribute_set(elysian_t* server, char* name, char* value);

elysian_err_t elysian_mvc_set_reqserved_cb(elysian_t* server, elysian_reqserved_cb_t cb, void* data);


elysian_client_t* elysian_current_client(elysian_t* server);
elysian_err_t elysian_mvc_set_view(elysian_t* server, char* view);
elysian_err_t elysian_mvc_redirect(elysian_t* server, char* redirection_url);

elysian_err_t elysian_mvc_get_requested_url(elysian_t* server, char** requested_url);
elysian_err_t elysian_mvc_get_param(elysian_t* server, char* param_name, elysian_req_param_t* req_param);
elysian_err_t elysian_mvc_param_size(elysian_t* server, elysian_req_param_t* req_param, uint32_t* param_size);
elysian_err_t elysian_mvc_read_param(elysian_t* server, elysian_req_param_t* req_param, uint8_t* buf, uint32_t buf_size, uint32_t* read_size);
elysian_err_t elysian_mvc_get_param_bytes(elysian_t* server, char* param_name, uint8_t** param_value, uint8_t* param_found);
elysian_err_t elysian_mvc_get_param_str(elysian_t* server, char* param_name, char** param_value, uint8_t* param_found);
elysian_err_t elysian_mvc_get_param_uint(elysian_t* server, char* param_name, uint32_t* param_value, uint8_t* param_found);
elysian_err_t elysian_mvc_get_param_int(elysian_t* server, char* param_name, int32_t* param_value, uint8_t* param_found);

/*======================================================================================================================================
 Memory Management                                                 															
 ======================================================================================================================================*/
void* elysian_mem_malloc(elysian_t* server, uint32_t size, elysian_mem_malloc_prio_t prio);
void elysian_mem_free(elysian_t* server, void* ptr);
uint32_t elysian_mem_usage(void);

/*======================================================================================================================================
 Time                                                															
 ======================================================================================================================================*/
uint32_t elysian_time_now(void);

/*======================================================================================================================================
 Filesystem                                                															
 ======================================================================================================================================*/
void elysian_fs_finit(elysian_t* server, elysian_file_t* file);
elysian_err_t elysian_fs_fopen(elysian_t* server, char* vrt_path, elysian_file_mode_t mode, elysian_file_t* file);
uint8_t elysian_fs_fisopened(elysian_t* server, elysian_file_t* file);
elysian_err_t elysian_fs_fsize(elysian_t* server, elysian_file_t* file, uint32_t* filesize);
elysian_err_t elysian_fs_fseek(elysian_t* server, elysian_file_t* file, uint32_t seekpos);
elysian_err_t elysian_fs_ftell(elysian_t* server, elysian_file_t* file, uint32_t* seekpos);
elysian_err_t elysian_fs_fread(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size, uint32_t* actualreadsize);
elysian_err_t elysian_fs_fwrite(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size, uint32_t* actual_write_sz);
elysian_err_t elysian_fs_fclose(elysian_t* server, elysian_file_t* file);
elysian_err_t elysian_fs_fremove(elysian_t* server, char* vrt_path);

/*======================================================================================================================================
 Strings                                                															
 ======================================================================================================================================*/
char* elysian_strstr(char *haystack, char *needle);
char* elysian_strcasestr(char *haystack, char *needle);
elysian_err_t elysian_sprintf(char * buf, const char* format, ... );

#endif
