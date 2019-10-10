#include "shell.h"
#include "cache_ins.h"
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
cache_region_t CACHE_REGIONS[256];
uint32_t fetch_mem(int set,int line,uint32_t address, int offset);
uint32_t read_insert_new(int set,uint32_t address,int offset);
int get_least_recent(int line);
void set_most_recent(int set, int dest);

#define CACHE_NREGIONS ((int)256)

void init_cache_ins() {                                           
    int i;
    for (i = 0; i < CACHE_NREGIONS; i++) {
        //CACHE_REGIONS[i].cache = malloc(32);
        memset(CACHE_REGIONS[i].cache, 0, 32);
        CACHE_REGIONS[i].tag = -1;
        CACHE_REGIONS[i].valid = 0;
        CACHE_REGIONS[i].shift = 0;
        CACHE_REGIONS[i].dirty = 0;
        CACHE_REGIONS[i].recent = 0;
    }
        printf("done init \n");
}


/***************************************************************/
/*                                                             */
/* Procedure: cache_ins_read_32                                */
/*                                                             */
/* Purpose: Read a 32-bit word from cache                      */
/*                                                             */
/***************************************************************/
uint32_t cache_ins_read_32(uint32_t address)
{   
   
    int set = 4*(( (uint32_t) (address >> 5) & 0x3F ));
    int tag = (int)(address >> 11); // count 21
    int offset = (int) address & 0x1F;
    int i;
    for(i = 0;i < NWAY_INST; i++){
        if (CACHE_REGIONS[set+i].tag == tag ){
            if(CACHE_REGIONS[set+i].shift == 0){
                if(offset < 0x1D) {
                    if(CACHE_REGIONS[set+i].valid == 1){
                        set_most_recent(set,i);
                        return
                            (CACHE_REGIONS[set+i].cache[offset+3] << 24) |
                            (CACHE_REGIONS[set+i].cache[offset+2] << 16) |
                            (CACHE_REGIONS[set+i].cache[offset+1] <<  8) |
                            (CACHE_REGIONS[set+i].cache[offset+0] <<  0);
                    }
                    else{
                        return fetch_mem(set,i,address, offset); // shift(1) or not(0)
                    }

                }
                else{
                    return read_insert_new(set,address,offset);
                }
            }
            if(CACHE_REGIONS[set+i].shift == 1){
                if(offset > 0x3) {
                    if(CACHE_REGIONS[set+i].valid == 1){
                        set_most_recent(set,i);
                        return
                            (CACHE_REGIONS[set+i].cache[offset+3] << 24) |
                            (CACHE_REGIONS[set+i].cache[offset+2] << 16) |
                            (CACHE_REGIONS[set+i].cache[offset+1] <<  8) |
                            (CACHE_REGIONS[set+i].cache[offset+0] <<  0);
                    }
                    else{
                        return fetch_mem(set,i,address, offset); // shift(1) or not(0)
                    }

                }
                else{
                   return read_insert_new(set,address,offset);
                }
            }
                
        }
    }
    //TODO CACHE miss due to tag missmatch
    return read_insert_new(set,address,offset);
}

uint32_t read_insert_new(int set,uint32_t address,int offset){
    #ifdef DEBUG
        printf("start read_insert_new \n");
    #endif
    int valid_line = -1;
    int i;
    for(i = 0;i < NWAY_INST; i++){
        if(CACHE_REGIONS[set+i].valid == 0 ){
            valid_line = i;
            break;
        }
    }
    if(valid_line >= 0){
        return fetch_mem(set,valid_line,address,offset);
        
    }else{
        i = get_least_recent(set);
        return fetch_mem(set,i,address,offset);
    }

}

uint32_t fetch_mem(int set,int line,uint32_t address, int offset){
    #ifdef DEBUG
       // printf("memory STALL %d\n",STALL);
    #endif
    int dest = set+line;
    int tag = (int)(address >> 11); // count 21 
    uint32_t prefix = address & 0xFFFFFFEF;
    CACHE_REGIONS[dest].tag = tag;
    uint32_t mem = mem_read_32(address);
    uint32_t value;
    if(offset < 0x1D){
        CACHE_REGIONS[dest].shift = 0;
        int i;
        for( i = 0; i < 32; i=i+4){ 
            uint32_t temp = mem_read_32(prefix + i);
            printf("fetch %08x %08x \n",prefix+i, temp);
            CACHE_REGIONS[dest].cache[i+3] = (temp >> 24) & 0xFF;
            CACHE_REGIONS[dest].cache[i+2] = (temp >> 16) & 0xFF;
            CACHE_REGIONS[dest].cache[i+1] = (temp >> 8) & 0xFF;
            CACHE_REGIONS[dest].cache[i+0] = (temp >> 0) & 0xFF;
        }
    }
    else{
        CACHE_REGIONS[dest].shift = 1;
        int i;
        for(i = 0; i < 28; i=i+4){ 
            uint32_t temp = mem_read_32(prefix + (i+4));
            printf("fetch %08x %08x \n",prefix+i, temp);
            CACHE_REGIONS[dest].cache[i+3] = (temp >> 24) & 0xFF;
            CACHE_REGIONS[dest].cache[i+2] = (temp >> 16) & 0xFF;
            CACHE_REGIONS[dest].cache[i+1] = (temp >> 8) & 0xFF;
            CACHE_REGIONS[dest].cache[i+0] = (temp >> 0) & 0xFF;
        }
        uint32_t temp = mem_read_32(prefix + 0x20);
        i = 28;
        printf("fetch %08x %08x \n",prefix+i, temp);
        CACHE_REGIONS[dest].cache[i+3] = (temp >> 24) & 0xFF;
        CACHE_REGIONS[dest].cache[i+2] = (temp >> 16) & 0xFF;
        CACHE_REGIONS[dest].cache[i+1] = (temp >> 8) & 0xFF;
        CACHE_REGIONS[dest].cache[i+0] = (temp >> 0) & 0xFF;
    }
    CACHE_REGIONS[dest].valid = 1;
    CACHE_REGIONS[dest].dirty = 0;
    set_most_recent(set,line);
    return mem;
        // (CACHE_REGIONS[dest].cache[offset+3] << 24) |
        // (CACHE_REGIONS[dest].cache[offset+2] << 16) |
        // (CACHE_REGIONS[dest].cache[offset+1] <<  8) |
        // (CACHE_REGIONS[dest].cache[offset+0] <<  0);
}

int get_least_recent(int line){
    int i;
    for( i=0; i < NWAY_INST; i++){
        if (CACHE_REGIONS[line+i].recent == 0){
            return i;
        }
    }
}

void set_most_recent(int set, int dest){
    if(dest > NWAY_INST - 1){
        return;
    }
    int i;
    for(i=0; i < NWAY_INST; i++){
        if(CACHE_REGIONS[set+i].recent > CACHE_REGIONS[set+dest].recent)
            CACHE_REGIONS[set+i].recent--;
    }
    CACHE_REGIONS[set+dest].recent = NWAY_INST - 1;
}

/***************************************************************/
/*                                                             */
/* Procedure: cache_write_32                                   */
/*                                                             */
/* Purpose: Write a 32-bit word to cache                       */
/*                                                             */
/***************************************************************/
// void cache_write_32(uint32_t address, uint32_t value)
// {
//     int set =NWAY_INST*((int)((uint32_t) address << 21) >> 5);
//     int tag = (int)(address >> 11); // count 21 
//     int offset = (int) address << 27;
//     for(int i = 0;i < NWAY_INST; i++){
//         if (CACHE_REGIONS[set+i].tag == tag ){
//             if(CACHE_REGIONS[set+i].shift == 0){
//                 if(offset < 0x1B) {
//                     //the same block, write directly
//                     write_over();
//                 }
//                 else{
//                     write_insert_new(set,address,offset,)
//                 }
                   
//             }
//             if(CACHE_REGIONS[set+i].shift == 1){
//                 if(offset > 0x3) {
//                     // the same block
//                     write_over()
//                 }
//                 else{
//                    write_insert_new(set,address,offset);
//                 }
//             }
                
//         }
//     }
//     //TODO CACHE miss due to tag missmatch
//     read_insert_new(set,address,offset);
// }

