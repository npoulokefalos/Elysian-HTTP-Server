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
** The maximum number of concurrent HTTP connections.
*/
#define ELYSIAN_MAX_CLIENTS_NUM			   			(uint32_t) (10)

/*
* Specify if the underlying TCP/IP enviroment suuports socket select() API.
* If set to (0), the select() API will be internally emulated using polling with exponential backoff. Not thoroughly tested!
* If set to (1), the native select() API will be used instead of polling, offering better power consumption and responsiveness.
*/
#define ELYSIAN_SOCKET_SELECT_SUPPORTED				(1)

/*
** The maximum amount of memory (in Kilobytes) that the Web Server could allocate.
*/
#define ELYSIAN_MAX_MEMORY_USAGE_KB					(10)

/*
** The maximum HTTP body size the HTTP server is allowed to process.
** This is specifically for HTTP requests that are configured to be stored to the RAM memory device.
** The actual size will be the minimum of {ELYSIAN_MAX_MEMORY_USAGE_KB, ELYSIAN_MAX_HTTP_BODY_SIZE_KB_RAM}.
*
** Note: This is specifically for HTTP requests that are configured to be stored in RAM. 
** Bigger HTTP body sizes can been supported when an EXT memory device is used. 
** Check the ELYSIAN_MVC_CONTROLLER_FLAG_USE_EXT_FS flag.
*/
#define ELYSIAN_MAX_HTTP_BODY_SIZE_KB_RAM	   		(3)

/*
** The maximum HTTP body size the HTTP server is allowed to process.
** This is specifically for HTTP requests that are configured to be stored to the EXT memory device.
** Check the ELYSIAN_MVC_CONTROLLER_FLAG_USE_EXT_FS flag.
*/
#define ELYSIAN_MAX_HTTP_BODY_SIZE_KB_EXT	  		(5 * 1024)

/*
** The Maximum segment size (MSS, in bytes) that will be used to send the HTTP Response.
** The actual value depends on the network interface and is usually less than 1460 bytes. 
*/
#define ELYSIAN_HTTP_RESPONSE_BODY_BUF_SZ_MAX   	(1400)

/*
** The maximum file path len supported. Files will fail to be opened/removed if the path is longer than the maximum allowed.
** Note: Increasing the size of this variable will require an increase of the stack size of the ELYSIAN thread as well.
*/
#define ELYSIAN_FS_MAX_PATH_LEN						(256)

/* 
** Absolute root path of the EXT storage device (SD Card, USB, Hard Disk, ..)
** HTTP requests reffering to the virtual path "/fs_ext/path/to/file" will be served using the 
** elysian_port_fs_ext_fopen() API call with absolute path equal to "{ELYSIAN_FS_EXT_ABS_ROOT}/path/to/file".
*/
#define ELYSIAN_FS_EXT_ABS_ROOT						"/fs_ext"

/*
** Specify the memory device for the "index.html" web page. 
**
** When an HTTP request arrives and the requested resource is "/", the web server does not know the 
** location of the index.html page (there is no memory device prefix inside "/")
** and it should be specified here:
**
** - ELYSIAN_FS_INDEX_HTML_VRT_ROOT == ELYSIAN_FS_ROM_VRT_ROOT : 
**	 index.html is located to the ROM memory device and will be served using the
** 	 elysian_fs_rom_fopen() API call with absolute path equal to "/index.html".
**	 To build it, put the "index.html" page under the "fs_rom" directory and use 
**   the "makefsdata.py" utility that generates the ROM memory device file structure.
**
** - ELYSIAN_FS_INDEX_HTML_VRT_ROOT == ELYSIAN_FS_EXT_VRT_ROOT : 
**	 index.html is located to the EXT(ernal) memory device and will be served using the 
** 	 elysian_port_fs_ext_fopen() API call with absolute path equal to "{ELYSIAN_FS_EXT_ABS_ROOT}/path/to/file".
*/
#define ELYSIAN_FS_INDEX_HTML_VRT_ROOT				ELYSIAN_FS_ROM_VRT_ROOT

#endif
