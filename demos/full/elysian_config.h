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

#ifndef __ELYSIAN_CONFIG_H
#define __ELYSIAN_CONFIG_H

//#define ELYSIAN_ENV_UNIX
//#define ELYSIAN_ENV_WINDOWS
//#define ELYSIAN_ENV_EMBEDDED

#if defined (ELYSIAN_ENV_UNIX)
	#define ELYSIAN_OS_ENV_UNIX
	#define ELYSIAN_TCPIP_ENV_UNIX
	#define ELYSIAN_FS_ENV_UNIX
#elif defined (ELYSIAN_ENV_WINDOWS)
	#define ELYSIAN_OS_ENV_WINDOWS
	#define ELYSIAN_TCPIP_ENV_WINDOWS
	#define ELYSIAN_FS_ENV_WINDOWS
#else
	/*
	** Select OS
	*/
	#define ELYSIAN_OS_ENV_CHIBIOS
	//#	define ELYSIAN_OS_ENV_FREERTOS

	/*
	** Choose TCP/IP stack
	*/
	#define ELYSIAN_TCPIP_ENV_LWIP

	/*
	** Choose Filesystem
	*/
	#define ELYSIAN_FS_ENV_FATFS
#endif


/*
** Configuration options
*/
#define ELYSIAN_MAX_CLIENTS_NUM               		(uint32_t)(10)

/*
* Specify if the underlying TCP/IP enviroment suuports socket select() API.
* If set to (0), the select() API will be internally emulated using polling with exponential backoff. Not thoroughly tested!
* If set to (1), then the native select() API will be used instead of polling, offering better power consumption and responsiveness.
*/
#define ELYSIAN_SOCKET_SELECT_SUPPORTED				(1)

/*
** Specify the maximum amount of memory that the Web Server is going to work with.
*/
#define ELYSIAN_MAX_MEMORY_USAGE_KB	    			(10)

/*
** Specify the maximum size of HTTP body that should be accepted by the server.
** This is specifically for HTTP requests that are configured to be stored in RAM.
*/
#define ELYSIAN_MAX_HTTP_BODY_SIZE_KB_RAM	    		(3)

/*
** Specify the maximum size of HTTP body that should be accepted by the server.
** This is specifically for HTTP requests that are configured to be stored in DISK.
*/
#define ELYSIAN_MAX_HTTP_BODY_SIZE_KB_DISK	    		(5 * 1024)

/*
** The size of buffer that will be allocated to send the HTTP body
** part of the message message. 
*/
#define ELYSIAN_HTTP_RESPONSE_BODY_BUF_SZ_MAX   	(1400)
#define ELYSIAN_HTTP_RESPONSE_BODY_BUF_SZ_MIN  		(ELYSIAN_HTTP_RESPONSE_BODY_BUF_SZ_MAX/8)

/*
** Define the maximum path len supported. Files we fail to open/remove if the path is longer than the maximum allowed.
** Note: Increasing the size of this variable may require an increase of the stack size of the ELYSIAN thread as well.
*/
#define ELYSIAN_FS_MAX_PATH_LEN						(256)


/* 
** DISK partition which refers to resources stored into the hard disk or external SDCARD.
*/
#define ELYSIAN_FS_EXT_ABS_ROOT						"/fs_ext"

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
**	 index.html is located to DISK, and will be requested as {ELYSIAN_FSDISK_ABS_ROOT}index.html from elysian_port_fsdisk_fopen()
*/
#define ELYSIAN_FS_INDEX_HTML_VRT_ROOT				ELYSIAN_FS_ROM_VRT_ROOT

#endif
