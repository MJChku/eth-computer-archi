#include "shell.h"
#include "cache_data.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#define NWAY_INST ((int)4)
#define NWAY_DATA ((int)8)
typedef struct {
    uint32_t tag;
    bool valid;
    bool dirty;
    bool shift;
    int recent;
    uint8_t cache[32];
} cache_region_t;

/* cacheory will be dynamically allocated at initialization */
cache_region_t CACHE_DATA_REGIONS[2048];

uint32_t read_insert_new_data(int set,uint32_t address,int offset);
uint32_t write_insert_new(int set,uint32_t address,int offset, uint32_t value);
uint32_t fetch_mem_data(int set,int line,uint32_t address, int offset);
uint32_t write_mem(int set,int line,uint32_t address, int offset, uint32_t value);
//void cache_write_32(uint32_t address, uint32_t value);
int get_least_recent_data(int line);
void set_most_recent_data(int set, int dest);

#define CACHE_NREGIONS ((int)(2048))
void init_cache_data() {                                           
    int i;
    for (i = 0; i < CACHE_NREGIONS; i++) {
        //CACHE_DATA_REGIONS[i].cache = malloc(32);
        memset(CACHE_DATA_REGIONS[i].cache, 0, 32);
        CACHE_DATA_REGIONS[i].tag = -1;
        CACHE_DATA_REGIONS[i].valid = 0;
        CACHE_DATA_REGIONS[i].shift = 0;
        CACHE_DATA_REGIONS[i].dirty = 0;
        CACHE_DATA_REGIONS[i].recent = 0;
    }
}


/***************************************************************/
/*                                                             */
/* Procedure: cache_ins_data_32                                */
/*                                                             */
/* Purpose: Read a 32-bit word from cache                      */
/*                                                             */
/***************************************************************/
uint32_t cache_data_read_32(uint32_t address)
{   
   
    int set = NWAY_DATA*(( (uint32_t) (address >> 5) & 0xFF ));
    int tag = (int)(address >> 13); // count 21
    int offset = (int) address & 0x1F;
    int i;
    
    for(i = 0;i < NWAY_DATA; i++){
        if (CACHE_DATA_REGIONS[set+i].tag == tag ){
            if(CACHE_DATA_REGIONS[set+i].shift == 0){
                if(offset < 0x1D) {
                    if(CACHE_DATA_REGIONS[set+i].valid == 1){
                        return
                            (CACHE_DATA_REGIONS[set+i].cache[offset+3] << 24) |
                            (CACHE_DATA_REGIONS[set+i].cache[offset+2] << 16) |
                            (CACHE_DATA_REGIONS[set+i].cache[offset+1] <<  8) |
                            (CACHE_DATA_REGIONS[set+i].cache[offset+0] <<  0);
                    }
                    else{
                        return fetch_mem_data(set,i,address, offset); // shift(1) or not(0)
                    }

                }
                else{
                    return read_insert_new_data(set,address,offset);
                }
            }
            if(CACHE_DATA_REGIONS[set+i].shift == 1){
                if(offset > 0x3) {
                    if(CACHE_DATA_REGIONS[set+i].valid == 1){
                        return
                            (CACHE_DATA_REGIONS[set+i].cache[offset+3] << 24) |
                            (CACHE_DATA_REGIONS[set+i].cache[offset+2] << 16) |
                            (CACHE_DATA_REGIONS[set+i].cache[offset+1] <<  8) |
                            (CACHE_DATA_REGIONS[set+i].cache[offset+0] <<  0);
                    }
                    else{
                        return fetch_mem_data(set,i,address, offset); // shift(1) or not(0)
                    }

                }
                else{
                   return read_insert_new_data(set,address,offset);
                }
            }
                
        }
    }
    //TODO CACHE miss due to tag missmatch
    return read_insert_new_data(set,address,offset);
}

uint32_t read_insert_new_data(int set,uint32_t address,int offset){
    #ifdef DEBUG
        printf("start read_insert_new_data \n");
    #endif
    int valid_line = -1;
    int i;
    for(i = 0;i < NWAY_DATA; i++){
        if(CACHE_DATA_REGIONS[set+i].valid == 0 ){
            valid_line = i;
            break;
        }
    }
    if(valid_line >= 0){
        return fetch_mem_data(set,valid_line,address,offset);
        
    }else{
        i = get_least_recent_data(set);
        return fetch_mem_data(set,i,address,offset);
    }

}

uint32_t fetch_mem_data(int set,int line,uint32_t address, int offset){
    // STALL ++;
    // printf("stall %d\n", STALL);
    // #ifdef DEBUG
    //     printf("memory STALL %d\n",STALL);
    // #endif
    int dest = set+line;

    if(CACHE_DATA_REGIONS[dest].valid = 1 && CACHE_DATA_REGIONS[dest].dirty == 1 ){
        uint32_t prefix = (CACHE_DATA_REGIONS[dest].tag << 13) | (set << 5);
        if(CACHE_DATA_REGIONS[dest].shift == 0){
            uint32_t temp_add, temp_value;
            int i;
            for( i = 0; i < 32; i=i+4){ 
                temp_add = prefix + i; 
                temp_value = 
                (CACHE_DATA_REGIONS[dest].cache[i+3] <<24) |
                (CACHE_DATA_REGIONS[dest].cache[i+2] <<16) |
                (CACHE_DATA_REGIONS[dest].cache[i+1] << 8) |
                (CACHE_DATA_REGIONS[dest].cache[i+0] << 0) ;
                mem_write_32(temp_add,temp_value);
            }
        }else{
            uint32_t temp_add, temp_value;
            int i;
            for( i = 0; i < 28; i=i+4){ 
                temp_add = prefix + i + 4; 
                temp_value = 
                (CACHE_DATA_REGIONS[dest].cache[i+3] <<24) |
                (CACHE_DATA_REGIONS[dest].cache[i+2] <<16) |
                (CACHE_DATA_REGIONS[dest].cache[i+1] << 8) |
                (CACHE_DATA_REGIONS[dest].cache[i+0] << 0) ;
                mem_write_32(temp_add,temp_value);
            }
            i = 28;
            temp_add = prefix + 0x20; 
            temp_value = 
            (CACHE_DATA_REGIONS[dest].cache[i+3] <<24) |
            (CACHE_DATA_REGIONS[dest].cache[i+2] <<16) |
            (CACHE_DATA_REGIONS[dest].cache[i+1] << 8) |
            (CACHE_DATA_REGIONS[dest].cache[i+0] << 0) ;
            mem_write_32(temp_add,temp_value);

        }
        CACHE_DATA_REGIONS[dest].valid = 0;
    }
    


    int tag = (int)(address >> 13); // count 21 
    uint32_t prefix = address & 0xFFFFFFEF;
    CACHE_DATA_REGIONS[dest].tag = tag;
    uint32_t mem = mem_read_32(address);
    uint32_t value;
    if(offset < 0x1D){
        CACHE_DATA_REGIONS[dest].shift = 0;
        int i;
        for( i = 0; i < 32; i=i+4){ 
            uint32_t temp = mem_read_32(prefix + i);
            CACHE_DATA_REGIONS[dest].cache[i+3] = (temp >> 24) & 0xFF;
            CACHE_DATA_REGIONS[dest].cache[i+2] = (temp >> 16) & 0xFF;
            CACHE_DATA_REGIONS[dest].cache[i+1] = (temp >> 8) & 0xFF;
            CACHE_DATA_REGIONS[dest].cache[i+0] = (temp >> 0) & 0xFF;
        }
    }else{
        CACHE_DATA_REGIONS[dest].shift = 1;
        int i;
        for(i = 0; i < 28; i=i+4){ 
            uint32_t temp = mem_read_32(prefix + (i+4));
            CACHE_DATA_REGIONS[dest].cache[i+3] = (temp >> 24) & 0xFF;
            CACHE_DATA_REGIONS[dest].cache[i+2] = (temp >> 16) & 0xFF;
            CACHE_DATA_REGIONS[dest].cache[i+1] = (temp >> 8) & 0xFF;
            CACHE_DATA_REGIONS[dest].cache[i+0] = (temp >> 0) & 0xFF;
        }
        uint32_t temp = mem_read_32(prefix + 0x20);
        i = 28;
        CACHE_DATA_REGIONS[dest].cache[i+3] = (temp >> 24) & 0xFF;
        CACHE_DATA_REGIONS[dest].cache[i+2] = (temp >> 16) & 0xFF;
        CACHE_DATA_REGIONS[dest].cache[i+1] = (temp >> 8) & 0xFF;
        CACHE_DATA_REGIONS[dest].cache[i+0] = (temp >> 0) & 0xFF;
    }
    CACHE_DATA_REGIONS[dest].valid = 1;
    CACHE_DATA_REGIONS[dest].dirty = 0;
    set_most_recent(set,line);
    return mem;
        // (CACHE_DATA_REGIONS[dest].cache[offset+3] << 24) |
        // (CACHE_DATA_REGIONS[dest].cache[offset+2] << 16) |
        // (CACHE_DATA_REGIONS[dest].cache[offset+1] <<  8) |
        // (CACHE_DATA_REGIONS[dest].cache[offset+0] <<  0);
}

int get_least_recent_data(int line){
    int i;
    for( i=0; i < NWAY_DATA; i++){
        if (CACHE_DATA_REGIONS[line+i].recent == 0){
            return i;
        }
    }
}

void set_most_recent_data(int set, int dest){
    if(dest > NWAY_DATA - 1){
        return;
    }
    int i;
    for(i=0; i < NWAY_DATA; i++){
        if(CACHE_DATA_REGIONS[set+i].recent > CACHE_DATA_REGIONS[set+dest].recent)
            CACHE_DATA_REGIONS[set+i].recent--;
    }
    CACHE_DATA_REGIONS[set+dest].recent = NWAY_DATA - 1;
}

/***************************************************************/
/*                                                             */
/* Procedure: cache_write_32                                   */
/*                                                             */
/* Purpose: Write a 32-bit word to cache                       */
/*                                                             */
/***************************************************************/
void cache_write_32(uint32_t address, uint32_t value)
{
    int set = NWAY_DATA*(( (uint32_t) (address >> 5) & 0xFF ));
    int tag = (int)(address >> 13); // count 21
    int offset = (int) (address & 0x1F);
    int i;
    for( i = 0;i < NWAY_DATA; i++){
        if (CACHE_DATA_REGIONS[set+i].tag == tag ){
            if(CACHE_DATA_REGIONS[set+i].shift == 0){
                if(offset < 0x1D) {
                    printf("write \n");
                    //the same block, write directly
                    CACHE_DATA_REGIONS[set+i].cache[offset+3] = (value >> 24) & 0xFF;
                    CACHE_DATA_REGIONS[set+i].cache[offset+2] = (value >> 16) & 0xFF;
                    CACHE_DATA_REGIONS[set+i].cache[offset+1] = (value >> 8) & 0xFF;
                    CACHE_DATA_REGIONS[set+i].cache[offset+0] = (value >> 0) & 0xFF;
                    CACHE_DATA_REGIONS[set+i].dirty = 1;
                    CACHE_DATA_REGIONS[set+i].valid = 1;
                    set_most_recent(set,i);
                    return;
                }
                else{
                    write_insert_new(set,address,offset,value);
                    return;
                }
                   
            }
            if(CACHE_DATA_REGIONS[set+i].shift == 1){
                if(offset > 0x3) {
                    // the same block
                    CACHE_DATA_REGIONS[set+i].cache[offset+3] = (value >> 24) & 0xFF;
                    CACHE_DATA_REGIONS[set+i].cache[offset+2] = (value >> 16) & 0xFF;
                    CACHE_DATA_REGIONS[set+i].cache[offset+1] = (value >> 8) & 0xFF;
                    CACHE_DATA_REGIONS[set+i].cache[offset+0] = (value >> 0) & 0xFF;
                    CACHE_DATA_REGIONS[set+i].dirty = 1;
                    CACHE_DATA_REGIONS[set+i].valid = 1;
                    set_most_recent(set,i);
                    return;
                }
                else{
                   write_insert_new(set,address,offset,value);
                   return;
                }
            }
                
        }
    }
    //TODO CACHE miss due to tag missmatch
   write_insert_new(set,address,offset,value);
}


uint32_t write_insert_new(int set,uint32_t address,int offset, uint32_t value){
    #ifdef DEBUG
        printf("start read_insert_new_data \n");
    #endif
    int valid_line = -1;
    int i;
    for(i = 0;i < NWAY_DATA; i++){
        if(CACHE_DATA_REGIONS[set+i].valid == 0 ){
            valid_line = i;
            break;
        }
    }
    if(valid_line >= 0){
        return write_mem(set,valid_line,address,offset,value);
        
    }else{
        i = get_least_recent(set);
        write_mem(set,i,address,offset,value);
    }

}

uint32_t write_mem(int set,int line,uint32_t address, int offset, uint32_t value){
    // STALL++; 
    // printf("stall %d\n", STALL);
    // #ifdef DEBUG
    //     printf("memory STALL %d\n",STALL);
    // #endif
    
    int dest = set+line;

    if(CACHE_DATA_REGIONS[dest].valid = 1 && CACHE_DATA_REGIONS[dest].dirty == 1 ){
        uint32_t prefix = (CACHE_DATA_REGIONS[dest].tag << 13) | (set << 5);
        if(CACHE_DATA_REGIONS[dest].shift == 0){
            uint32_t temp_add, temp_value;
            int i;
            for( i = 0; i < 32; i=i+4){ 
                temp_add = prefix + i; 
                temp_value = 
                (CACHE_DATA_REGIONS[dest].cache[i+3] <<24) |
                (CACHE_DATA_REGIONS[dest].cache[i+2] <<16) |
                (CACHE_DATA_REGIONS[dest].cache[i+1] << 8) |
                (CACHE_DATA_REGIONS[dest].cache[i+0] << 0) ;
                mem_write_32(temp_add,temp_value);
            }
        }else{
            uint32_t temp_add, temp_value;
            int i;
            for( i = 0; i < 28; i=i+4){ 
                temp_add = prefix + i + 4; 
                temp_value = 
                (CACHE_DATA_REGIONS[dest].cache[i+3] <<24) |
                (CACHE_DATA_REGIONS[dest].cache[i+2] <<16) |
                (CACHE_DATA_REGIONS[dest].cache[i+1] << 8) |
                (CACHE_DATA_REGIONS[dest].cache[i+0] << 0) ;
                mem_write_32(temp_add,temp_value);
            }
            i = 28;
            temp_add = prefix + 0x20; 
            temp_value = 
            (CACHE_DATA_REGIONS[dest].cache[i+3] <<24) |
            (CACHE_DATA_REGIONS[dest].cache[i+2] <<16) |
            (CACHE_DATA_REGIONS[dest].cache[i+1] << 8) |
            (CACHE_DATA_REGIONS[dest].cache[i+0] << 0) ;
            mem_write_32(temp_add,temp_value);

        }
          CACHE_DATA_REGIONS[dest].valid = 0;
    }

    int tag = (int)(address >> 13); // count 21 
    uint32_t prefix = address & 0xFFFFFFEF;
    CACHE_DATA_REGIONS[dest].tag = tag;
    if(offset < 0x1D){
        CACHE_DATA_REGIONS[dest].shift = 0;
        int i;
        for( i = 0; i < 32; i=i+4){ 
            uint32_t temp = mem_read_32(prefix + i);
            CACHE_DATA_REGIONS[dest].cache[i+3] = (temp >> 24) & 0xFF;
            CACHE_DATA_REGIONS[dest].cache[i+2] = (temp >> 16) & 0xFF;
            CACHE_DATA_REGIONS[dest].cache[i+1] = (temp >> 8) & 0xFF;
            CACHE_DATA_REGIONS[dest].cache[i+0] = (temp >> 0) & 0xFF;
        }
    }else{
        CACHE_DATA_REGIONS[dest].shift = 1;
        int i;
        for(i = 0; i < 28; i=i+4){ 
            uint32_t temp = mem_read_32(prefix + (i+4));
            CACHE_DATA_REGIONS[dest].cache[i+3] = (temp >> 24) & 0xFF;
            CACHE_DATA_REGIONS[dest].cache[i+2] = (temp >> 16) & 0xFF;
            CACHE_DATA_REGIONS[dest].cache[i+1] = (temp >> 8) & 0xFF;
            CACHE_DATA_REGIONS[dest].cache[i+0] = (temp >> 0) & 0xFF;
        }
        uint32_t temp = mem_read_32(prefix + 0x20);
        i = 28;
        CACHE_DATA_REGIONS[dest].cache[i+3] = (temp >> 24) & 0xFF;
        CACHE_DATA_REGIONS[dest].cache[i+2] = (temp >> 16) & 0xFF;
        CACHE_DATA_REGIONS[dest].cache[i+1] = (temp >> 8) & 0xFF;
        CACHE_DATA_REGIONS[dest].cache[i+0] = (temp >> 0) & 0xFF;
    }
    CACHE_DATA_REGIONS[dest].valid = 1;
    CACHE_DATA_REGIONS[dest].dirty = 1;
    set_most_recent(set,line);
    CACHE_DATA_REGIONS[dest].cache[offset+3] = (value >> 24) & 0xFF;
    CACHE_DATA_REGIONS[dest].cache[offset+2] = (value >> 16) & 0xFF;
    CACHE_DATA_REGIONS[dest].cache[offset+1] = (value >> 8) & 0xFF;
    CACHE_DATA_REGIONS[dest].cache[offset+0] = (value >> 0) & 0xFF;
}


