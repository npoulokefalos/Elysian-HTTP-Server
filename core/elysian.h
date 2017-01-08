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

#define ELYSIAN_PRINTF                	printf
#define ELYSIAN_LOG(msg, ...)         	do { ELYSIAN_PRINTF("[%u] ELYSIAN_LOG %s():%d: " msg "\r\n", elysian_time_now(), __func__, __LINE__, ##__VA_ARGS__);} while(0);
#define ELYSIAN_LOG_ERR(msg, ...)     	do { ELYSIAN_PRINTF("[%u] ELYSIAN_ERR %s():%d: " msg "\r\n", elysian_time_now(), __func__, __LINE__, ##__VA_ARGS__);} while(0);
#define ELYSIAN_ASSERT(cond)     		if (!(cond)) { ELYSIAN_PRINTF("[%u] ELYSIAN_ASSERT Function %s(), Line %d\r\n", elysian_time_now(), __func__, __LINE__); while(1){} }
#define ELYSIAN_UNUSED_ARG(arg)       	(void)(arg)

#define ELYSIAN_INDEX_OOB32				((uint32_t) 0xFFFFFFFF) /* Index out of bounds */

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
	ELYSIAN_MVC_CONTROLLER_FLAG_USE_EXT_FS		= 1 << 3, // While processing, temporary save the HTTP body using the EXT filesystem (instead of the RAM filesystem)
}elysian_mvc_controller_flag_e;


#include "elysian_internal.h"

struct elysian_t{
	uint16_t listening_port;
    elysian_schdlr_t scheduler;
    elysian_mvc_controller_t* controllers;
	elysian_authentication_cb_t authentication_cb;
	const elysian_file_rom_t* rom_fs;
	uint32_t starvation_detection_t0;
};

/*======================================================================================================================================
 Core API                                                      															
 ======================================================================================================================================*/
/* 
** @brief 		Allocates space for a new server instance
**
** @param[in]	server The server instance
**
** @return  	The allocated server instance or NULL
 */
elysian_t* elysian_new(void);

/* 
** @brief 		Starts the particular server instance
**
** @param[in]	server 				The server instance
** @param[in]	port				Server's listening port
** @param[in]	rom_fs				Pointer to the list of files which constitute the ROM filesystem.
** @param[in]	authentication_cb	Authentication callback, used for base access authentication
**
** @retval  	ELYSIAN_ERR_OK		On success
** @retval  	ELYSIAN_ERR_FATAL	On failure
 */
elysian_err_t elysian_start(elysian_t* server, uint16_t port, const elysian_file_rom_t rom_fs[], elysian_authentication_cb_t authentication_cb);

/* 
** @brief 		Executes the server shceduler for the particular interval.
**
** @param[in]	server 				The server instance
** @param[in]	interval_ms			Number of milliseconds that the function will block.
**
** @retval  	ELYSIAN_ERR_OK		On success
** @retval  	ELYSIAN_ERR_FATAL	On failure.
 */
elysian_err_t elysian_poll(elysian_t* server, uint32_t interval_ms);

/* 
** @brief 		Stops the particular server instance
**
** @param[in]	server	The server instance
 */
void elysian_stop(elysian_t* server);

/*======================================================================================================================================
 MVC API                                                     															
 ======================================================================================================================================*/

/* 
** @brief 		Registers a MVC controller. The web server will trigger the controller handler
**				when an HTTP request matches the specific HTTP URL and HTTP method criteria.
**
** @param[in]	server 		The server instance
** @param[in]	url			The HTTP request URL that will trigger the specific controller handler
** @param[in]	handler		The handler that will be triggered when the specific URL will be requested
** @param[in]	flags		Controller flags indicating the HTTP method that will trigger the controller
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
										
										buf = elysian_mem_malloc(server, 512, ELYSIAN_MEM_MALLOC_PRIO_NORMAL); // user allocated memory block
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
elysian_err_t elysian_mvc_controller(elysian_t* server, const char* url, elysian_mvc_controller_handler_t handler, elysian_mvc_controller_flag_e flags);

/* 
** @brief 		Returns the currently served HTTP client instance
**
** @param[in]	server	The server instance
**
** @return		The currently served HTTP client instance
 */
elysian_client_t* elysian_mvc_client(elysian_t* server);

elysian_err_t elysian_mvc_attribute_set(elysian_t* server, char* name, char* value);

elysian_err_t elysian_mvc_httpreq_url_get(elysian_t* server, char** url);
elysian_err_t elysian_mvc_httpreq_header_get(elysian_t* server, char* header_name, char** header_value);

elysian_err_t elysian_mvc_param_get(elysian_t* server, char* param_name, elysian_req_param_t* req_param);
elysian_err_t elysian_mvc_param_size(elysian_t* server, elysian_req_param_t* req_param, uint32_t* param_size);
elysian_err_t elysian_mvc_param_filename(elysian_t* server, elysian_req_param_t* req_param, char** filename);
elysian_err_t elysian_mvc_param_read(elysian_t* server, elysian_req_param_t* req_param, uint32_t offset, uint8_t* buf, uint32_t buf_size, uint32_t* read_size);
elysian_err_t elysian_mvc_param_get_bytes(elysian_t* server, char* param_name, uint8_t** param_value, uint32_t* param_size, uint8_t* param_found);
elysian_err_t elysian_mvc_param_get_str(elysian_t* server, char* param_name, char** param_value, uint8_t* param_found);
elysian_err_t elysian_mvc_param_get_uint(elysian_t* server, char* param_name, uint32_t* param_value, uint8_t* param_found);
elysian_err_t elysian_mvc_param_get_int(elysian_t* server, char* param_name, int32_t* param_value, uint8_t* param_found);

elysian_err_t elysian_mvc_view_set(elysian_t* server, char* view);
elysian_err_t elysian_mvc_redirect(elysian_t* server, char* redirection_url);

elysian_err_t elysian_mvc_httpreq_served_handler(elysian_t* server, elysian_reqserved_cb_t cb, void* data);

/*======================================================================================================================================
 Memory Management API                                          															
 ======================================================================================================================================*/
void* elysian_mem_malloc(elysian_t* server, uint32_t size, elysian_mem_malloc_prio_t prio);
void elysian_mem_free(elysian_t* server, void* ptr);
uint32_t elysian_mem_usage(void);

/*======================================================================================================================================
 Time                                                															
 ======================================================================================================================================*/
uint32_t elysian_time_now(void);

/*======================================================================================================================================
 Filesystem API                                             															
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
 Strings manipulation API                                           															
 ======================================================================================================================================*/
char* elysian_strstr(char *haystack, char *needle);
char* elysian_strcasestr(char *haystack, char *needle);
elysian_err_t elysian_sprintf(char * buf, const char* format, ... );
elysian_err_t elysian_str2uint(char* buf, uint32_t* uint_var);

#endif
