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

#ifdef ELYSIAN_TCPIP_ENV_UNIX
	#include "errno.h"
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <sys/wait.h>
#endif

#ifdef ELYSIAN_TCPIP_ENV_WINDOWS
	#include <winsock2.h>
	#include <windows.h>
#endif

#include <sys/types.h> 
#include <fcntl.h> /* Added for the nonblocking socket */
#include <strings.h> //bzero
#include <errno.h> //bzero


/* ---------------------------------------------------------------------------------------------------------------------------------------
** Time
** ------------------------------------------------------------------------------------------------------------------------------------ */

uint32_t elysian_port_time_now(){
#if defined(ELYSIAN_OS_ENV_UNIX)
    struct timeval  tv;
    gettimeofday(&tv, NULL);
    return  ((tv.tv_sec) * 1000) + ((tv.tv_usec) / 1000) ;
#elif defined(ELYSIAN_OS_ENV_CHIBIOS)
    return chTimeNow();
#elif defined(ELYSIAN_OS_ENV_WINDOWS)
	SYSTEMTIME time;
	GetSystemTime(&time);
	uint32_t millis = (time.wSecond * 1000) + time.wMilliseconds; /* Rolls every 59999 ms */
	return millis;
#else
    return 0;
#endif
}

void elysian_port_time_sleep(uint32_t ms){
#if defined(ELYSIAN_OS_ENV_UNIX)
    usleep(1000 * ms);
#elif defined(ELYSIAN_OS_ENV_CHIBIOS)
	chThdSleepMilliseconds(ms);
#elif defined(ELYSIAN_OS_ENV_WINDOWS)
	Sleep(ms);
#else

#endif
}

/*
** OS specific call to suspend the current thread if other ones with the same priority
** are ready to be executed. At its simpliest form it can be set to elysian_port_time_sleep(1).
*/
void elysian_port_thread_yield(){
#if defined(ELYSIAN_OS_ENV_UNIX)
    elysian_port_time_sleep(1);
#elif defined(ELYSIAN_OS_ENV_CHIBIOS)
	chThdYield();
#elif defined(ELYSIAN_OS_ENV_WINDOWS)
	elysian_port_time_sleep(1);
#else

#endif
}

/* ---------------------------------------------------------------------------------------------------------------------------------------
** Memory
** ------------------------------------------------------------------------------------------------------------------------------------ */

void* elysian_port_mem_malloc(uint32_t size){
#if defined(ELYSIAN_OS_ENV_UNIX)
    return malloc(size);
#elif defined(ELYSIAN_OS_ENV_CHIBIOS)
    return chHeapAlloc(NULL, size);
#elif defined(ELYSIAN_OS_ENV_WINDOWS)
	return malloc(size);
#else
    return NULL;
#endif
}

void elysian_port_mem_free(void* ptr){
#if defined(ELYSIAN_OS_ENV_UNIX)
    free(ptr);
#elif defined(ELYSIAN_OS_ENV_CHIBIOS)
	chHeapFree(ptr);
#elif defined(ELYSIAN_OS_ENV_WINDOWS)
	free(ptr);
#else

#endif
}

/* ---------------------------------------------------------------------------------------------------------------------------------------
** Sockets
** ------------------------------------------------------------------------------------------------------------------------------------ */

/* 
** @brief 		Get web server's hostname or IP (for example 192.168.1.1 or xxx.com)
**
** @param[in]	server The server instance
** @param[out] 	hostname Server's hostname
**
** @retval ELYSIAN_ERR_OK  		The operation was succesfull. hostname stores the hostname of the server.
** @retval ELYSIAN_ERR_FATAL  	There was a fatal error. hostname is invalid. Any clients requested the hostname will be closed.
 */
elysian_err_t elysian_port_hostname_get(char hostname[64]){
	strcpy(hostname, "localhost");
	return ELYSIAN_ERR_OK;
}

/* 
** @brief 		Make the specific socket blocking or non-blocking
**
** @param[in]	server The server instance
** @param[in] 	socket The socket to be made blocking or non-blocking
** @param[in] 	blocking If 1, make the socket blocking. If 0, make the socket non-blocking
**
** @retval ELYSIAN_ERR_OK  		The server socket was created succesfully. server_socket stores the socket descriptor.
** @retval ELYSIAN_ERR_FATAL  	There was a fatal error. The server connection was not created. server_socket is invalid.
 */
void elysian_setblocking(elysian_socket_t* socket, uint8_t blocking){
#if defined(ELYSIAN_TCPIP_ENV_UNIX)
    int flags = fcntl(socket->fd, F_GETFL, 0);
    if (flags < 0) {
        ELYSIAN_LOG("fcntl");
        while(1){}
    };
    flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
    fcntl(socket->fd, F_SETFL, flags);
#elif defined(ELYSIAN_TCPIP_ENV_WINDOWS)
	unsigned long flags = !!blocking;
	ioctlsocket(socket->fd, FIONBIO, &flags);
#else

#endif
}

/* 
** @brief 		Close the specific socket
**
** @param[in]	server The server instance
** @param[in] 	socket The socket to be closed
 */
void elysian_port_socket_close(elysian_socket_t* socket){
#if defined(ELYSIAN_TCPIP_ENV_UNIX)
    close(socket->fd);
#elif defined(ELYSIAN_TCPIP_ENV_WINDOWS)
	closesocket(socket->fd);
#else

#endif
}

/* 
** @brief 		Create a server socket and bind it to a specific port
**
** @param[in]	server The server instance
** @param[in] 	port The listening port of the server socket
** @param[out] 	server_socket The created server socket
**
** @retval ELYSIAN_ERR_OK  		The server socket was created succesfully. server_socket stores the socket descriptor.
** @retval ELYSIAN_ERR_FATAL  	There was a fatal error. The server connection was not created. server_socket is invalid.
 */
elysian_err_t elysian_port_socket_listen(uint16_t port, elysian_socket_t* server_socket){
#if defined(ELYSIAN_TCPIP_ENV_UNIX)
    int server_socket_fd;
    struct sockaddr_in server_sockaddrin;
    if ((server_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        return ELYSIAN_ERR_FATAL;
    }
	
    server_sockaddrin.sin_family = AF_INET;
    server_sockaddrin.sin_port = htons(port);
    server_sockaddrin.sin_addr.s_addr = INADDR_ANY;
    memset(&(server_sockaddrin.sin_zero), 0, sizeof(server_sockaddrin.sin_zero));

    if (bind(server_socket_fd, (struct sockaddr *)&server_sockaddrin, sizeof(struct sockaddr)) == -1) {
        ELYSIAN_LOG_ERR("bind error!");
        return ELYSIAN_ERR_FATAL;
    }

    if (listen(server_socket_fd, ELYSIAN_MAX_CLIENTS_NUM) == -1) {
        ELYSIAN_LOG_ERR("listen error!");
        return ELYSIAN_ERR_FATAL;
    }
    
    server_socket->fd = server_socket_fd;
#elif defined(ELYSIAN_TCPIP_ENV_WINDOWS)
	SOCKET server_socket_fd;
    struct sockaddr_in server_sockaddrin;
	WSADATA wsaData;
	
	//Initialize Winsock
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        ELYSIAN_LOG_ERR("WSAStartup error!");
        return ELYSIAN_ERR_FATAL;
    }
	
    if ((server_socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		ELYSIAN_LOG_ERR("socket error!");
		WSACleanup();
        return ELYSIAN_ERR_FATAL;
    }
	
    server_sockaddrin.sin_family = AF_INET;
    server_sockaddrin.sin_port = htons(port);
    server_sockaddrin.sin_addr.s_addr = INADDR_ANY;
	//memset(&(server_sockaddrin.sin_zero), 0, sizeof(struct sockaddr_in));

    if (bind(server_socket_fd, (SOCKADDR*)&server_sockaddrin, sizeof(struct sockaddr)) == SOCKET_ERROR) {
        ELYSIAN_LOG_ERR("bind error!");
		WSACleanup();
        return ELYSIAN_ERR_FATAL;
    }

    if (listen(server_socket_fd, ELYSIAN_MAX_CLIENTS_NUM) == SOCKET_ERROR) {
        ELYSIAN_LOG_ERR("listen error!");
		WSACleanup();
        return ELYSIAN_ERR_FATAL;
    }
    
    server_socket->fd = server_socket_fd;
#else

#endif
    return ELYSIAN_ERR_OK;
}

/* 
** @brief 		Block for a specified time period waiting for new connections. If timeout_ms is zero, the call must be non blocking.
**				If this is not true, the web server is not going to behave properly and multi-client support will not be possible.
**
** @param[in]	server The server instance
** @param[in] 	server_socket Server socket descriptor
** @param[in] 	timeout_ms The maximum perod in msec that the call is allowed block
** @param[in] 	timeout_ms The maximum time period that this call could block
** @param[out] 	client_socket The socket descriptor of the accepted client connection.
**
** @retval ELYSIAN_ERR_OK  		New client socket was accepted. client_socket stores the socket descriptor for the new client.
** @retval ELYSIAN_ERR_POLL  	No new connection was accepted. client_socket is invalid.
** @retval ELYSIAN_ERR_FATAL  	There was a fatal error. No new connection was accepted. client_socket is invalid.
 */
elysian_err_t elysian_port_socket_accept(elysian_socket_t* server_socket, uint32_t timeout_ms, elysian_socket_t* client_socket){
    int client_socket_fd;
    struct sockaddr_in client_addr;
    
#if defined(ELYSIAN_TCPIP_ENV_UNIX)
	socklen_t sockaddr_in_size;

    sockaddr_in_size = sizeof(struct sockaddr_in);
    
    elysian_setblocking(server_socket, 0);
    client_socket_fd = accept(server_socket->fd, (struct sockaddr *)&client_addr, &sockaddr_in_size);
    elysian_setblocking(server_socket, 1);
    
    if(client_socket_fd < 0) {
		if((errno != EAGAIN) && (errno != EWOULDBLOCK)){
             return ELYSIAN_ERR_FATAL;
        }else{
			 return ELYSIAN_ERR_POLL;
		}
    } 
	
    ELYSIAN_LOG("New socket %d accepted!", client_socket_fd);
	
    client_socket->fd = client_socket_fd;
    
    return ELYSIAN_ERR_OK;
#elif defined(ELYSIAN_TCPIP_ENV_WINDOWS)
	int sockaddr_in_size;
    sockaddr_in_size = sizeof(struct sockaddr_in);
    
    elysian_setblocking(server_socket, 0);
    client_socket_fd = accept(server_socket->fd, (struct sockaddr *)&client_addr, &sockaddr_in_size);
    elysian_setblocking(server_socket, 1);
    
    if(client_socket_fd == INVALID_SOCKET) {
		int win_errno = WSAGetLastError();
		if (win_errno != WSAEWOULDBLOCK) {
			printf("accept() failed with error %d\n", WSAGetLastError());
			return ELYSIAN_ERR_FATAL;
		} else {
			return ELYSIAN_ERR_POLL;
		}
    }
    
    ELYSIAN_LOG("Socket %d accepted!", client_socket_fd);
    client_socket->fd = client_socket_fd;
    
    return ELYSIAN_ERR_OK;
#else
	return ELYSIAN_ERR_FATAL;
#endif 
}


/* 
** @brief 		Read data through socket. The operation must be non blocking. If this is not true, the web server is not going to 
**				behave properly and multi-client support will not be possible.
**
** @param[in]	server The server instance
** @param[in] 	client_socket The socket descriptor
** @param[out] 	buf Buffer that is going to store the received data
** @param[in] 	buf_size The size of data to be read
**
** @retval >=0	The number of bytes succesfully read. 0 indicates that there were currently no data available to the particular
**				socket. Connection is not considered lost in this case.
** @retval -1	There was a fatal error. Connection is considered lost. elysian_port_socket_close() is going to follow.
 */
int elysian_port_socket_read(elysian_socket_t* client_socket, uint8_t* buf, uint16_t buf_size){
    int result;
    
    //ELYSIAN_LOG("Socket %d Trying to read %u bytes\r\n", client_socket->fd, buf_size);

    elysian_setblocking(client_socket, 0);
	
#if defined(ELYSIAN_TCPIP_ENV_UNIX)
    result = recv(client_socket->fd, buf, buf_size, 0);
    if(result < 0){
        if((errno != EAGAIN) && (errno != EWOULDBLOCK)){
			ELYSIAN_LOG_ERR(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> read() result is %d, ERRNO = %d", result, errno);
            result = -1;
        }else{
			result = 0;
		}
    }else if(result == 0){
        /*
        ** The return value will be 0 when the peer has performed an orderly shutdown
        */
        result = -1;
    }
#elif defined(ELYSIAN_TCPIP_ENV_WINDOWS)
    result = recv(client_socket->fd, (char*) buf, buf_size, 0);
    if(result < 0){
        int win_errno = WSAGetLastError();
        if(win_errno != WSAEWOULDBLOCK ){
			ELYSIAN_LOG_ERR(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> read() result is %d, ERRNO = %d", result, win_errno);
            result = -1;
        }else{
			result = 0;
		}
    }else if(result == 0){
        /*
        ** The return value will be 0 when the peer has performed an orderly shutdown
        */
        result = -1;
    }
#else
	result = -1;
#endif

    elysian_setblocking(client_socket, 1);

    //ELYSIAN_LOG("received = %u bytes\r\n", result);

	return result;
}

/* 
** @brief 		Write data through socket. The operation must be non blocking. If this is not true, the web server is not going to 
**				behave properly and multi-client support will not be possible.
**
** @param[in]	server The server instance
** @param[in] 	client_socket The socket descriptor
** @param[in] 	buf The data to be sent
** @param[in] 	buf_size The size of data to be sent
**
** @retval >=0	The number of bytes succesfully sent. 0 indicates that there were currently no resurces to perfom the operation,
**				and the upper layer should try again later with exponential backoff. Connection is not considered lost in this case.
** @retval -1	There was a fatal error. Connection is considered lost. elysian_port_socket_close() is going to follow.
 */
int elysian_port_socket_write(elysian_socket_t* client_socket, uint8_t* buf, uint16_t buf_size){
    int result;
    
	//ELYSIAN_LOG("Trying to write %u bytes\r\n", buf_size);
	
    elysian_setblocking(client_socket, 0);
	
#if defined(ELYSIAN_TCPIP_ENV_UNIX)
	result = write(client_socket->fd, buf, buf_size);
    if(result < 0){
        if((errno != EAGAIN) && (errno != EWOULDBLOCK)){
            ELYSIAN_LOG_ERR(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> write() result is %d, ERRNO = %d", result, errno);
            result = -1;
        }else{
			result = 0;
		}
    }
#elif defined(ELYSIAN_TCPIP_ENV_WINDOWS)
	result = send(client_socket->fd, (const char *) buf, buf_size, 0);
    if(result == SOCKET_ERROR){
		int win_errno = WSAGetLastError();
        if(win_errno != WSAEWOULDBLOCK){
            ELYSIAN_LOG_ERR(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> send() result is %d, ERRNO = %d", result, win_errno);
            result = -1;
        }else{
			result = 0;
		}
    }
#else
	result = -1;
#endif

    elysian_setblocking(client_socket, 1);

    return result;
}

#if (ELYSIAN_SOCKET_SELECT_SUPPORTED == 1)
/* 
** @brief 		Block for a specified time period waiting for input data (client sockets) or new connections (server socket) from particular sockets. 
**
** @param[in]	server The server instance
** @param[in] 	socket_readset Array of sockets from which read events are expected.
** @param[in] 	socket_readset_sz The size of the socket_readset array
** @param[in] 	timeout_ms The maximum time period that this call could block
** @param[out] 	socket_readset_status Array indicating from which sockets read data(client sockets) or new connections(server socket) are available.
**				Setting socket_readset_status[k] to 1 means that socket descriptor socket_readset[k] has available data(if it is a client socket) or
**				has accepted a new connection (if it is a server socket). Setting it to 0 indicates that there were no events for the particular socket.
**
** @retval ELYSIAN_ERR_OK  		The operation was succesfull. socket_readset_status[] indicates zero or more sockets that are able to read data.
** @retval ELYSIAN_ERR_FATAL  	There was a fatal error. 
 */
elysian_err_t elysian_port_socket_select(elysian_socket_t* socket_readset[], uint32_t socket_readset_sz, uint32_t timeout_ms, uint8_t socket_readset_status[]){
    uint32_t index;
    int retval;
	
#if defined(ELYSIAN_TCPIP_ENV_UNIX)
	struct timeval timeval;
	fd_set fd_readset;
    FD_ZERO(&fd_readset);
    //ELYSIAN_LOG("----");
    for(index = 0; index < socket_readset_sz; index++){
        //ELYSIAN_LOG("elysian_port_socket_select(timeout = %u, socket_readset[%d]->fd = %d, size = %d)", timeout_ms, index, socket_readset[index]->fd, socket_readset_sz);
        socket_readset_status[index] = 0;
        FD_SET(socket_readset[index]->fd, &fd_readset);
    }
    //ELYSIAN_LOG("----");
    
    timeval.tv_sec 		= (timeout_ms / 1000); 
    timeval.tv_usec 	= (timeout_ms % 1000) * 1000;
    
    retval = select (FD_SETSIZE, &fd_readset, NULL, NULL, &timeval);
    
    if(retval < 0){
        /* Select error */
        ELYSIAN_LOG("Select Error!\r\n");
    }else if(retval == 0){
        /* Select timeout */
        ELYSIAN_LOG("Select Timeout!\r\n");
    }else{
        /* Read/Write event */
        //ELYSIAN_LOG("Select Event!\r\n");
        for(index = 0; index < socket_readset_sz; index++){
            
            if (FD_ISSET(socket_readset[index]->fd, &fd_readset)){
                //ELYSIAN_LOG("Socket[index = %d] %d can read!\r\n",index,socket_readset[index]->fd);
                socket_readset_status[index] = 1;
            }
        }
    }
	return ELYSIAN_ERR_OK;
#elif defined(ELYSIAN_TCPIP_ENV_WINDOWS)
	TIMEVAL timeval;
	fd_set fd_readset;
    FD_ZERO(&fd_readset);
    //ELYSIAN_LOG("----");
    for(index = 0; index < socket_readset_sz; index++){
        //ELYSIAN_LOG("elysian_port_socket_select(timeout = %u, socket_readset[%d]->fd = %d, size = %d)", timeout_ms, index, socket_readset[index]->fd, socket_readset_sz);
        socket_readset_status[index] = 0;
        FD_SET(socket_readset[index]->fd, &fd_readset);
    }
    //ELYSIAN_LOG("----");
    
    timeval.tv_sec 		= (timeout_ms / 1000); 
    timeval.tv_usec 	= (timeout_ms % 1000) * 1000;
    
    retval = select (FD_SETSIZE, &fd_readset, NULL, NULL, &timeval);
    
    if(retval == SOCKET_ERROR){
        /* Select error */
		int win_errno = WSAGetLastError();
        ELYSIAN_LOG("Select Error! win_errno = %d\r\n", win_errno);
    }else if(retval == 0){
        /* Select timeout */
        ELYSIAN_LOG("Select Timeout!\r\n");
    }else{
        /* Read/Write event */
        //ELYSIAN_LOG("Select Event!\r\n");
        for(index = 0; index < socket_readset_sz; index++){
            
            if (FD_ISSET(socket_readset[index]->fd, &fd_readset)){
                //ELYSIAN_LOG("Socket[index = %d] %d can read!\r\n",index,socket_readset[index]->fd);
                socket_readset_status[index] = 1;
            }
        }
    }
	return ELYSIAN_ERR_OK;
#else
	return ELYSIAN_ERR_FATAL;
#endif 
}
#endif


/* ---------------------------------------------------------------------------------------------------------------------------------------
** Filesystem
** ------------------------------------------------------------------------------------------------------------------------------------ */

/* 
** @brief Open file
**
** @param[in] 	server The server instance
** @param[in] 	abs_path The absolute file path
** @param[in] 	mode The desired file mode
** @param[out] 	file The file descriptor of the opened file
**
** @retval ELYSIAN_ERR_OK  		The operation was succesfull, file stores the file descriptor of the opene file.
** @retval ELYSIAN_ERR_POLL 	There are currently no resurces to perfom the operation, try again later with exponential backoff
** @retval ELYSIAN_ERR_NOTFOUND There file was not found
** @retval ELYSIAN_ERR_FATAL  	There was a fatal error. 
 */
elysian_err_t elysian_port_fs_ext_fopen(elysian_t* server, char* abs_path, elysian_file_mode_t mode, elysian_file_t* file){
	ELYSIAN_LOG("[[ Opening ext file '%s']]", abs_path);
#if (defined(ELYSIAN_FS_ENV_UNIX) || defined(ELYSIAN_FS_ENV_WINDOWS))
	/*
	** Make path relative to the "elysian" executable by removing leading '/'
	*/
	abs_path = &abs_path[1];
#endif
	
#if defined(ELYSIAN_FS_ENV_UNIX)
    file->descriptor.ext.fd = fopen(abs_path, (mode == ELYSIAN_FILE_MODE_READ) ? "rb" : "wb");
    if(!file->descriptor.ext.fd){
        return ELYSIAN_ERR_NOTFOUND;
    }
    return ELYSIAN_ERR_OK;
#elif defined(ELYSIAN_FS_ENV_WINDOWS)
    file->descriptor.ext.fd = fopen(abs_path, (mode == ELYSIAN_FILE_MODE_READ) ? "rb" : "wb");
    if(!file->descriptor.ext.fd){
        return ELYSIAN_ERR_NOTFOUND;
    }
    return ELYSIAN_ERR_OK;
#elif defined(ELYSIAN_FS_ENV_FATAFS)
    FIL* file;
    FRESULT res;
    if(!(file = elysian_mem_malloc(server, sizeof(FIL))){
		return ELYSIAN_ERR_POLL;
	}
    fr = f_open(file, abs_path, (mode == ELYSIAN_FILE_MODE_READ) ? (FA_READ | FA_OPEN_EXISTING) : (FA_WRITE | FA_CREATE_ALWAYS) ));
    if((res != FR_OK){
        elysian_mem_free(server, file);
        return ELYSIAN_ERR_NOTFOUND;
    }
    *vfile = file;
    return ELYSIAN_ERR_OK;
#else
    return ELYSIAN_ERR_NOTFOUND;
#endif
}

/* 
** @brief Get the size of a particular file
**
** @param[in] 	server The server instance
** @param[in] 	file The file descriptor (retrieved using elysian_port_fs_ext_fopen)
** @param[out] 	filesize The size of the file

** @retval ELYSIAN_ERR_OK  		The operation was succesfull, filesize stores the size of the file.
** @retval ELYSIAN_ERR_FATAL  	There was a fatal error. The upper layer should avoid using the particular file descriptor 
**								for fread/fwrite/fseek/ftell as the operation be always failing. elysian_port_fs_ext_fclose() 
**								is the only file operation that could use the particular file descriptor.
 */
elysian_err_t elysian_port_fs_ext_fsize(elysian_t* server, elysian_file_t* file, uint32_t* filesize){
    *filesize = 0;
#if defined(ELYSIAN_FS_ENV_UNIX)
    elysian_file_ext_t* file_disk = &file->descriptor.ext;
    uint32_t seekpos = ftell(file_disk->fd);
    fseek(file_disk->fd, 0L, SEEK_END);
    *filesize = ftell(file_disk->fd);
    fseek(file_disk->fd, seekpos, SEEK_SET);
    return ELYSIAN_ERR_OK;
#elif defined(ELYSIAN_FS_ENV_WINDOWS)
    elysian_file_ext_t* file_disk = &file->descriptor.ext;
    uint32_t seekpos = ftell(file_disk->fd);
    fseek(file_disk->fd, 0L, SEEK_END);
    *filesize = ftell(file_disk->fd);
    fseek(file_disk->fd, seekpos, SEEK_SET);
	
    return ELYSIAN_ERR_OK;
#elif defined(ELYSIAN_FS_ENV_FATAFS)
	elysian_file_ext_t* file_disk = &file->descriptor.ext;
	*filesize = f_size(file_disk->fd);
    return ELYSIAN_ERR_OK;
#else
    return ELYSIAN_ERR_FATAL;
#endif
}

/* 
** @brief Seek to particular file position.
**
** @param[in] 	server The server instance
** @param[in] 	file The file descriptor (retrieved using elysian_port_fs_ext_fopen)
** @param[in] 	seekpos The desired seek position. Valid range is [0, fsize -1]

** @retval ELYSIAN_ERR_OK  		The operation was succesfull.
** @retval ELYSIAN_ERR_FATAL  	There was a fatal error. The upper layer should avoid using the particular file descriptor 
**								for fread/fwrite/fseek/ftell as the operation be always failing. elysian_port_fs_ext_fclose() 
**								is the only file operation that could use the particular file descriptor.
 */
elysian_err_t elysian_port_fs_ext_fseek(elysian_t* server, elysian_file_t* file, uint32_t seekpos){
#if defined(ELYSIAN_FS_ENV_UNIX)
    elysian_file_ext_t* file_disk = &file->descriptor.ext;;
    fseek(file_disk->fd, seekpos, SEEK_SET);
    return ELYSIAN_ERR_OK;
#elif defined(ELYSIAN_FS_ENV_WINDOWS)
	elysian_file_ext_t* file_disk = &file->descriptor.ext;
    fseek(file_disk->fd, seekpos, SEEK_SET);
    return ELYSIAN_ERR_OK;
#elif defined(ELYSIAN_FS_ENV_FATAFS)
	FRESULT res;
	elysian_file_ext_t* file_disk = &file->descriptor.ext;
	res = f_lseek(file_disk->fd, seekpos);
	if(res != FR_OK){
		return ELYSIAN_ERR_FATAL;
	}
    return ELYSIAN_ERR_OK;
#else
    return ELYSIAN_ERR_FATAL;
#endif
}

/* 
** @brief Get the seek position of an opened file
**
** @param[in] 	server The server instance
** @param[in] 	file The file descriptor (retrieved using elysian_port_fs_ext_fopen)
** @param[out] 	seekpos The current seek position. Valid range is [0, fsize -1]

** @retval ELYSIAN_ERR_OK  		The operation was succesfull, seekpos stores the current seek position.
** @retval ELYSIAN_ERR_FATAL  	There was a fatal error. The upper layer should avoid using the particular file descriptor 
**								for fread/fwrite/fseek/ftell as the operation be always failing. elysian_port_fs_ext_fclose() 
**								is the only file operation that could use the particular file descriptor.
 */
elysian_err_t elysian_port_fs_ext_ftell(elysian_t* server, elysian_file_t* file, uint32_t* seekpos){
#if defined(ELYSIAN_FS_ENV_UNIX)
	elysian_file_ext_t* file_disk = &file->descriptor.ext;
    *seekpos = ftell(file_disk->fd);
    return ELYSIAN_ERR_OK;
#elif defined(ELYSIAN_FS_ENV_WINDOWS)
	elysian_file_ext_t* file_disk = &file->descriptor.ext;
    *seekpos = ftell(file_disk->fd);
    return ELYSIAN_ERR_OK;
#elif defined(ELYSIAN_FS_ENV_FATAFS)
	elysian_file_ext_t* file_disk = &file->descriptor.ext;
	*filesize = f_tell(file_disk->fd);
    return ELYSIAN_ERR_OK;
#else
    return ELYSIAN_ERR_FATAL;
#endif
}

/* 
** @brief Read data from an opened file
**
** @param[in] server The server instance
** @param[in] file The file descriptor (retrieved using elysian_port_fs_ext_fopen)
** @param[out] buf The buffer that is going to store the retrieved data
** @param[in] buf_size The size of buf

** @retval >=0  The number of bytes succesfully read. If the value is 0, EOF is indicated.
** @retval -1 	There was a fatal error. The upper layer should avoid using the particular file descriptor 
**				for fread/fwrite/fseek/ftell as the operation be always failing. elysian_port_fs_ext_fclose() 
**				is the only file operation that could use the particular file descriptor.
 */
int elysian_port_fs_ext_fread(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size){
#if defined(ELYSIAN_FS_ENV_UNIX)
	int result;
	elysian_file_ext_t* file_disk = &file->descriptor.ext;
    result = fread(buf, 1, buf_size, file_disk->fd);
    return result;
#elif defined(ELYSIAN_FS_ENV_WINDOWS)
	int result;
	elysian_file_ext_t* file_disk = &file->descriptor.ext;
    result = fread(buf, 1, buf_size, file_disk->fd);
    return result;
#elif defined(ELYSIAN_FS_ENV_FATAFS)
   	FRESULT res;
	elysian_file_ext_t* file_disk = &file->descriptor.ext;
	res = f_read(file_disk->fd, readbuf, readbufsize, actualreadsize);
	if(res != FR_OK){
		*actualreadsize = 0;
		return ELYSIAN_ERR_FATAL;
	}
    return ELYSIAN_ERR_OK;
#else
    return ELYSIAN_ERR_FATAL;
#endif
}

/* 
** @brief Write data to an opened file
**
** @param[in] server The server instance
** @param[in] file The file descriptor (retrieved using elysian_port_fs_ext_fopen)
** @param[in] buf The buffer containing the data to be written
** @param[in] buf_size The size of buf

** @retval >=0 	The number of bytes succesfully written. 0 indicates that there were currently no resurces to perfom the operation,
**				and the upper layer should try again later with exponential backoff.
** @retval -1 	There was a fatal error. The upper layer should avoid using the particular file descriptor 
**				for fread/fwrite/fseek/ftell as the operation be always failing. elysian_port_fs_ext_fclose() 
**				is the only file operation that could use the particular file descriptor.
 */
int elysian_port_fs_ext_fwrite(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size){
#if defined(ELYSIAN_FS_ENV_UNIX)
	int result;
	elysian_file_ext_t* file_disk = &file->descriptor.ext;
	
	result = fwrite(buf, 1, buf_size, file_disk->fd);
	if(result == buf_size){
		return result;
	}else{
		return -1;
	}
#elif defined(ELYSIAN_FS_ENV_WINDOWS)
	int result;
	elysian_file_ext_t* file_disk = &file->descriptor.ext;
	
	result = fwrite(buf, 1, buf_size, file_disk->fd);
	if(result == buf_size){
		return result;
	}else{
		return -1;
	}
#elif defined(ELYSIAN_FS_ENV_FATAFS)
   	FRESULT res;
	int actual_write_sz;
	elysian_file_ext_t* file_disk = &file->descriptor.ext;
	res = f_write(file_disk->fd, write_buf, write_buf_sz, &actual_write_sz);
	if(res != FR_OK){
		return -1;
	}else{
		return actual_write_sz;
	}
#else
    return -1;
#endif
}

/* 
** @brief Close an opened file
**
** @param[in] server The server instance
** @param[in] file The file descriptor (retrieved using elysian_port_fs_ext_fopen)
**
** @retval ELYSIAN_ERR_OK 		The file was found and succesfully closed
** @retval ELYSIAN_ERR_FATAL 	The file could not be closed (either because it was not found or because EXT is not mounted)
 */
elysian_err_t elysian_port_fs_ext_fclose(elysian_t* server, elysian_file_t* file){
#if defined(ELYSIAN_FS_ENV_UNIX)
	elysian_file_ext_t* file_disk = &file->descriptor.ext;
    fclose(file_disk->fd);
    return ELYSIAN_ERR_OK;
#elif defined(ELYSIAN_FS_ENV_WINDOWS)
	elysian_file_ext_t* file_disk = &file->descriptor.ext;
    fclose(file_disk->fd);
    return ELYSIAN_ERR_OK;
#elif defined(ELYSIAN_FS_ENV_FATAFS)
   	FRESULT res;
	elysian_file_ext_t* file_disk = &file->descriptor.ext;
	res = f_close(file_disk->fd);
	if(res != FR_OK){
		return ELYSIAN_ERR_FATAL;
	}
    return ELYSIAN_ERR_OK;
#else
    return ELYSIAN_ERR_OK;
#endif
}

/* 
** @brief Remove a file from the storage device.
**
** @param[in] server The server instance
** @param[in] abs_path The absolute file path
**
** @retval ELYSIAN_ERR_OK 		The file was found and succesfully removed
** @retval ELYSIAN_ERR_FATAL 	The file could not be removed (either because it was not found or because EXT is not mounted)
 */
elysian_err_t elysian_port_fs_ext_fremove(elysian_t* server, char* abs_path){
	ELYSIAN_LOG("[[ Removing ext file '%s']]", abs_path);
#if (defined(ELYSIAN_FS_ENV_UNIX) || defined(ELYSIAN_FS_ENV_WINDOWS))
	/*
	** Make path relative to the "elysian" executable by removing leading '/'
	*/
	abs_path = &abs_path[1];
#endif
	
#if defined(ELYSIAN_FS_ENV_UNIX)
    remove(abs_path);
    return ELYSIAN_ERR_OK;
#elif defined(ELYSIAN_FS_ENV_WINDOWS)
    remove(abs_path);
    return ELYSIAN_ERR_OK;
#elif defined(ELYSIAN_FS_ENV_FATAFS)
	FRESULT res;
    res = f_unlink(abs_path);
	if(res != FR_OK){
		return ELYSIAN_ERR_FATAL;
	}
    return ELYSIAN_ERR_OK;
#else
	return ELYSIAN_ERR_OK;
#endif
}
