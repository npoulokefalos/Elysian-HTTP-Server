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

/*
****************************************************************************************************************************
** Time
****************************************************************************************************************************
*/
uint32_t elysian_port_time_now();
void elysian_port_time_sleep(uint32_t ms);

/*
****************************************************************************************************************************
** Threads
****************************************************************************************************************************
*/
void elysian_port_thread_yield(void);

/*
****************************************************************************************************************************
** Memory
****************************************************************************************************************************
*/
void* elysian_port_mem_malloc(uint32_t size);
void elysian_port_mem_free(void* ptr);

/*
****************************************************************************************************************************
** Hostname
****************************************************************************************************************************
*/
elysian_err_t elysian_port_hostname_get(char hostname[64]);

/*
****************************************************************************************************************************
** Sockets
****************************************************************************************************************************
*/
void elysian_port_socket_close(elysian_socket_t* socket);
elysian_err_t elysian_port_socket_listen(uint16_t port, elysian_socket_t* server_socket);
elysian_err_t elysian_port_socket_accept(elysian_socket_t* server_socket, uint32_t timeout_ms, elysian_socket_t* client_socket);
int elysian_port_socket_read(elysian_socket_t* client_socket, uint8_t* buf, uint16_t buf_size);
int elysian_port_socket_write(elysian_socket_t* client_socket, uint8_t* buf, uint16_t buf_size);
elysian_err_t elysian_port_socket_select(elysian_socket_t* socket_readset[], uint32_t socket_readset_sz, uint32_t timeout_ms, uint8_t socket_readset_status[]);

/*
****************************************************************************************************************************
** Filesystem
****************************************************************************************************************************
*/

elysian_err_t elysian_port_fs_disk_fopen(elysian_t* server, char* abs_path, elysian_file_mode_t mode, elysian_file_t* file);


elysian_err_t elysian_port_fs_disk_fsize(elysian_t* server, elysian_file_t* file, uint32_t* filesize);


elysian_err_t elysian_port_fs_disk_fseek(elysian_t* server, elysian_file_t* file, uint32_t seekpos);


elysian_err_t elysian_port_fs_disk_ftell(elysian_t* server, elysian_file_t* file, uint32_t* seekpos);


int elysian_port_fs_disk_fread(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size);

int elysian_port_fs_disk_fwrite(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size);

elysian_err_t elysian_port_fs_disk_fclose(elysian_t* server, elysian_file_t* file);

elysian_err_t elysian_port_fs_disk_fremove(elysian_t* server, char* abs_path);
