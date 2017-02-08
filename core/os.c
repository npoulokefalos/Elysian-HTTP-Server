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
#include "elysian_port.h"

/*
****************************************************************************************************************************
** Time
****************************************************************************************************************************
*/

uint32_t elysian_time_now() {
    return  elysian_port_time_now();
}

void elysian_time_sleep(uint32_t ms) {
    elysian_port_time_sleep(ms);
}

uint32_t elysian_time_elapsed(uint32_t tic_ms) {
	uint32_t toc_ms;
	uint32_t elapsed_ms;
	
	toc_ms = elysian_time_now();
	elapsed_ms = (toc_ms >= tic_ms) ? toc_ms - tic_ms : toc_ms;
	
	return elapsed_ms;
}

void elysian_thread_yield(){
	elysian_port_thread_yield();
}

/*
****************************************************************************************************************************
** Memory
****************************************************************************************************************************
*/

#define elysian_mem_OFFSETOF(type, field)    (uint32_t) ((unsigned long) &(((type *) 0)->field))

typedef struct elysian_mem_block_t elysian_mem_block_t;
struct elysian_mem_block_t{
    uint32_t size;
    uint8_t data[0];
};

uint32_t elysian_mem_usage_bytes = 0;

uint32_t elysian_mem_usage(){
    ELYSIAN_LOG("elysian_mem_usage() =============================== %u", elysian_mem_usage_bytes);
    return elysian_mem_usage_bytes;
}

void* elysian_mem_malloc(elysian_t* server, uint32_t size){
	//elysian_schdlr_task_t* task = elysian_schdlr_current_task_get(server);
    void* ptr;

#if 0
	if (rand() % 6 == 0) {
		return NULL;
	}
#endif
	
	if (elysian_mem_usage_bytes + size > (ELYSIAN_MAX_MEMORY_USAGE_KB * 1024)) {
		  return NULL;
	}

	
    ptr = elysian_port_mem_malloc(sizeof(elysian_mem_block_t) + size);
    if(!ptr){
        return NULL;
    }else{
        elysian_mem_block_t* mem_block = (elysian_mem_block_t*) ptr;
        mem_block->size = size;
        elysian_mem_usage_bytes += size;
		//if (task) {
		//	task->allocated_memory += size;
		//	ELYSIAN_LOG("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++ [client %u] alloc %u = %u", task->client->id, size, task->allocated_memory);
		//}
		elysian_mem_usage();
        return mem_block->data;
    }
}


void elysian_mem_free(elysian_t* server, void* vptr){
	//elysian_schdlr_task_t* task = elysian_schdlr_current_task_get(server);
    uint8_t* ptr = vptr;
    ptr -= elysian_mem_OFFSETOF(elysian_mem_block_t,data);
    elysian_mem_block_t* mem_block = (elysian_mem_block_t*) ptr;
    elysian_mem_usage_bytes -= (uint32_t) mem_block->size;
	//if (task) {
	//	task->allocated_memory -= (uint32_t) mem_block->size;
	//	ELYSIAN_LOG("------------------------------------------------------------- [client %u] free %u = %u", task->client->id, mem_block->size, task->allocated_memory);
	//}
    elysian_port_mem_free(mem_block);
	elysian_mem_usage();
}


elysian_err_t elysian_os_hostname_get(char hostname[64]){
    return elysian_port_hostname_get(hostname);
}


/*
****************************************************************************************************************************
** Sockets
****************************************************************************************************************************
*/

void elysian_socket_close(elysian_socket_t* socket){
    ELYSIAN_ASSERT(socket->actively_closed == 0);
    if(socket->actively_closed){return;}
    elysian_port_socket_close(socket);
    socket->actively_closed = 1;
}

elysian_err_t elysian_socket_listen(uint16_t port, elysian_socket_t* server_socket){
    elysian_err_t err;
    err = elysian_port_socket_listen(port, server_socket);
    return err;
}

elysian_err_t elysian_socket_accept(elysian_socket_t* server_socket, uint32_t timeout_ms, elysian_socket_t* client_socket){
    elysian_err_t err;
    
    ELYSIAN_ASSERT(server_socket->actively_closed == 0);
    
    if((server_socket->passively_closed) || (server_socket->actively_closed)){
        return ELYSIAN_ERR_FATAL;
    }
    
    err = elysian_port_socket_accept(server_socket, timeout_ms, client_socket);
    
    client_socket->passively_closed = 0;
    client_socket->actively_closed = 0;
    
    return err;
}

elysian_err_t elysian_socket_read(elysian_socket_t* client_socket, uint8_t* data, uint16_t datalen, uint32_t* received){
    int result;
    
    ELYSIAN_ASSERT(client_socket->actively_closed == 0);
    
    *received = 0;
    if((client_socket->passively_closed) || (client_socket->actively_closed)){
        return ELYSIAN_ERR_FATAL;
    }
    
    result = elysian_port_socket_read(client_socket, data, datalen);
	if(result > 0){
		*received = result;
		return ELYSIAN_ERR_OK;
	}else if(result == 0){
		*received = 0;
		return ELYSIAN_ERR_POLL;
	}else{
		*received = 0;
		return ELYSIAN_ERR_FATAL;
	}
}

elysian_err_t elysian_socket_write(elysian_socket_t* client_socket, uint8_t* data, uint16_t datalen, uint32_t* sent){
    int result;
    
    ELYSIAN_ASSERT(client_socket->actively_closed == 0);
    
    *sent = 0;
    if((client_socket->passively_closed) || (client_socket->actively_closed)){
        return ELYSIAN_ERR_FATAL;
    }
    
    result = elysian_port_socket_write(client_socket, data, datalen);
	if(result > 0){
		*sent = result;
		return ELYSIAN_ERR_OK;
	}else if(result == 0){
		*sent = 0;
		return ELYSIAN_ERR_POLL;
	}else{
		*sent = 0;
		return ELYSIAN_ERR_FATAL;
	}
}

#if (ELYSIAN_SOCKET_SELECT_SUPPORTED == 1)
elysian_err_t elysian_socket_select(elysian_socket_t* socket_readset[], uint32_t socket_readset_sz, uint32_t timeout_ms, uint8_t socket_readset_status[]){
    elysian_err_t err;
    err = elysian_port_socket_select(socket_readset, socket_readset_sz, timeout_ms, socket_readset_status);
    return err;
}
#endif

/*
****************************************************************************************************************************
** Filesystem
****************************************************************************************************************************
*/

const elysian_fs_memdev_t fs_memdevs[] = {
	{
		.vrt_root = ELYSIAN_FS_RAM_VRT_ROOT,
		.abs_root = ELYSIAN_FS_RAM_ABS_ROOT,
		.fopen = elysian_fs_ram_fopen, 
		.fsize = elysian_fs_ram_fsize,
		.fseek = elysian_fs_ram_fseek,
        .ftell = elysian_fs_ram_ftell,
		.fread = elysian_fs_ram_fread,
		.fwrite = elysian_fs_ram_fwrite,
		.fclose = elysian_fs_ram_fclose,
		.fremove = elysian_fs_ram_fremove
	},
	{
		.vrt_root = ELYSIAN_FS_ROM_VRT_ROOT,
		.abs_root = ELYSIAN_FS_ROM_ABS_ROOT,
		.fopen = elysian_fs_rom_fopen, 
		.fsize = elysian_fs_rom_fsize,
		.fseek = elysian_fs_rom_fseek,
        .ftell = elysian_fs_rom_ftell,
		.fread = elysian_fs_rom_fread,
		.fwrite = elysian_fs_rom_fwrite,
		.fclose = elysian_fs_rom_fclose,
		.fremove = elysian_fs_rom_fremove
	},
	{
		.vrt_root = ELYSIAN_FS_EXT_VRT_ROOT,
		.abs_root = ELYSIAN_FS_EXT_ABS_ROOT,
		.fopen = elysian_port_fs_ext_fopen, 
		.fsize = elysian_port_fs_ext_fsize,
		.fseek = elysian_port_fs_ext_fseek,
        .ftell = elysian_port_fs_ext_ftell,
		.fread = elysian_port_fs_ext_fread,
		.fwrite = elysian_port_fs_ext_fwrite,
		.fclose = elysian_port_fs_ext_fclose,
		.fremove = elysian_port_fs_ext_fremove
	},
	{
		.vrt_root = ELYSIAN_FS_VRT_VRT_ROOT,
		.abs_root = ELYSIAN_FS_VRT_ABS_ROOT,
		.fopen = elysian_fs_vrt_fopen, 
		.fsize = elysian_fs_vrt_fsize,
		.fseek = elysian_fs_vrt_fseek,
        .ftell = elysian_fs_vrt_ftell,
		.fread = elysian_fs_vrt_fread,
		.fwrite = elysian_fs_vrt_fwrite,
		.fclose = elysian_fs_vrt_fclose,
		.fremove = elysian_fs_vrt_fremove
	}
};

elysian_fs_memdev_t* elysian_fs_get_memdev(char* vrt_path){
	uint16_t vrt_root_len;
	uint8_t vrt_root_offset;
	uint8_t k;

	if (vrt_path[0] != '/') {
		vrt_root_offset = 1;
	} else {
		vrt_root_offset = 0;
	}
	
	for (k = 0; k < sizeof(fs_memdevs) / sizeof(elysian_fs_memdev_t); k++) {
		vrt_root_len = strlen(fs_memdevs[k].vrt_root) - vrt_root_offset;
		ELYSIAN_LOG("Comparing virtual root path of file '%s' with '%s'..", vrt_path, &fs_memdevs[k].vrt_root[vrt_root_offset]);
		if((strlen(vrt_path) > vrt_root_len) && (memcmp(vrt_path, &fs_memdevs[k].vrt_root[vrt_root_offset], vrt_root_len) == 0) && (vrt_path[vrt_root_len] == '/')){
            ELYSIAN_LOG("Matches!");
			return (elysian_fs_memdev_t*) &fs_memdevs[k];
		}
	}

	return NULL;
}

void elysian_fs_finit(elysian_t* server, elysian_file_t* file){
    file->mode = ELYSIAN_FILE_MODE_NA;
	file->status = ELYSIAN_FILE_STATUS_CLOSED;
    file->memdev = NULL;
}

elysian_err_t elysian_fs_fopen(elysian_t* server, char* vrt_path, elysian_file_mode_t mode, elysian_file_t* file) {
	char abs_path[ELYSIAN_FS_MAX_PATH_LEN];
	elysian_fs_memdev_t* memdev;
	elysian_err_t err;

	ELYSIAN_LOG("Opening file with virtual path '%s'..", vrt_path);
	
	ELYSIAN_ASSERT(mode == ELYSIAN_FILE_MODE_READ || mode == ELYSIAN_FILE_MODE_WRITE);
	
	memdev = elysian_fs_get_memdev(vrt_path);
	if (!memdev){
		ELYSIAN_LOG("File does not belong to any memdev (invalid root path?)");
		return ELYSIAN_ERR_NOTFOUND;
	} else {
		ELYSIAN_LOG("File belongs to '%s' memdev", memdev->vrt_root);
	}
	
	/*
	** Don't create a file that we will not be able to remove
	*/
	if(strlen(memdev->abs_root) + strlen(&vrt_path[strlen(memdev->vrt_root)]) + 1 <= ELYSIAN_FS_MAX_PATH_LEN) {
		elysian_sprintf(abs_path, "%s%s", memdev->abs_root, &vrt_path[strlen(memdev->vrt_root)]);
	} else {
		ELYSIAN_LOG("Filepath too long");
		return ELYSIAN_ERR_FATAL;
	}
	
	ELYSIAN_LOG("Opening file with absolute path '%s'..", vrt_path);
	
	err = memdev->fopen(server, abs_path, mode, file);
	
	if(err == ELYSIAN_ERR_OK){
		file->memdev = memdev;
		file->mode = mode;
		file->status = ELYSIAN_FILE_STATUS_OPENED;
		return ELYSIAN_ERR_OK;
	}else if(err == ELYSIAN_ERR_NOTFOUND){
		/* Continue */
		ELYSIAN_LOG("File not found!");
	}else if(err == ELYSIAN_ERR_POLL){
		/* Continue */
	}else{
		err = ELYSIAN_ERR_FATAL;
	}
	
	file->mode = ELYSIAN_FILE_MODE_NA;
	file->status = ELYSIAN_FILE_STATUS_CLOSED;
	return err;
}

uint8_t elysian_fs_fisopened(elysian_t* server, elysian_file_t* file){
	return (file->status == ELYSIAN_FILE_STATUS_OPENED);
}

elysian_err_t elysian_fs_fsize(elysian_t* server, elysian_file_t* file, uint32_t* filesize){
    elysian_err_t err;
	
	ELYSIAN_ASSERT(file->status == ELYSIAN_FILE_STATUS_OPENED);
	ELYSIAN_ASSERT(file->mode == ELYSIAN_FILE_MODE_READ);
	ELYSIAN_ASSERT(file->memdev);
	
	err = file->memdev->fsize(server, file, filesize);
	
    return err;
}

elysian_err_t elysian_fs_fseek(elysian_t* server, elysian_file_t* file, uint32_t seekpos){
    elysian_err_t err;
	
	ELYSIAN_ASSERT(file->status == ELYSIAN_FILE_STATUS_OPENED);
	ELYSIAN_ASSERT(file->mode == ELYSIAN_FILE_MODE_READ);
	ELYSIAN_ASSERT(file->memdev);
	
	err = file->memdev->fseek(server, file, seekpos);

    return err;
}

elysian_err_t elysian_fs_ftell(elysian_t* server, elysian_file_t* file, uint32_t* seekpos){
    elysian_err_t err;
	
	ELYSIAN_ASSERT(file->status == ELYSIAN_FILE_STATUS_OPENED);
	ELYSIAN_ASSERT(file->mode == ELYSIAN_FILE_MODE_READ);
	ELYSIAN_ASSERT(file->memdev);
	
	err = file->memdev->ftell(server, file, seekpos);

    return err;
}

elysian_err_t elysian_fs_fread(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size, uint32_t* actualreadsize){
    int result;
    uint32_t buf_index = 0;
	
	ELYSIAN_ASSERT(file->status == ELYSIAN_FILE_STATUS_OPENED);
	ELYSIAN_ASSERT(file->mode == ELYSIAN_FILE_MODE_READ);
	ELYSIAN_ASSERT(file->memdev);
	
	/*
	** We could just 'buf_size' bytes and get EOF when actualreadsize < buf_size.
	** This would require from the underlying memory devices to read exactly 'buf_size'
	** bytes to not indicate EOF. This could be a bit tricky to handle in the application layer
	** when HDL memory device is used. Therefore, we loop until get actualreadsize == 0, and
	** is the only indication that EOF has been reached. HDL memory device can return < buf_size
	** bytes without indicating EOF.
	*/
	*actualreadsize = 0;
	
	while (buf_size - buf_index) {
		result = file->memdev->fread(server, file, &buf[buf_index], buf_size - buf_index);
		if (result >= 0){
			ELYSIAN_ASSERT(result <= buf_size - buf_index);
			buf_index += result;
			if (result == 0) {
				break;
			}
		}else{
			*actualreadsize = 0;
			return ELYSIAN_ERR_FATAL;
		}
	}
	
	*actualreadsize = buf_index;
	return ELYSIAN_ERR_OK;
}

/*

*/
elysian_err_t elysian_fs_fwrite(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size, uint32_t* actual_write_sz){
    int result;
    
	ELYSIAN_ASSERT(file->status == ELYSIAN_FILE_STATUS_OPENED);
	ELYSIAN_ASSERT(file->mode == ELYSIAN_FILE_MODE_WRITE);
	ELYSIAN_ASSERT(file->memdev);
	
	result = file->memdev->fwrite(server, file, buf, buf_size);
	if(result > 0){
		*actual_write_sz = result;
		return ELYSIAN_ERR_OK;
	}else if(result == 0){
		*actual_write_sz = 0;
		return ELYSIAN_ERR_POLL;
	}else{
		*actual_write_sz = 0;
		return ELYSIAN_ERR_FATAL;
	}
}


elysian_err_t elysian_fs_fclose(elysian_t* server, elysian_file_t* file){
    elysian_err_t err;
    
	ELYSIAN_ASSERT(file->status == ELYSIAN_FILE_STATUS_OPENED);
	ELYSIAN_ASSERT(file->memdev);
	
	err = file->memdev->fclose(server, file);
    
    file->mode = ELYSIAN_FILE_MODE_NA;
	file->status = ELYSIAN_FILE_STATUS_CLOSED;
    file->memdev = NULL;
    
    if(err != ELYSIAN_ERR_OK){
		return ELYSIAN_ERR_FATAL;
	}else{
		return ELYSIAN_ERR_OK;
	}
}

/*
** Never return ELYSIAN_ERR_POLL here, since it could cause memory leaks on user layer.
** Memory allocation is not allowed here.
*/
elysian_err_t elysian_fs_fremove(elysian_t* server, char* vrt_path) {
	char abs_path[ELYSIAN_FS_MAX_PATH_LEN];
	elysian_fs_memdev_t* memdev;
    elysian_err_t err;

	memdev = elysian_fs_get_memdev(vrt_path);
	if(!memdev){
		return ELYSIAN_ERR_FATAL;
	}
    
	if (strlen(memdev->abs_root) + strlen(&vrt_path[strlen(memdev->vrt_root)]) + 1 <= ELYSIAN_FS_MAX_PATH_LEN) {
		elysian_sprintf(abs_path, "%s%s", memdev->abs_root, &vrt_path[strlen(memdev->vrt_root)]);
	} else {
		return ELYSIAN_ERR_FATAL;
	}
	
	err = memdev->fremove(server, abs_path);
	
	ELYSIAN_ASSERT(err != ELYSIAN_ERR_POLL);
	
    return err;
}

