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

#define ELYSIAN_ENV_UNIX
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

#define ELYSIAN_SERVER_NAME							"Elysian Web Server"

/*
** Virtual and absolute paths for the RAM, ROM, DISK and internal Web Server's partitions. 
**
** When an HTTP request arrives and the virtual path of the requested reource is {ELYSIAN_FS_xxx_VRT_ROOT}resource, 
** the web server is going to open the file from the absolute path {ELYSIAN_FS_xxx_ABS_ROOT}resource as follows:
**
** - ELYSIAN_FS_xxx_VROOT is ELYSIAN_FS_RAM_VRT_VROOT:
**	 The file is located into the RAM partition, and elysian_fs_ram_fopen will be called with path = {ELYSIAN_FS_RAM_ABS_ROOT}resource
**
** - ELYSIAN_FS_xxx_VROOT is ELYSIAN_FS_ROM_VRT_ROOT:
**	 The file is located into the ROM partition, and elysian_port_fs_rom_fopen() will be called with path = {ELYSIAN_FS_ROM_ABS_ROOT}resource
**
** - ELYSIAN_FS_xxx_VROOT is ELYSIAN_FS_DISK_VRT_ROOT:
**	 The file is located into the DISK partition, and elysian_port_fs_disk_fopen() will be called with path = {ELYSIAN_FS_DISK_ABS_ROOT}resource
**
** All web pages should refer to the virtual resource paths which provide a hint to the web server for the partition (RAM, ROM, DISK, WS)
** of the particular resource. The Web Server transforms the virtual path to an absolute path according to user preferences.
*/

/* 
** RAM partition which provides an abstraction so data stored in either RAM/ROM/DISK can be handled in the same manner.
** It is mostly used internally by the web server (for example to store small POST requests), but it can be used from user too.
*/
#define ELYSIAN_FS_RAM_VRT_ROOT						"/fs_root/ram"
#define ELYSIAN_FS_RAM_ABS_ROOT						""

/* 
** ROM partition which refers to resources stored as "const" variables in the .text area of the program
** (For example microcontroller's internal FLASH memory)
*/
#define ELYSIAN_FS_ROM_VRT_ROOT						"/fs_root/rom" 
#define ELYSIAN_FS_ROM_ABS_ROOT						""

/* 
** DISK partition which refers to resources stored into the hard disk or external SDCARD.
*/
#define ELYSIAN_FS_DISK_VRT_ROOT					"/fs_root/disk"
#define ELYSIAN_FS_DISK_ABS_ROOT					"/fs_root/disk"

/* 
** Web Server's internal partition for sending default content, for example default error pages (404, 500, ..)
** when user-defined error pages are not provided in any other user partition.
*/
#define ELYSIAN_FS_WS_VRT_ROOT						"/fs_root/ws"
#define ELYSIAN_FS_WS_ABS_ROOT						""

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
** - ELYSIAN_FS_INDEX_HTML_VRT_ROOT == ELYSIAN_FS_DISK_VRT_ROOT : 
**	 index.html is located to DISK, and will be requested as {ELYSIAN_FSDISK_ABS_ROOT}index.html from elysian_port_fsdisk_fopen()
*/
#define ELYSIAN_FS_INDEX_HTML_VRT_ROOT				ELYSIAN_FS_ROM_VRT_ROOT

/*
** Special file that may be used in responses with empty body. It is located to the ELYSIAN_FS_WS_VRT_ROOT partition.
** CAUTION: Do not change the partition of this file, unless it is guaranteed that this file will always exist to
**			the specified partition.
*/
#define ELYSIAN_FS_EMPTY_FILE_NAME 					"/empty.file"
#define ELYSIAN_FS_EMPTY_FILE_VRT_ROOT 				ELYSIAN_FS_WS_VRT_ROOT

/*
** Special file that may be used for testing. It is located to the ELYSIAN_FS_WS_VRT_ROOT partition.
** CAUTION: Do not change the partition of this file, unless it is guaranteed that it be always existing to
**			the specified partition.
*/
#define ELYSIAN_FS_HUGE_FILE_NAME 					"/huge.file"
#define ELYSIAN_FS_HUGE_FILE_VRT_ROOT 				ELYSIAN_FS_WS_VRT_ROOT

/*
** Define the maximum path len supported. Files we fail to open/remove if the path is longer than the maximum allowed.
** Note: Increasing the size of this variable may require an increase of the stack size of the ELYSIAN thread as well.
*/
#define ELYSIAN_FS_MAX_PATH_LEN						(256)
#endif
