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

#if 0
uint32_t elysian_strcmp_file(elysian_file_t* file, uint32_t from_index, char* pattern, uint8_t match_case){
	uint16_t buf_len, buf_sz;
	uint32_t init_index;
	uint16_t tmp_buf_offset;
	uint8_t match;
	uint8_t tmp_buf[128];
	uint16_t pattern_len;
	uint16_t i;
	uint8_t c1, c2;
	
	pattern_len = strlen(pattern);
	init_index = ftell(file);
	if(from_index != init_index){
		fseek(file, from_index, SEEK_SET);
	}
	
	match = 1;
	buf_sz = 0;
	buf_len = 0;
	tmp_buf_offset = 0;
	for(i = 0; i < pattern_len; i++){
		if(buf_len == 0){
			tmp_buf_offset += buf_sz;
			buf_sz = fread(tmp_buf, 1, sizeof(tmp_buf), file);
			if(buf_sz == 0){
				match = 0;
				goto op_finished;
			}
			buf_len = buf_sz;
		}
		c1 = pattern[i];
		c2 = tmp_buf[i - tmp_buf_offset];
		if(c1 != c2){
			match = 0;
			goto op_finished;
		}
		buf_len--;
	}
	
	op_finished:
	fseek(file, init_index, SEEK_SET);
	return !match;
}
#endif

elysian_err_t elysian_strncpy_file(elysian_t* server, elysian_file_t* file, uint32_t offset, char* str, uint32_t n){
	elysian_err_t err;
	uint32_t read_sz;
	uint32_t current_offset;
	
    /*
	** Seek to the appropriate file position
	*/
	err = elysian_fs_ftell(server, file, &current_offset);
    if(err != ELYSIAN_ERR_OK){
        return err;
    }
	if(current_offset != offset){
		err = elysian_fs_fseek(server, file, offset);
        if(err != ELYSIAN_ERR_OK){
            return err;
        }
	}
	
    /*
    ** Read n bytes
    */
    err = elysian_fs_fread(server, file, (uint8_t*) str, n, &read_sz);
    if(err != ELYSIAN_ERR_OK){
        return err;
    }
	if(read_sz != n){
		return ELYSIAN_ERR_FATAL;
	}

    str[n] = '\0';
    
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_strstr_file(elysian_t* server, elysian_file_t* file, uint32_t offset, char* pattern1, char* pattern2, uint8_t match_case, uint32_t* index1, uint32_t* index2){
	uint8_t tmp_buf[128 + 1];  /* This is the maximum size of pattern we can search for */
	uint8_t* buf;
	uint32_t buf_sz;
	char* pattern;
	uint32_t pattern_len;
	uint32_t current_offset;
	uint32_t read_len;
	uint8_t eof;
	uint32_t index;
	uint32_t match_index;
	uint32_t buf_index, buf_index0, buf_index1, pattern_index;
    elysian_err_t err;

	/*
	** Seek to the appropriate file position
	*/
	err = elysian_fs_ftell(server, file, &current_offset);
    if(err != ELYSIAN_ERR_OK){
        return err;
    }
	if(current_offset != offset){
		err = elysian_fs_fseek(server, file, offset);
        if(err != ELYSIAN_ERR_OK){
            return err;
        }
	}
	
	/*
	** Try to allocate a bigger buffer to speedup the search process.
	** If we are out of memory we are going to work with tmp_buf.
	*/
	buf_sz = 2 * 512;
	while(1){
		buf = elysian_mem_malloc(server, buf_sz + 1);
		if(!buf){
			buf_sz = (buf_sz > 512) ? buf_sz - 512 : 0;
			if(buf_sz <= sizeof(tmp_buf)){
				buf = tmp_buf;
				buf_sz = sizeof(tmp_buf) - 1;
				break;
			}
		}else{
			break;
		}
	};
	
	eof = 0;
	*index1 = ELYSIAN_INDEX_OOB32;
	*index2 = ELYSIAN_INDEX_OOB32;
	pattern = pattern1;
	pattern_len = strlen(pattern1);
	match_index = offset;
	buf_index0 = 0;
	buf_index1 = 0;
	pattern_index = 0;
	buf_index = 0;
	while(1){
		if(buf_index == buf_index1){
			if(eof){
				err = ELYSIAN_ERR_OK;
				goto op_finished; /* The whole file stream was processed */
			}

			for(index = 0; index < pattern_index; index++){
				buf[index] = buf[buf_index0 + index];
			}
			
			match_index += buf_index0;
			buf_index -= buf_index0;
			
			buf_index0 = 0;
			buf_index1 = pattern_index;
			
			if(buf_sz - buf_index1 == 0){
				goto op_finished; /* Pattern len is bigger than our working buffer, abort */
			}else{
				//f_read(file, &buf[buf_index1], buf_sz - buf_index1,(UINT*)&read_len);
                err = elysian_fs_fread(server, file, &buf[buf_index1], buf_sz - buf_index1, &read_len);
                if(err != ELYSIAN_ERR_OK){
                    goto op_finished;
                }
				if(read_len != buf_sz - buf_index1){
					eof = 1;
				}
				buf_index1 += (uint32_t) read_len;
				buf[buf_index1] = '\0';
			}
		}
		

		int match;
		if(match_case){
            match = buf[buf_index] == pattern[pattern_index];
        }else{
            match = toupper(buf[buf_index]) == toupper(pattern[pattern_index]);
        }
		
		//printf("buf='%s', pattern = '%s', buf_index = %u, pattern_index = %u\r\n", buf, pattern, buf_index, pattern_index);
		if(match){
			buf_index++;
			pattern_index++;
			if(pattern_index == pattern_len){
				/*
				** Match
				*/
				if(pattern == pattern1){
					*index1 = match_index + buf_index0;
					buf_index0 += pattern_len;
					pattern_index = 0;
					buf_index = buf_index0;
					pattern = pattern2;
					pattern_len = strlen(pattern2);
					if(pattern_len == 0){
						ELYSIAN_LOG("pattern_len == 0\r\n");
						err = ELYSIAN_ERR_OK;
						goto op_finished;
					}
				}else{
					*index2 = match_index + buf_index0;
					err = ELYSIAN_ERR_OK;
					goto op_finished;
				}
			}
		}else{
			buf_index0++;
			pattern_index = 0;
			buf_index = buf_index0;
		}
	}
    
op_finished:
	
	if(buf != tmp_buf){
		elysian_mem_free(server, buf);
	}
    
    return ELYSIAN_ERR_OK;
}

#if 0

int main(){
	FILE * pFile;
	//long size;

	pFile = fopen ("test.txt","rb");
	if (pFile==NULL){return -1;}
	
	printf("Found pos = %u\r\n", file_strstr(pFile, 0, "pattern", 0));
	
	printf("compare = %u\r\n",file_strcmp(pFile, 34, "patte", 0));
	
	
	return 0;
};
#endif
