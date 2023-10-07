#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

  
/*create cache, input number of entries. If success, return 1, else return -1. If number of entries is too big or create cache twice without destroy, function fail*/
int cache_create(int num_entries) {
  if (cache_enabled() ){
    return -1;
    }
  if (num_entries > 4096 || num_entries < 2){
    return -1;
    }
  cache_size = num_entries;
  cache = calloc(cache_size,sizeof(cache_entry_t));
  return 1;
}
/*destroy cache, free memory. If success, return 1, else return -1. If destroy cache twice without create, function fail.*/
int cache_destroy(void) {
  if (cache_enabled()==false){
    return -1;
    }
  cache_size = 0;
  clock = 0;
  free(cache);
  return 1;
}
/*search block in cache. Input disk number, block number and a pointer buf to point target block content. if success, return 1, else return -1. use null pointer will lead to fail. */
/*Once the search starts, the function will increment queries once, and increase hits then update clock if find the target block.*/
int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  //fail check
  if (cache_enabled() == false || buf == NULL){
    return -1;
    }
  //increase queries
  num_queries += 1;
  //loop to search target
  for (int i = 0; i < cache_size; i++){
    if (cache[i].valid ){
      if (cache[i].disk_num == disk_num && cache[i].block_num == block_num){
        //copy the target block
        memcpy(buf,&cache[i].block,256);
        //increase hits
        num_hits += 1;
        //update clock
        cache[i].access_time = clock;
        clock += 1;
        return 1;
        }
      }
    }
  return -1;
}

/*update cache of target block. Input disk number and block number, and a pointer buf that point to a array that want to updat at target block*/
/*This function will update clock if success, do nothing if fail*/
void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  if (cache_enabled() == false ){
    return;
    }
  // loop to search target
  for (int i = 0; i < cache_size; i++){
    if (cache[i].valid){
      if (cache[i].disk_num == disk_num && cache[i].block_num == block_num){
        // update block content in cache
        memcpy(&cache[i].block,buf,256);
        // update clock
        cache[i].access_time = clock;
        clock += 1;
        return ;
        }
      }
    }
  return ;
}

  
/*insert a new cache. input disk number and block number, and a pointer buf that point to the content that want to insert.*/
/*if success, return 1, else return -1.if pointer is null or disk/block number is out of range, function fail.*/
/*when the target block exist, update the block. If block is the same, function fail. if cache is full, use LRU policy to insert cache.*/
/*this function will update clock if success*/
int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  //fail check
  if (cache_enabled() == false){
    return -1;
    }
  if (buf == NULL){    
    return -1;
    }
  if (disk_num < 0 || disk_num > 15 || block_num < 0 || block_num > 255 ){
    return -1;
    }
  // check if cache is full
  int Valid = 0;
  // loop to check valid entries
  for (int i = 0; i < cache_size; i++){
    if (cache[i].valid){
      Valid += 1;
      if (cache[i].disk_num == disk_num && cache[i].block_num == block_num){
        //check fail
        if (memcmp(&cache[i].block, buf, 256) == 0){
          return -1;
          }
        // update block
        memcpy(&cache[i].block,buf, 256);
        //update clock
        cache[i].access_time = clock;
        clock += 1;
        return 1;
        }
      }
    }
  //when arraylist is full, LRU
  if (Valid == cache_size){
    int mintime = cache[0].access_time;
    // loop to find the minimal clock
    for (int i = 0; i < cache_size; i++){
      if (mintime > cache[i].access_time){
        mintime = cache[i].access_time;
      }
    }
    // insert the new entry
    for (int i = 0; i < cache_size; i++){
      if ( cache[i].access_time == mintime){
        cache[i].disk_num = disk_num;
        cache[i].block_num = block_num;
        memcpy(&cache[i].block,buf,256);
        cache[i].access_time = clock;
        clock += 1;
        } 
      }
    }
  //when arraylist is not full, add it
  else{
    cache[Valid].valid = true;
    cache[Valid].disk_num = disk_num;
    cache[Valid].block_num = block_num;
    memcpy(&cache[Valid].block,buf,256);
    cache[Valid].access_time = clock;
    clock += 1;
    }
  return 1;
}
/*this finction is to check if the cache is created.*/
bool cache_enabled(void) {
  if (cache_size > 2){
    return true;
    }
  return false;
}


void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
