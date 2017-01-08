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


#define elysian_stats_GRANUALITY_MS     (1000)
#define elysian_stats_BUCKETS           (10)

uint32_t history[elysian_stats_RES_NUM][elysian_stats_BUCKETS];
uint8_t samples;
uint32_t current_value = 0;
void elysian_stats_update(elysian_stats_res_t recource_id, uint32_t value){
    return;
    
    static uint32_t then = 0;
    uint32_t now;
    uint32_t bucket_index_now;
    uint32_t bucket_index_then;
    uint32_t bucket_index_shift;
    uint32_t index;
    elysian_stats_res_type_t recource_type;
    
    now = elysian_time_now();
    
    /*
    ** [... , ...)
    */
    bucket_index_now = now/elysian_stats_GRANUALITY_MS;
    bucket_index_then = then/elysian_stats_GRANUALITY_MS;
    bucket_index_shift = bucket_index_now - bucket_index_then;
    
    //ELYSIAN_LOG("}}}}}}}}} BUCKET SHIFT IS %u, current_value is %u, value is %u\r\n", bucket_index_shift, current_value, value);
    
    switch(recource_id){
        case elysian_stats_RES_MEM:
            recource_type = elysian_stats_RES_TYPE_MAXVALUE;
            break;
        case elysian_stats_RES_SLEEP:
            recource_type = elysian_stats_RES_TYPE_INTERVAL;
            break;
        default:
            ELYSIAN_ASSERT(0);
            break;
    };


    if(recource_type == elysian_stats_RES_TYPE_INTERVAL){
        if(bucket_index_shift >= elysian_stats_BUCKETS){
            for(index = 0; index < elysian_stats_BUCKETS; index++){
                history[recource_id][index] = 0;
            }
        }else{
            for(index = 0; index < elysian_stats_BUCKETS; index++){
                if(index < elysian_stats_BUCKETS - bucket_index_shift){
                    history[recource_id][index] = history[recource_id][index + bucket_index_shift];
                }else{
                    history[recource_id][index] = 0;
                }
            }
        }
        
        uint32_t stat_value;
        index = elysian_stats_BUCKETS - 1;
        while(1){
            stat_value = value % elysian_stats_GRANUALITY_MS;
            if(stat_value){
            }else{
                stat_value = value >= elysian_stats_GRANUALITY_MS ? elysian_stats_GRANUALITY_MS : value;
                
            }
            history[recource_id][index] += stat_value;
            value -= stat_value;
            if(!value){break;}
            if(!index){break;}
            index--;
        }
    }
    

    if(recource_type == elysian_stats_RES_TYPE_MAXVALUE){
        uint32_t fill_value = current_value;//history[recource_id][elysian_stats_BUCKETS - 1];
#if 0
        if(bucket_index_shift >= elysian_stats_BUCKETS){
            for(index = 0; index < elysian_stats_BUCKETS; index++){
                history[recource_id][index] = fill_value;
                samples = 0;
            }
        }else{
#endif
            //uint32_t fill_value = history[recource_id][elysian_stats_BUCKETS - 1];
            bucket_index_shift = (bucket_index_shift > elysian_stats_BUCKETS - 1) ? elysian_stats_BUCKETS - 1 : bucket_index_shift;
            for(index = 0; index < elysian_stats_BUCKETS; index++){
                if(index < elysian_stats_BUCKETS - bucket_index_shift){
                    history[recource_id][index] = history[recource_id][index + bucket_index_shift];
                }else{
                    history[recource_id][index] = fill_value;
                    samples = 0;
                }
            }
        //}
        if(value != -1){
            if(value > history[recource_id][elysian_stats_BUCKETS - 1]){
                history[recource_id][elysian_stats_BUCKETS - 1] = value;
            }
            
            samples++;
            current_value = value;
        }

        //ELYSIAN_LOG("}}}}}}}}} current_value is %u\r\n", current_value);
    }

    then = elysian_time_now();
}


void elysian_stats_get(elysian_stats_res_t recource_id){
    return;
    uint32_t index;
    elysian_stats_update(recource_id, -1);
    printf("------------------- [");
    for(index = 0; index < elysian_stats_BUCKETS; index++){
        printf("[%4u] ", history[recource_id][index]);
    }
    printf("]\r\n");
}
