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

elysian_err_t elysian_resource_open_static(elysian_t* server) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
    err = elysian_fs_fopen(server, client->mvc.view, ELYSIAN_FILE_MODE_READ, &client->resource->file);
    if(err != ELYSIAN_ERR_OK){
        return err;
    }
    return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_resource_size_static(elysian_t* server, uint32_t* resource_size) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	err = elysian_fs_fsize(server, &client->resource->file, resource_size);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	client->resource->calculated_size = *resource_size;
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_resource_seek_static(elysian_t* server, uint32_t seekpos) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	err = elysian_fs_fseek(server, &client->resource->file, client->mvc.range_start);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_resource_read_static(elysian_t* server, uint8_t* readbuf, uint32_t readbufsz, uint32_t* readbufszactual){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
    err = elysian_fs_fread(server, &client->resource->file, readbuf, readbufsz, readbufszactual);
    if (err != ELYSIAN_ERR_OK) {
        return err;
    }
    return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_resource_close_static(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
    err = elysian_fs_fclose(server, &client->resource->file);
    return err;
}


/* -------------------------------------------------------------------------------------------------------------------------- */
elysian_err_t elysian_resource_open_dynamic(elysian_t* server);
elysian_err_t elysian_resource_size_dynamic(elysian_t* server, uint32_t* size);
elysian_err_t elysian_resource_seek_dynamic (elysian_t* server, uint32_t seekpos);
elysian_err_t elysian_resource_read_dynamic(elysian_t* server, uint8_t* read_buf, uint32_t read_buf_sz, uint32_t* read_buf_sz_actual);
elysian_err_t elysian_resource_close_dynamic(elysian_t* server);

typedef struct elysian_resource_dynamic_priv_t elysian_resource_dynamic_priv_t;
struct elysian_resource_dynamic_priv_t {
	uint32_t seekpos;
    uint8_t* wbuf;
    
    uint8_t sbuf[128]; /* Search buffer */
    uint16_t sbuf_index0;
    uint16_t sbuf_index1;
    
    uint16_t rbuf_index0; /* Replace buffer */
    uint16_t rbuf_index1;
    
	uint8_t shadow_buf_size;
	
    uint8_t eof;
};

elysian_err_t elysian_resource_open_dynamic(elysian_t* server) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_resource_dynamic_priv_t* priv;
    elysian_err_t err;
	
    err = elysian_fs_fopen(server, client->mvc.view, ELYSIAN_FILE_MODE_READ, &client->resource->file);
    if (err != ELYSIAN_ERR_OK) {
        return err;
    }

    if (!(priv = elysian_mem_malloc(server, sizeof(elysian_resource_dynamic_priv_t)))) {
        elysian_fs_fclose(server, &client->resource->file);
        return ELYSIAN_ERR_POLL;
    }
    
	priv->seekpos = 0;
	client->resource->priv = priv;
	
	err = elysian_resource_seek_dynamic(server, 0);
	if (err != ELYSIAN_ERR_OK) {
		elysian_fs_fclose(server, &client->resource->file);
        return err;
    }
	
    return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_resource_size_dynamic(elysian_t* server, uint32_t* resource_size) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	uint32_t initial_pos;
	elysian_resource_dynamic_priv_t* priv = (elysian_resource_dynamic_priv_t*)client->resource->priv;
	*resource_size = 0;

	if (client->resource->calculated_size == -1) {
		initial_pos = priv->seekpos;
		
		err = elysian_resource_seek_dynamic(server, ELYSIAN_FILE_SEEK_END);
		if(err != ELYSIAN_ERR_OK){
			return err;
		}
		
		client->resource->calculated_size = priv->seekpos;
		*resource_size = client->resource->calculated_size;
		
		if (priv->seekpos != initial_pos) {
			err = elysian_resource_seek_dynamic(server, initial_pos);
			if(err != ELYSIAN_ERR_OK){
				return err;
			}
		}
	} else {
		/*
		** Size already calculated
		*/
		*resource_size = client->resource->calculated_size;
	}
	
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_resource_seek_dynamic (elysian_t* server, uint32_t seekpos) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_resource_dynamic_priv_t* priv = (elysian_resource_dynamic_priv_t*)client->resource->priv;
	uint32_t read_size_actual;
	uint32_t read_size;
	elysian_err_t err;
	uint32_t resource_size;
	
	/*
	** Always seek if seekpos is 0 (resource_open requires proper initialization -- priv->seekpos is not init)
	*/
	if ((seekpos == 0) || (priv->seekpos > seekpos)) {
		
        err = elysian_fs_fseek(server, &client->resource->file, 0);
        if (err != ELYSIAN_ERR_OK) {
            return err;
        }

		priv->seekpos = 0;
		
        priv->wbuf = priv->sbuf;
        priv->sbuf_index0 = 1;
        priv->sbuf_index1 = 1;
		priv->shadow_buf_size = 0;
		
        priv->eof = 0;
    }
	
	/*
	** Seek to desired position
	*/
	resource_size = 0;
	while (seekpos - priv->seekpos) {
		read_size = seekpos - priv->seekpos;
		if (read_size > 1024) {
			read_size = 1024;
		}
		err = client->resource->read(server, NULL, read_size, &read_size_actual);
		if (err != ELYSIAN_ERR_OK) {
			return err;
		}
		resource_size += read_size_actual;
		if (read_size_actual != read_size) {
			break;
		}
	};
	
	if (seekpos == ELYSIAN_FILE_SEEK_END) {
		return ELYSIAN_ERR_OK;
	} else {
		if (priv->seekpos != seekpos) {
			return ELYSIAN_ERR_FATAL;
		} else {
			return ELYSIAN_ERR_OK;
		}
	}
}

elysian_err_t elysian_resource_read_dynamic(elysian_t* server, uint8_t* read_buf, uint32_t read_buf_sz, uint32_t* read_buf_sz_actual){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_resource_dynamic_priv_t* priv = ((elysian_resource_dynamic_priv_t*)client->resource->priv);
	uint16_t read_buf_index;
	uint16_t copy_sz;
	char* prefix = "<%=";
	char* suffix = "%>";
	uint16_t prefix_len = strlen(prefix);
	uint16_t suffix_len = strlen(suffix);
	char* suffix_ptr;
	uint32_t read_len;
	uint32_t index;
	elysian_err_t err;
	
#define shift_and_fill_sbuf(server, file, priv) \
{ \
	for(index = 0; index < priv->sbuf_index1 - priv->sbuf_index0 + priv->shadow_buf_size; index++){\
		priv->sbuf[index] = priv->sbuf[priv->sbuf_index0 + index]; \
	} \
	priv->sbuf_index1 = priv->sbuf_index1 - priv->sbuf_index0 + priv->shadow_buf_size; \
	priv->sbuf_index0 = 0; \
	if(!priv->eof){ \
		err = elysian_fs_fread(server, &client->resource->file, &priv->sbuf[priv->sbuf_index1], sizeof(priv->sbuf) - 1 - priv->sbuf_index1, &read_len); \
		if(err != ELYSIAN_ERR_OK){ \
			return err; \
		} \
		read_len = (read_len > 0) ? read_len : 0; \
		if(read_len < sizeof(priv->sbuf) - 1 - priv->sbuf_index1){ \
			priv->eof = 1; \
		} \
	}else{ \
		read_len = 0; \
	} \
	priv->sbuf_index1 += read_len; \
	priv->sbuf[priv->sbuf_index1] = '\0'; \
	if(priv->sbuf_index1 <= prefix_len){ \
		priv->shadow_buf_size = 0; \
	}else{ \
		priv->shadow_buf_size = prefix_len - 1; \
	} \
	priv->sbuf_index1 -= priv->shadow_buf_size; \
}	
		
	*read_buf_sz_actual = 0;
	read_buf_index = 0;
	while (read_buf_index < read_buf_sz){
		if(priv->wbuf != priv->sbuf){
			/*
			** Replace {prefix}{attribute_name}{suffix} with attribute's value
			*/
            ELYSIAN_LOG("Replacing..");
			copy_sz = read_buf_sz - read_buf_index > priv->rbuf_index1 - priv->rbuf_index0 ? priv->rbuf_index1 - priv->rbuf_index0 : read_buf_sz - read_buf_index;
			if (read_buf){
				memcpy(&read_buf[read_buf_index], (uint8_t*) &priv->wbuf[priv->rbuf_index0], copy_sz);
			}
			read_buf_index += copy_sz;
			priv->rbuf_index0 += copy_sz;
			if(priv->rbuf_index0 == priv->rbuf_index1){
				priv->wbuf = priv->sbuf;
				continue;
			}
		}else{
			/*
			** Copy input to output until {prefix}{attribute_name}{suffix} is detected.
			*/
            //ELYSIAN_LOG("Searching..");
			if (priv->sbuf_index0 && priv->sbuf_index0 == priv->sbuf_index1){
				shift_and_fill_sbuf(server, file, priv);
                ELYSIAN_LOG("index0=%u, index1=%u", priv->sbuf_index0, priv->sbuf_index1);
				if(priv->sbuf_index0 == priv->sbuf_index1){break;}
			}
			ELYSIAN_ASSERT( priv->sbuf_index0 <= priv->sbuf_index1);
			if(priv->sbuf_index0 == priv->sbuf_index1){
				break; /* Whole input processed */
			}
			if(priv->sbuf[priv->sbuf_index0] == prefix[0]){
				if(memcmp(&priv->sbuf[priv->sbuf_index0], prefix, prefix_len) == 0){
					shift_and_fill_sbuf(server, file, priv);
					suffix_ptr = elysian_strstr((char*) &priv->sbuf[priv->sbuf_index0 + prefix_len], suffix);
					if(suffix_ptr){
						/*
						** {prefix}{attribute_name}{suffix} detected
						*/
						*suffix_ptr = '\0';
						char* attribute_name = (char*) &priv->sbuf[priv->sbuf_index0 + prefix_len];
						while((char*) &priv->sbuf[priv->sbuf_index0] < (char*) &suffix_ptr[suffix_len]){
							priv->sbuf_index0++;
							if(priv->sbuf_index0 > priv->sbuf_index1){ /* Some part may belong to the shadow buffer, take care of it */
								ELYSIAN_ASSERT(priv->shadow_buf_size);
								priv->sbuf_index1++;
								priv->shadow_buf_size--;
							}
						}
						elysian_mvc_attribute_t* attribute = elysian_mvc_attribute_get(server, attribute_name);
						if(attribute){
							priv->wbuf  = (uint8_t*) attribute->value;
						}else{
							priv->wbuf = (uint8_t*) "";
						}
						priv->rbuf_index0 = 0;
						priv->rbuf_index1 = strlen((char*)priv->wbuf);
						continue;
					}
				}
			}
			if(read_buf){
				read_buf[read_buf_index] = priv->sbuf[priv->sbuf_index0];
			}
			read_buf_index++;
			priv->sbuf_index0++;
		}
    }
	*read_buf_sz_actual = read_buf_index;
	priv->seekpos += read_buf_index;
	
	return ELYSIAN_ERR_OK;
#undef shift_and_fill_sbuf
}

elysian_err_t elysian_resource_close_dynamic(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
    
    elysian_mem_free(server, client->resource->priv);
    err = elysian_fs_fclose(server, &client->resource->file);
    return err;
}

/* -------------------------------------------------------------------------------------------------------------------------- */
void elysian_resource_init(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	
	client->resource = NULL;
}

elysian_err_t elysian_resource_open(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
    
    if(!(client->resource = elysian_mem_malloc(server, sizeof(elysian_resource_t)))){
        return ELYSIAN_ERR_POLL;
    }
	
	client->resource->priv = NULL;
	client->resource->calculated_size = -1;
	
    if (client->mvc.attributes) {
        client->resource->open = elysian_resource_open_dynamic;
		client->resource->size = elysian_resource_size_dynamic;
		client->resource->seek = elysian_resource_seek_dynamic;
        client->resource->read = elysian_resource_read_dynamic;
        client->resource->close = elysian_resource_close_dynamic;
    } else {
        client->resource->open = elysian_resource_open_static;
		client->resource->size = elysian_resource_size_static;
		client->resource->seek = elysian_resource_seek_static;
        client->resource->read = elysian_resource_read_static;
        client->resource->close = elysian_resource_close_static;
    }
    
	err = client->resource->open(server);
	if (err != ELYSIAN_ERR_OK) {
		elysian_mem_free(server, client->resource);
		client->resource = NULL;
	}
	
    switch (err) {
        case ELYSIAN_ERR_OK:
		{
			return ELYSIAN_ERR_OK;
		}break;
        case ELYSIAN_ERR_NOTFOUND:
		{
            return ELYSIAN_ERR_NOTFOUND;
		}break;
        case ELYSIAN_ERR_POLL:
		{
            return ELYSIAN_ERR_POLL;
		}break;
        default:
		{
			ELYSIAN_ASSERT(0);
            return ELYSIAN_ERR_FATAL;
		}break;
    };

	return ELYSIAN_ERR_OK;
}

uint8_t elysian_resource_isopened(elysian_t* server){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	
	return (client->resource != NULL);
}

elysian_err_t elysian_resource_size(elysian_t* server, uint32_t * resource_size) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	
	ELYSIAN_ASSERT(client->resource != NULL);
	err = client->resource->size(server, resource_size);
	return err;
}

elysian_err_t elysian_resource_seek(elysian_t* server, uint32_t seekpos) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
	elysian_err_t err;
	
	ELYSIAN_ASSERT(client->resource != NULL);
	err = client->resource->seek(server, seekpos);
	return err;
}

elysian_err_t elysian_resource_read(elysian_t* server, uint8_t* readbuf, uint32_t readbufsz, uint32_t* readbufszactual){
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
	
    ELYSIAN_ASSERT(client->resource != NULL);
	if (client->mvc.transfer_encoding == ELYSIAN_HTTP_TRANSFER_ENCODING_IDENTITY) {
		err = client->resource->read(server, readbuf, readbufsz, readbufszactual);
	} else if (client->mvc.transfer_encoding == ELYSIAN_HTTP_TRANSFER_ENCODING_CHUNKED) {
		err = client->resource->read(server, readbuf, readbufsz, readbufszactual);
	} else {
		ELYSIAN_ASSERT(0);
		err = ELYSIAN_ERR_FATAL;
	}
    return err;
}

elysian_err_t elysian_resource_close(elysian_t* server) {
	elysian_client_t* client = elysian_schdlr_current_client_get(server);
    elysian_err_t err;
	
    ELYSIAN_ASSERT(client->resource != NULL);
    err = client->resource->close(server);
    if(err != ELYSIAN_ERR_OK){
        return err;
    }
    elysian_mem_free(server, client->resource);
    client->resource = NULL;
    return ELYSIAN_ERR_OK;
}


