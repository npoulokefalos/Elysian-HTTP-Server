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

/* -----------------------------------------------------------------------------------------------------------
 RAM filesystem
----------------------------------------------------------------------------------------------------------- */

elysian_fs_ram_file_t fs_ram_files ={
    .name = NULL,
	.cbuf = NULL,
	.read_handles = 0,
	.write_handles = 0,
    .next = &fs_ram_files
};

elysian_err_t elysian_fs_ram_fopen(elysian_t* server, char* abs_path, elysian_file_mode_t mode, elysian_file_t* file){
	elysian_err_t err;
    elysian_fs_ram_file_t* fs_ram_file;

	ELYSIAN_LOG("Opening RAM file with name %s, mode is %u", abs_path, mode);
    
	/*
	* Findout if file exists
	*/
	fs_ram_file = fs_ram_files.next;
	while(fs_ram_file != &fs_ram_files){
		if(strcmp(abs_path, fs_ram_file->name) == 0){
			break;
		}
		fs_ram_file = fs_ram_file->next;
	};
	
	if(mode == ELYSIAN_FILE_MODE_READ){
		
		if(fs_ram_file == &fs_ram_files){
			ELYSIAN_LOG("Not found!\r\n");
			return ELYSIAN_ERR_NOTFOUND;
		}
		
		if(fs_ram_file->write_handles){
			/* File is opened for write, cannot read */
			return ELYSIAN_ERR_FATAL;
		}
		
		fs_ram_file->read_handles++;
		
		file->descriptor.ram.fd = fs_ram_file;
		file->descriptor.ram.pos = 0;
		file->mode = ELYSIAN_FILE_MODE_READ;
        
		return ELYSIAN_ERR_OK;
	}
	
	if(mode == ELYSIAN_FILE_MODE_WRITE){
		/*
		* If file exists, remove it first
		*/
		if(fs_ram_file != &fs_ram_files){
            ELYSIAN_LOG("Ram file exists, removing it");
			if(fs_ram_file->read_handles == 0 && fs_ram_file->write_handles == 0){
				err = elysian_fs_ram_fremove(server, abs_path);
				if(err != ELYSIAN_ERR_OK){
					return err;
				}	
			}else{
				/* File is opened for read/write, cannot remove */
                ELYSIAN_LOG("File is already opened..");
				return ELYSIAN_ERR_FATAL;
			}
		}
		
        ELYSIAN_LOG("allocating new ram file..");
		fs_ram_file = elysian_mem_malloc(server, sizeof(elysian_fs_ram_file_t));
		if(!fs_ram_file){
			return ELYSIAN_ERR_POLL;
		}
		
		fs_ram_file->name = elysian_mem_malloc(server, strlen(abs_path) + 1);
		if(!fs_ram_file->name){
			elysian_mem_free(server, fs_ram_file);
			return ELYSIAN_ERR_POLL;
		}
		strcpy(fs_ram_file->name, abs_path);
		fs_ram_file->cbuf = NULL;
		fs_ram_file->write_handles = 1;
		fs_ram_file->read_handles = 0;
		
		file->descriptor.ram.fd = fs_ram_file;
		file->descriptor.ram.pos = 0;
		file->mode = ELYSIAN_FILE_MODE_WRITE;
		
		/*
		** Add the file to the list
		*/
		fs_ram_file->next = fs_ram_files.next;
		fs_ram_files.next = fs_ram_file;

        ELYSIAN_LOG("Ram file created..");
        
		return ELYSIAN_ERR_OK;
	}

	return ELYSIAN_ERR_FATAL;
}

elysian_err_t elysian_fs_ram_fsize(elysian_t* server, elysian_file_t* file, uint32_t* filesize){
	elysian_cbuf_t* cbuf_next;
    elysian_fs_ram_file_t* fs_ram_file;
    elysian_file_ram_t* file_ram;
    
    file_ram = &file->descriptor.ram;
    fs_ram_file = file_ram->fd;
    
	*filesize = 0;
	cbuf_next =	fs_ram_file->cbuf;
	while(cbuf_next){
		*filesize = (*filesize) + (cbuf_next->len);
        cbuf_next = cbuf_next->next;
	}
    return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_fs_ram_fseek(elysian_t* server, elysian_file_t* file, uint32_t seekpos){
    elysian_err_t err;
	uint32_t filesize;
    elysian_file_ram_t* file_ram;

    file_ram = &file->descriptor.ram;
	
	err = elysian_fs_ram_fsize(server, file, &filesize);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	
	ELYSIAN_ASSERT(seekpos < filesize);
	
	if(seekpos > filesize){
		return ELYSIAN_ERR_FATAL;
	}
	
	file_ram->pos = seekpos;

    return err;
}

elysian_err_t elysian_fs_ram_ftell(elysian_t* server,  elysian_file_t* file, uint32_t* seekpos){
    elysian_file_ram_t* file_ram = &file->descriptor.ram;
	*seekpos = file_ram->pos;
	return ELYSIAN_ERR_OK;
}


int elysian_fs_ram_fread(elysian_t* server,  elysian_file_t* file, uint8_t* buf, uint32_t buf_size){
    elysian_fs_ram_file_t* fs_ram_file;
    elysian_file_ram_t* file_ram;
    uint32_t cpy_sz;
    uint32_t read_sz;
    uint32_t pos;
    elysian_cbuf_t* cbuf;
    
    file_ram = &file->descriptor.ram;
    fs_ram_file = file_ram->fd;
    
    cbuf = fs_ram_file->cbuf;
    pos = file_ram->pos;
    while(cbuf){
        if(pos < cbuf->len){break;}
        pos -= cbuf->len;
        cbuf = cbuf->next;
    }
    read_sz = 0;
    while(cbuf){
        if(read_sz == buf_size){break;}
        cpy_sz = (cbuf->len - pos > buf_size - read_sz) ? buf_size - read_sz : cbuf->len - pos;
        memcpy(&buf[read_sz], &cbuf->data[pos], cpy_sz);
        read_sz += cpy_sz;
        pos += cpy_sz;
        if(pos == cbuf->len){
            cbuf = cbuf->next;
            pos = 0;
        }
    }
    file_ram->pos += read_sz;
    return read_sz;
}


int elysian_fs_ram_fwrite(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size){
    elysian_fs_ram_file_t* fs_ram_file;
    elysian_file_ram_t* file_ram;
	elysian_cbuf_t* write_cbuf;
	
    file_ram = &file->descriptor.ram;
    fs_ram_file = file_ram->fd;
    
	write_cbuf = elysian_cbuf_alloc(server, buf, buf_size);
	if(!write_cbuf){
		return 0;
	}
	
    if(fs_ram_file->cbuf == NULL) {
        fs_ram_file->cbuf = write_cbuf;
    }else{
        elysian_cbuf_t* cbuf;
        cbuf = fs_ram_file->cbuf;
        while(cbuf->next){
            cbuf = cbuf->next;
        }
        cbuf->next = write_cbuf;
    }
    return buf_size;
}

elysian_err_t elysian_fs_ram_fclose(elysian_t* server, elysian_file_t* file){
    elysian_fs_ram_file_t* fs_ram_file;
    elysian_file_ram_t* file_ram;
	
    file_ram = &file->descriptor.ram;
    fs_ram_file = file_ram->fd;
    
	if(file->mode == ELYSIAN_FILE_MODE_READ){
		ELYSIAN_ASSERT(fs_ram_file->read_handles > 0);
		fs_ram_file->read_handles--;
        return ELYSIAN_ERR_OK;
	}
	
	if(file->mode == ELYSIAN_FILE_MODE_WRITE){
		ELYSIAN_ASSERT(fs_ram_file->write_handles > 0);
		fs_ram_file->write_handles--;
        return ELYSIAN_ERR_OK;
	}
    
    return ELYSIAN_ERR_FATAL;
}

elysian_err_t elysian_fs_ram_fremove(elysian_t* server, char* abs_path){
    elysian_fs_ram_file_t* fs_ram_file;
    elysian_fs_ram_file_t* fs_ram_file_prev;
	
    ELYSIAN_LOG("Removing ram file..%s",abs_path);
    
	/*
	** Locate the file
	*/
	fs_ram_file_prev = &fs_ram_files;
	fs_ram_file = fs_ram_files.next;
	while(fs_ram_file != &fs_ram_files){
		if(strcmp(abs_path, fs_ram_file->name) == 0){
			fs_ram_file_prev->next = fs_ram_file->next;
			break;
		}
		fs_ram_file_prev = fs_ram_file;
		fs_ram_file = fs_ram_file->next;
	};
	
	
	if(fs_ram_file == &fs_ram_files){
		/*
		** File not found
		*/
		ELYSIAN_ASSERT(0);
		return ELYSIAN_ERR_FATAL;
	}
	
	if(fs_ram_file->read_handles > 0 || fs_ram_file->write_handles > 0){
		return ELYSIAN_ERR_FATAL;
	}
	
    ELYSIAN_LOG("Removing cbufs of ram file..%s",abs_path);
    
	elysian_mem_free(server, fs_ram_file->name);
	elysian_cbuf_list_free(server, fs_ram_file->cbuf);
	elysian_mem_free(server, fs_ram_file);
    return ELYSIAN_ERR_OK;
}

/* -----------------------------------------------------------------------------------------------------------
 ROM filesystem
----------------------------------------------------------------------------------------------------------- */
static const elysian_file_rom_def_t file_rom_def_empty_file = {
	.name = (char*) ELYSIAN_FS_EMPTY_FILE_ABS_PATH, 
	.ptr = (uint8_t*) "", 
	.size = 0
};

static const char elysian_default_http_status_page[] =
"<!DOCTYPE html>"
"<html>"
"<body style=\"font-family:Arial;background-color:white;color=white;\">"
"<div style=\"margin-left:auto;margin-right:auto;text-align:center;\">"
"<h1><%=http_status_code_description%></h1>"
"<h3>HTTP status <%=http_status_code_num%></h3>"
"An unexpected condition was encountered.<br/><br/>"
"<a style=\"text-decoration:none;\" href=\"../fs_rom/index.html\">Home Page</a>"
"</div>"
"</body>"
"</html>";
										
static const elysian_file_rom_def_t file_rom_def_http_status_page = {
	.name = (char*) ELYSIAN_FS_HTTP_STATUS_PAGE_ABS_PATH, 
	.ptr = (uint8_t*) elysian_default_http_status_page, 
	.size = sizeof(elysian_default_http_status_page) - 1
};

elysian_err_t elysian_fs_rom_fopen(elysian_t* server, char* abs_path, elysian_file_mode_t mode, elysian_file_t* file){
	int i;
	
    ELYSIAN_LOG("Opening file <%s>..", abs_path);
    
	if(mode == ELYSIAN_FILE_MODE_WRITE){
		return ELYSIAN_ERR_FATAL;
	}
	
	if (server->file_rom_def) {
		for(i = 0; server->file_rom_def[i].name != NULL; i++){
			printf("checking with %s --------- %s\r\n",  abs_path, server->file_rom_def[i].name);
			if(strcmp(abs_path, server->file_rom_def[i].name) == 0){
				(file)->descriptor.rom.def = &server->file_rom_def[i];
				(file)->descriptor.rom.pos = 0;
				return ELYSIAN_ERR_OK;
			}
		}
	}

	/* Try to match with the empty file */
	if (strcmp(abs_path, ELYSIAN_FS_HTTP_STATUS_PAGE_ABS_PATH) == 0) {
		printf("checking with %s --------- %s\r\n",  abs_path, ELYSIAN_FS_HTTP_STATUS_PAGE_ABS_PATH);
		(file)->descriptor.rom.def = &file_rom_def_http_status_page;
		(file)->descriptor.rom.pos = 0;
		return ELYSIAN_ERR_OK;
	}
	
	/* Try to match with the empty file */
	if (strcmp(abs_path, ELYSIAN_FS_EMPTY_FILE_ABS_PATH) == 0) {
		printf("checking with %s --------- %s\r\n",  abs_path, ELYSIAN_FS_EMPTY_FILE_ABS_PATH);
		(file)->descriptor.rom.def = &file_rom_def_empty_file;
		(file)->descriptor.rom.pos = 0;
		return ELYSIAN_ERR_OK;
	}

	return ELYSIAN_ERR_NOTFOUND;
}

elysian_err_t elysian_fs_rom_fsize(elysian_t* server, elysian_file_t* file, uint32_t* filesize){
    elysian_file_rom_t* file_rom = &file->descriptor.rom;
	*filesize = file_rom->def->size;
	return ELYSIAN_ERR_OK;
}


elysian_err_t elysian_fs_rom_fseek(elysian_t* server, elysian_file_t* file, uint32_t seekpos){
    elysian_file_rom_t* file_rom = &file->descriptor.rom;
	file_rom->pos = seekpos;
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_fs_rom_ftell(elysian_t* server, elysian_file_t* file, uint32_t* seekpos){
    elysian_file_rom_t* file_rom = &file->descriptor.rom;
	*seekpos = file_rom->pos;
	return ELYSIAN_ERR_OK;
}


int elysian_fs_rom_fread(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size){
	uint32_t read_size;
    elysian_file_rom_t* file_rom = &file->descriptor.rom;
    
	read_size = (buf_size > file_rom->def->size - file_rom->pos) ? file_rom->def->size - file_rom->pos : buf_size;
	memcpy(buf, &file->descriptor.rom.def->ptr[file_rom->pos], read_size);
	file_rom->pos += read_size;
	return read_size;
}

int elysian_fs_rom_fwrite(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size){
	return -1;
}

elysian_err_t elysian_fs_rom_fclose(elysian_t* server, elysian_file_t* file){
	/*
	** No special handling is needed.
	*/
    return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_fs_rom_fremove(elysian_t* server, char* abs_path){
	return ELYSIAN_ERR_OK;
}


/* -----------------------------------------------------------------------------------------------------------
 Web Server internal filesystem
----------------------------------------------------------------------------------------------------------- */
elysian_err_t elysian_fs_vrt_fopen(elysian_t* server, char* abs_path, elysian_file_mode_t mode, elysian_file_t* file) {
	elysian_err_t err;
    uint32_t i;
    
    ELYSIAN_LOG("Opening file <%s>..", abs_path);
    
	if(mode == ELYSIAN_FILE_MODE_WRITE){
		return ELYSIAN_ERR_FATAL;
	}

	if (server->file_vrt_def) {
		for(i = 0; server->file_vrt_def[i].name != NULL; i++){
			printf("checking with %s --------- %s\r\n",  abs_path, server->file_vrt_def[i].name);
			if(strcmp(abs_path, server->file_vrt_def[i].name) == 0){
				file->descriptor.vrt.def = &server->file_vrt_def[i];
				file->descriptor.vrt.pos = 0;
				file->descriptor.vrt.varg = NULL;
				err = (file)->descriptor.vrt.def->open_handler(server, &file->descriptor.vrt.varg);
				if(err == ELYSIAN_ERR_OK) {
					return ELYSIAN_ERR_OK;
				} else {
					return ELYSIAN_ERR_FATAL;
				}
			}
		}
	}

	return ELYSIAN_ERR_NOTFOUND;
}

elysian_err_t elysian_fs_vrt_fsize(elysian_t* server, elysian_file_t* file, uint32_t* filesize){
    elysian_file_vrt_t* file_vrt = &file->descriptor.vrt;
	uint8_t buf[256];
	uint32_t read_size;
	int read_size_actual;
	uint32_t initial_seekpos;
	elysian_err_t err;

	*filesize = 0;
	initial_seekpos = file_vrt->pos;
	if (file_vrt->pos) {
		err = file_vrt->def->seekreset_handler(server, file_vrt->varg);
		if (err == ELYSIAN_ERR_OK) {
			file_vrt->pos = 0;
		} else {
			return ELYSIAN_ERR_FATAL;
		}
	} 
	
	/*
	** Get file size
	*/
	do {
		read_size_actual = elysian_fs_vrt_fread(server, file, buf, sizeof(buf));
		if (read_size_actual < 0) {
			return ELYSIAN_ERR_FATAL;
		} else {
			*filesize += read_size_actual;
		}
	} while (read_size_actual != 0);

	if (file_vrt->pos) {
		err = file_vrt->def->seekreset_handler(server, file_vrt->varg);
		if (err == ELYSIAN_ERR_OK) {
			file_vrt->pos = 0;
		} else {
			return ELYSIAN_ERR_FATAL;
		}
	} 
	
	/*
	** Seek at initial pos
	*/
	while (file_vrt->pos < initial_seekpos) {
		read_size = (sizeof(buf) > (initial_seekpos - file_vrt->pos)) ? (initial_seekpos - file_vrt->pos) : sizeof(buf);
		read_size_actual = elysian_fs_vrt_fread(server, file, buf, read_size);
		if (read_size_actual < 0) {
			return ELYSIAN_ERR_FATAL;
		}
	};
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_fs_vrt_fseek(elysian_t* server, elysian_file_t* file, uint32_t seekpos) {
	elysian_file_vrt_t* file_vrt = &file->descriptor.vrt;
	uint8_t buf[256];
	uint32_t read_size;
	int read_size_actual;
	elysian_err_t err;

	if (seekpos < file_vrt->pos) {
		err = file_vrt->def->seekreset_handler(server, file_vrt->varg);
		if (err == ELYSIAN_ERR_OK) {
			file_vrt->pos = 0;
		} else {
			return ELYSIAN_ERR_FATAL;
		}
	} 
	
	while (file_vrt->pos < seekpos) {
		read_size = (sizeof(buf) > (seekpos - file_vrt->pos)) ? (seekpos - file_vrt->pos) : sizeof(buf);
		read_size_actual = elysian_fs_vrt_fread(server, file, buf, read_size);
		if (read_size_actual < 0) {
			return ELYSIAN_ERR_FATAL;
		}
		if ((read_size_actual != read_size) && (file_vrt->pos < seekpos)) {
			return ELYSIAN_ERR_FATAL;
		}
	};

	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_fs_vrt_ftell(elysian_t* server, elysian_file_t* file, uint32_t* seekpos){
    elysian_file_vrt_t* file_vrt = &file->descriptor.vrt;
	*seekpos = file_vrt->pos;
	return ELYSIAN_ERR_OK;
}

int elysian_fs_vrt_fread(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size){
	elysian_file_vrt_t* file_vrt = &file->descriptor.vrt;
	int read_size;

	//read_size = (buf_size > file_rom->size - file_vrt->pos) ? file_rom->size - file_rom->pos : buf_size;
	read_size = file_vrt->def->read_handler(server, file_vrt->varg, buf, buf_size);
	if (read_size > 0) {
		file_vrt->pos += read_size;
	}
	return read_size;
}

int elysian_fs_vrt_fwrite(elysian_t* server, elysian_file_t* file, uint8_t* buf, uint32_t buf_size){
	ELYSIAN_ASSERT(0);
	return -1;
}

elysian_err_t elysian_fs_vrt_fclose(elysian_t* server, elysian_file_t* file){
	elysian_file_vrt_t* file_vrt = &file->descriptor.vrt;
	file_vrt->def->close_handler(server, file_vrt->varg);
    return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_fs_vrt_fremove(elysian_t* server, char* abs_path){
	ELYSIAN_ASSERT(0);
	return ELYSIAN_ERR_FATAL;
}
