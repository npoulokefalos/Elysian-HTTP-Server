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
#include <stdarg.h>
void elysian_str_trim(elysian_t* server, char* str, char* ignore_prefix_chars, char* ignore_suffix_chars){
	uint16_t str_len;
	uint16_t i, k;
	uint16_t ignore_prefix_chars_len;
	uint16_t ignore_prefix_len;
	uint16_t ignore_suffix_chars_len;
	uint8_t ignore_char;
	
	str_len = strlen(str);
	
	/*
	** Trim prefix characters
	*/
	ignore_prefix_chars_len = strlen(ignore_prefix_chars);
	if(str_len && ignore_prefix_chars_len){
		ignore_prefix_len = 0;
		for(i = 0; i < str_len; i++){
			ignore_char = 0;
			for(k = 0; k < ignore_prefix_chars_len; k++){
				if(str[i] == ignore_prefix_chars[k]){
					ignore_char = 1;
					ignore_prefix_len++;
					break;
				}
			}
			if(!ignore_char){
				break;
			}
		}
		if(ignore_prefix_len){
			for(i = 0; i < str_len - ignore_prefix_len; i++){
				str[i] = str[ignore_prefix_len + i];
			}
			str_len -= ignore_prefix_len;
			str[str_len] = '\0';
		}
	}

	/*
	** Trim suffix characters
	*/
	ignore_suffix_chars_len = strlen(ignore_suffix_chars);
	if(str_len && ignore_suffix_chars_len){   
		for(k = 0; k < ignore_suffix_chars_len; k++){
			if(str[str_len - 1] == ignore_suffix_chars[k]){
				str[--str_len] = '\0';
				if(!str_len){
					break;
				}
			}
		}
	}
}

int elysian_strcmp(const char *str1, const char *str2, uint8_t match_case){
	register unsigned char *s1 = (unsigned char *) str1;
	register unsigned char *s2 = (unsigned char *) str2;
	unsigned char c1, c2;
	
	if ((!str1) || (!str2)) {
		return 1;
	}
	
	if(!match_case){
		do{
			c1 = (unsigned char) toupper((int)*s1++);
			c2 = (unsigned char) toupper((int)*s2++);
			if (c1 == '\0'){
				return c1 - c2;
			}
		}
		while (c1 == c2);
	}else{
		do{
			c1 = (unsigned char) (*s1++);
			c2 = (unsigned char) (*s2++);
			if (c1 == '\0'){
				return c1 - c2;
			}
		}
		while (c1 == c2);
	}
	return c1 - c2;
}

int elysian_strncmp(const char *str1, const char *str2, uint32_t n, uint8_t match_case){
	register unsigned char *s1 = (unsigned char *) str1;
	register unsigned char *s2 = (unsigned char *) str2;
	unsigned char c1, c2;
	uint32_t i;
	
	if ((!str1) || (!str2)) {
		return 1;
	}
	
	if(!match_case){
		for(i = 0; i < n; i++){
			c1 = (unsigned char) toupper((int)*s1++);
			c2 = (unsigned char) toupper((int)*s2++);
			if(c1 != c2){
				return 1;
			}
		}
	}else{
		for(i = 0; i < n; i++){
			c1 = (unsigned char) (*s1++);
			c2 = (unsigned char) (*s2++);
			if(c1 != c2){
				return 1;
			}
		}
	}
	return 0;
}

/**
 * @brief   Converts a DEC integer to HEX string
 * @return  0 on success, <0 on error
 */
int elysian_strhex2uint(char* hexstr, uint32_t* dec) {
	uint8_t i;

	*dec = 0;
	
	if (!hexstr) {
		return -1;
	}
	
	if ((hexstr[0] == '0') && ((hexstr[1] == 'x') || (hexstr[1] == 'X'))) { 
		hexstr += 2;
	}
	
	i = 0;
	while (*hexstr) {
		if(*hexstr > 47 && *hexstr < 58){ 
			/* 0 -> 9 */
			*dec += (*hexstr - 48);
		}else if(*hexstr > 64 && *hexstr < 71){ 
			/* A -> F */
			*dec += (*hexstr - 65 + 10);
		}else if(*hexstr > 96 && *hexstr < 103){
			/* a -> f */
			*dec += (*hexstr - 97 + 10);
		}else{
			break;
		}
		
		hexstr++;
		if((*hexstr > 47 && *hexstr < 58) || (*hexstr > 64 && *hexstr < 71) || (*hexstr > 96 && *hexstr < 103)){
			*dec <<= 4;
		}
		
		if(++i == 9) {
			/* Not a 32-bit integer */
			*dec = 0;
			return -1;
		}
	}
	
	if(!i) {
		/* No digits found */
		*dec = 0;
		return -1;
	}
	
	return 0;
}

elysian_err_t elysian_str2uint(char* buf, uint32_t* uint_var){
	uint32_t uint_len = 0;
	*uint_var = 0;
	
	if (!buf) {
		return ELYSIAN_ERR_FATAL;
	}
	
	while(buf[0] > 47 && buf[0] < 58){
		*uint_var += (buf[0] - 48);
		uint_len++;
		buf++;
		if(buf[0] > 47 && buf[0] < 58){
			*uint_var *= 10;
		}else{
			break;
		}
		if(uint_len == 11 /* strlen of 2^32 */){
			*uint_var = 0;
			return ELYSIAN_ERR_FATAL;
		}
	}
	if(!uint_len) {
		return ELYSIAN_ERR_FATAL;
	}else{
		return ELYSIAN_ERR_OK;
	}
}

elysian_err_t elysian_uint2str(uint32_t uint_var, char* buf, uint32_t buf_size){
	char const digit[] = "0123456789";
	uint32_t uint_len = 0;
	uint32_t uint_tmp = uint_var;
	if ((!buf) || (buf_size == 0)) {
		return ELYSIAN_ERR_FATAL;
	}
	do {
		uint_len++;
		uint_tmp = uint_tmp/10;
	} while(uint_tmp);
	if (uint_len + 1 > buf_size) {
		*buf = '\0';
		return ELYSIAN_ERR_FATAL;
	} else{
		buf[uint_len] = '\0';
	}
	do {
		buf[--uint_len] = digit[uint_var%10];
		uint_var = uint_var/10;
	} while(uint_var);
	ELYSIAN_ASSERT(uint_len == 0);
	return ELYSIAN_ERR_OK;
}

elysian_err_t elysian_int2str(int32_t int_var, char* buf, uint32_t buf_size){
	elysian_err_t err;
	
	if ((!buf) || (buf_size == 0)) {
		return ELYSIAN_ERR_FATAL;
	}
	
	if(int_var < 0){
		int_var = - int_var;
		err = elysian_uint2str((uint32_t) int_var, &buf[1], buf_size - 1);
		if(err != ELYSIAN_ERR_OK){
			buf[0] = '-';
		}
	}else{
		err = elysian_uint2str((uint32_t) int_var, &buf[0], buf_size);
	}
	return err;
}

int elysian_strcasecmp(char *a, char *b) {
	char c = -1;
	while(*a) {
		c = toupper(*a) - toupper(*b);
		if( c != 0 ) {
			return(c);
		}
		a++;
		b++;
	}
	return(c);
}

char* elysian_strcasestr(char *haystack, char *needle) {
	while (*haystack) {
		if (elysian_strcasecmp(needle, haystack) == 0) {
			return haystack;
		}
		haystack++;
	}
	return NULL;
}

char* elysian_strstr(char *haystack, char *needle) {
	return strstr(haystack, needle);
}

int elysian_snprintf(char* buf, uint32_t buf_size, const char* format, va_list valist){
	char* str_var;
	int int_var;
	unsigned int uint_var;
	uint32_t i = 0; 
	if(buf_size == 0){
		return 0;
	}
	while(format[0] && (i < buf_size - 1)){
		if(format[0] != '%'){
			buf[i++] = format[0];
			format += 1;
		}else if(format[1] == 's'){
			str_var = va_arg(valist, char *);
			while(str_var != NULL && *str_var && (i < buf_size - 1)){
				buf[i++] = *str_var++;
			}
			format += 2;
		}else if(format[1] == 'u'){
			uint_var = va_arg(valist, unsigned int);
			elysian_uint2str(uint_var, &buf[i], buf_size - 1 - i);
			i += strlen(&buf[i]);
			format += 2;
		}else if((format[1] == 'd') || (format[1] == 'i')){
			int_var = va_arg(valist, int);
			elysian_int2str(int_var, &buf[i], buf_size - 1 - i);
			i += strlen(&buf[i]);
			format += 2;
		}else{
			buf[i++] = format[0];
		}
	}
	buf[i] = '\0';
	return i;
}

elysian_err_t elysian_sprintf(char * buf, const char* format, ... ){
	va_list valist;
	va_start(valist, format);
	elysian_snprintf(buf, -1, format, valist);
	va_end(valist);
	return ELYSIAN_ERR_OK;
}
