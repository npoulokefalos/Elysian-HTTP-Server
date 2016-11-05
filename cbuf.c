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

elysian_cbuf_t* elysian_cbuf_alloc(elysian_t* server, uint8_t* data, uint32_t len){
	elysian_cbuf_t* cbuf = elysian_mem_malloc(server, sizeof(elysian_cbuf_t) + len + 1 /* '\0' */, ELYSIAN_MEM_MALLOC_PRIO_NORMAL);
	if(cbuf){
		cbuf->next = NULL;
		cbuf->len = len;
		cbuf->data = cbuf->wa;
		if(data){
			memcpy(cbuf->data, data, len);
		}else{
			memset(cbuf->data, 0, len);
		}
		cbuf->data[len] = '\0';
	}
	return cbuf;
}

void elysian_cbuf_free(elysian_t* server, elysian_cbuf_t* cbuf){
	elysian_mem_free(server, cbuf);
}

void elysian_cbuf_list_append(elysian_cbuf_t** cbuf_list, elysian_cbuf_t* cbuf_new){
	elysian_cbuf_t* cbuf;
	// TODO, ensure that last cbuf->next = NULL
	
	if(*cbuf_list == NULL) {
		*cbuf_list = cbuf_new;
        return;
	}
    cbuf = *cbuf_list;
	while(cbuf->next){
		cbuf = cbuf->next;
	}
	cbuf->next = cbuf_new;
}

void elysian_cbuf_list_free(elysian_t* server, elysian_cbuf_t* cbuf_list){
	elysian_cbuf_t* cbuf_next;
	while(cbuf_list){
        cbuf_next = cbuf_list->next;
        elysian_cbuf_free(server, cbuf_list);
		cbuf_list = cbuf_next;
	}
}

uint32_t elysian_cbuf_list_len(elysian_cbuf_t* cbuf_list){
	elysian_cbuf_t* cbuf = cbuf_list;
	uint32_t len = 0;
	while(cbuf){
		len += cbuf->len;
        cbuf = cbuf->next;
	}
	return len;
}

elysian_err_t elysian_cbuf_list_split(elysian_t* server, elysian_cbuf_t** cbuf_list0, uint32_t size, elysian_cbuf_t** cbuf_list1) {
	elysian_cbuf_t* cbuf;
	elysian_cbuf_t* cbuf_next;
	elysian_err_t err;
	
	if(size == 0){
        return ELYSIAN_ERR_OK;
    }
	
	ELYSIAN_ASSERT(*cbuf_list0 != NULL, "");
	//ELYSIAN_ASSERT(*cbuf_list1 == NULL, "");
	
	err = elysian_cbuf_rechain(server, cbuf_list0, size);
	if(err != ELYSIAN_ERR_OK){
		return err;
	}
	
	*cbuf_list1 = NULL;
	while(size){
		cbuf = *cbuf_list0;
		ELYSIAN_ASSERT(cbuf != NULL, "");
        ELYSIAN_ASSERT(size >= cbuf->len, "");
        
		cbuf_next = cbuf->next;
        size -= cbuf->len;
        *cbuf_list0 = cbuf->next;
        cbuf->next = NULL;
        elysian_cbuf_list_append(cbuf_list1, cbuf);
        
        if(size == 0){
            return ELYSIAN_ERR_OK;
        }
        
		cbuf = cbuf_next;
	}

	ELYSIAN_ASSERT(0, "");
	return ELYSIAN_ERR_FATAL;
}

#if 0
elysian_cbuf_t* elysian_cbuf_chain(elysian_cbuf_t* cbuf0, elysian_cbuf_t* cbuf1){
	elysian_cbuf_t* cbuf = cbuf0;
	if(!cbuf) {
		return cbuf1;
	}
	while(cbuf->next){
		cbuf = cbuf->next;
	}
	cbuf->next = cbuf1;
	return cbuf0;
}
#endif

/*
** Splits the first cbuf of the cahin so it has exactly "size" len
*/
elysian_err_t elysian_cbuf_rechain(elysian_t* server, elysian_cbuf_t** cbuf_list, uint32_t size){
	elysian_cbuf_t* cbuf_new;
    elysian_cbuf_t* cbuf_prev;
    elysian_cbuf_t* cbuf;
	uint16_t alloc_sz1, alloc_sz2;
	
    if(size == 0){
        return ELYSIAN_ERR_OK;
    }
    
	ELYSIAN_ASSERT((*cbuf_list) != NULL, "");
	
    cbuf_prev = NULL;
    cbuf = *cbuf_list;
	while(cbuf){
        if(size == cbuf->len){
            return ELYSIAN_ERR_OK;
        }else if(size < cbuf->len){
            break;
        }else{
            size -= cbuf->len;
            cbuf_prev = cbuf;
            cbuf = cbuf->next;
        }
	}
    
    ELYSIAN_ASSERT(cbuf != NULL, "");
    ELYSIAN_ASSERT(cbuf->len > size, "");

   
	/*
	** Required allocation size if we are going to prepend a cbuf
	*/
	alloc_sz1 = size;
	
	/*
	** Required allocation size if we are going to append a cbuf
	*/
	alloc_sz2 = cbuf->len - size;
	
    ELYSIAN_LOG("Rechain sz1=%u, sz2=%u\r\n",alloc_sz1,alloc_sz2);
     
	if(alloc_sz1 < alloc_sz2){
		/* Prepend */
		cbuf_new = elysian_cbuf_alloc(server, &(cbuf->data)[0], alloc_sz1);
		if(!cbuf_new){
			return ELYSIAN_ERR_POLL;
		}
		cbuf_new->next = cbuf;
        if(cbuf_prev == NULL){
            (*cbuf_list) = cbuf_new;
        }else{
            cbuf_prev->next = cbuf_new;
        }
		cbuf_new->next->data = &cbuf_new->next->data[alloc_sz1];
		cbuf_new->next->len -= alloc_sz1;
	}else{
		/* Append */
		cbuf_new = elysian_cbuf_alloc(server, &cbuf->data[size], alloc_sz2);
		if(!cbuf_new){
			return ELYSIAN_ERR_POLL;
		}
		cbuf_new->next = cbuf->next;
		cbuf->next = cbuf_new;
		cbuf->data[size] = '\0';
		cbuf->len -= alloc_sz2;
	}
	
	return ELYSIAN_ERR_OK;
}

void elysian_cbuf_strget(elysian_cbuf_t* cbuf, uint32_t cbuf_index, char* buf, uint32_t buf_len){
    uint32_t copy_size;
    uint32_t buf_index;
    
    while(cbuf){
		if(cbuf_index < cbuf->len){
            break;
        }
		cbuf_index -= cbuf->len;
		cbuf = cbuf->next;
	}
    
    buf_index = 0;
    while(buf_index < buf_len){
        ELYSIAN_ASSERT(cbuf != NULL, "");
        copy_size = (cbuf->len - cbuf_index > buf_len - buf_index) ? buf_len - buf_index : cbuf->len - cbuf_index;
        memcpy(&buf[buf_index], &cbuf->data[cbuf_index], copy_size);
        cbuf_index +=  copy_size;
        buf_index += copy_size;
        if(cbuf_index == cbuf->len){
            cbuf = cbuf->next;
            cbuf_index = 0;
        }
    }
}

#if 1
uint8_t elysian_cbuf_strcmp(elysian_cbuf_t* cbuf, uint32_t index, char* str, uint8_t matchCase){
	unsigned char c1;
	unsigned char c2;
    uint32_t  strIndex,strLen;

	while(cbuf){
		if(index < cbuf->len){break;}
		index -= cbuf->len;
		cbuf = cbuf->next;
	}
    
    ELYSIAN_ASSERT(cbuf != NULL, "");
    
	strLen = strlen(str);
    
    for(strIndex = 0; strIndex < strLen; strIndex++){
        if(!cbuf) {return 1;}

        c1 = str[strIndex];
        c2 = cbuf->data[index];
        
        if(matchCase){
            if(c1 != c2){return 1;}
        }else{
            if(toupper(c1) != toupper(c2)) {return 1;}
        }
        
        index++;
        if(index == cbuf->len){
            cbuf = cbuf->next;
            index = 0;
        }
    }
    return 0;
}

void elysian_cbuf_strcpy(elysian_cbuf_t* cbuf, uint32_t index0, uint32_t index1, char* str){
    uint32_t copy_len;
    uint32_t index;

    ELYSIAN_ASSERT(index0 <= index1, "");
    
    *str = '\0';

	while(cbuf){
		if(index0 < cbuf->len){break;}
		index0 -= cbuf->len;
        index1 -= cbuf->len;
		cbuf = cbuf->next;
	}
    
    copy_len = index1 - index0 + 1;
	for(index = 0; index < copy_len; index++){
        ELYSIAN_ASSERT(cbuf != NULL, "");
        
		str[index] = cbuf->data[index0++];

		if(index0 == cbuf->len){
            index0 = 0;
			cbuf = cbuf->next;
		}
	}
    
    str[index] = '\0';
}
#endif

uint32_t elysian_cbuf_strstr(elysian_cbuf_t* cbuf0, uint32_t index, char* str, uint8_t matchCase){
	unsigned char c1;
	unsigned char c2;
	elysian_cbuf_t* cbuf;
	uint32_t strIndex,tmpBufIndex,skipIndex;
	uint32_t strLen;
	uint8_t found;

	skipIndex = 0;
	while(cbuf0){
		if(index < cbuf0->len){break;}
		skipIndex += cbuf0->len;
		index -= cbuf0->len;
		cbuf0 = cbuf0->next;
	}

	strLen  = strlen((char*)str);
	while(cbuf0){
		cbuf 		= cbuf0;
		tmpBufIndex = index;
		found 		= 1;
        
		for(strIndex = 0; strIndex < strLen; strIndex++){
            
            if(tmpBufIndex == cbuf->len){
                cbuf = cbuf->next;
                if(!cbuf) {
                    //found = 0; 
                    //break;
                    return ELYSIAN_INDEX_OOB32;
                }
                tmpBufIndex = 0;
            }

			c1 = str[strIndex];
			c2 = cbuf->data[tmpBufIndex];
            
            if(matchCase){
                if(c1 != c2){
                    found = 0; 
                    break;
                }
            }else{
                if(toupper(c1) != toupper(c2)){
                    found = 0; 
                    break;
                }
            }
            
            tmpBufIndex++;
		}

		if(found) {return skipIndex + index;}; /* Found */

		index++;
		if(index == cbuf0->len){
			skipIndex += cbuf0->len;
			cbuf0 = cbuf0->next;
			index = 0;
		}
	};

	return ELYSIAN_INDEX_OOB32;
}

#if 0
void elysian_cbuf_copy_escape(elysian_cbuf_t* cbuf, uint32_t index0, uint32_t index1, uint8_t* str){//} uint32_t copyLen){
    uint32_t index;
    uint16_t encodedStrIndex;
    uint8_t encodedStr[3];
    uint16_t len;
    //HS_ENSURE(parentBuf != NULL,"hsPbufStrCpyN(): parrentPbuf != NULL");

    *str = '\0';

    while(index0 >= cbuf->len){
        index0 -= cbuf->len;
        cbuf = cbuf->next;
    };
    
    encodedStrIndex = 0;
    len = index1 - index0;
    for(index = 0; index <= len; index++){
        encodedStr[encodedStrIndex] = cbuf->data[index0];
        if((encodedStrIndex == 0) && (encodedStr[encodedStrIndex] != '%')){
            *str++ = encodedStr[encodedStrIndex];
        }else if(encodedStrIndex == 2){
            encodedStrIndex = 0;
            // copy escaped
            //*str++ =
        }else{
            encodedStrIndex++;
        }
        
        if(index0 == cbuf->len){
            cbuf = cbuf->next;
            index0 = 0;
            //if(!parentBuf) {HS_ENSURE(0,"hsPbufStrCpyN(): invalid index [2]!"); return; }
        }
    }
}

elysian_cbuf_t* elysian_cbuf_list_dropn(elysian_cbuf_t* cbuf, uint32_t n){
	elysian_cbuf_t* cbuf_next;

	while(cbuf){
        cbuf_next = cbuf->next;
		if(n < cbuf->len){
            cbuf->data = &cbuf->data[n];
            cbuf->len -= n;
            break;
        }
		n -= cbuf->len;
        elysian_cbuf_free(server, cbuf);
		cbuf = cbuf_next;
	}
    
    return cbuf;
}
#endif

#if 0
cbuf_t* elysian_cbuf_get(cbuf_t* cbuf, uint32_t absolute_index, uint32_t* relative_index){
	while(cbuf){
		if(absolute_index < cbuf->len){*relative_index = absolute_index; return cbuf;}
		absolute_index -= cbuf->len;
		cbuf = cbuf->next;
	}
	ENSURE(0);
	return NULL;
}
cbuf_t* elysian_cbuf_replace(cbuf_t* cbuf, char* str0, char* str1){
	uint32_t index0;
	uint32_t index1;
	uint32_t relative_index0;
	uint32_t relative_index1;
	cbuf_t* cbuf0;
	cbuf_t* cbuf1;
	cbuf_t* cbuf_tmp;
	index0 = elysian_cbuf_strstr(cbuf, 0, str0, 0);
	if(index0 == ELYSIAN_INDEX_OOB32){
		printf("NOT FOUND!!!\r\n");
	}
		
	cbuf0 = cbuf;
	cbuf0_prev = NULL;
	while(cbuf0){
		if(index0 < cbuf0->len){break;}
		index0 -= cbuf0->len;
		cbuf0_prev = cbuf0;
		cbuf0 = cbuf0->next;
	}
	cbuf1 = cbuf0;
	index1 = index0 + strlen(str0) - 1;
	while(cbuf1){
		if(index1 < cbuf1->len){break;}
		index1 -= cbuf1->len;
		cbuf1 = cbuf1->next;
	}
	return NULL;
}
#endif

void cbuf_list_print(elysian_cbuf_t* cbuf){
	uint32_t index = 0;
    
	printf("Printing cbuf chain..\r\n");
	while(cbuf){
		printf("[cbuf_%u, len %u] -> '%s'\r\n", index++, cbuf->len,  cbuf->data);
		cbuf = cbuf->next;
	}
}

#if 0
int main(){
	char* text0 = "text0 data 00000";
	char* text1 = "text1 data 11111";
	char* text2 = "text2 data 22222";
	
	elysian_cbuf_t* cbuf[3];
	elysian_cbuf_t* cbuf_start;
	printf("Start..\r\n");
	
	cbuf[0] = elysian_cbuf_alloc(server, (uint8_t*) text0, strlen(text0));
	cbuf[1] = elysian_cbuf_alloc(server, (uint8_t*) text1, strlen(text1));
	cbuf[2] = elysian_cbuf_alloc(server, (uint8_t*) text2, strlen(text2));
	
	
	cbuf_start = elysian_cbuf_chain(cbuf[0], cbuf[1]);
	cbuf_start = elysian_cbuf_chain(cbuf_start, cbuf[2]);
	
	cbuf_print(cbuf_start);
	
	elysian_cbuf_rechain(cbuf_start, 1);
	
	cbuf_print(cbuf_start);
	
	printf("strstr result = %u\r\n", elysian_cbuf_strstr(cbuf_start, 0, "1t", 0));
	
	cbuf_replace(cbuf_start, "1t", "xx");
	
	cbuf_print(cbuf_start);
	
	return 0;
}
#endif
