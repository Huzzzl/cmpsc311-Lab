#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"

int mo_counter = 0;
/* this function is to encode the command,disk and block to build operation. */
uint32_t combine(int command,int disk,int block){
  return command*67108864 + disk * 4194304 + block; // 4^13 , 4^11
  }

/* Use counter to check the mount status, when counter is 1, status is mount. Return -1 if fall. Mount twice without unmount lead to fail. */
int mdadm_mount(void) {  
  if (mo_counter == 0){
    mo_counter = 1;
    jbod_operation(combine(JBOD_MOUNT,0,0),NULL);
    return 1;
    }
  return -1;
  }

/* Use counter to check the mount status, when counter is 0, status is unmount. Return -1 if fall. Unmount twice without mount lead to fail. */
int mdadm_unmount(void) { 
  if (mo_counter == 1){
    mo_counter = 0;
    jbod_operation(combine(JBOD_UNMOUNT,0,0),NULL);
    return 1;
    }
  return -1;
  }
/* Read the data. Input target address, target length, and an array pointer buf to point to the target content. */
/* Return -1 if fail, return len if success. If unmont, address or length are out of range, or length exists but buff is null, function fail.*/
int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  // Check for unreasonable data
  if (mo_counter == 0){
    return -1;
    }
  if (addr+len > 1048576){
    return -1;
    }
  if (len>1024){
    return -1;
    }
  if (buf == NULL && len>0){
    return -1;
    }
  // set basic information
  int address = addr;
  int end = addr + len;
  int disk = address / 65536;
  int block = address % 65536 / 256;
  uint8_t out[256];
  // move I/O to current position
  jbod_operation(combine(JBOD_SEEK_TO_DISK,disk,0),NULL);
  jbod_operation(combine(JBOD_SEEK_TO_BLOCK,0,block),NULL);
  // loop to update address(current address) and block(current block).
  while (address < end){
    int off = address % 256;
    uint8_t temp[256];
    // check if read cross disk, if true then update disk
    if (block == 256){
      block = 0;
      disk += 1;
      jbod_operation(combine(JBOD_SEEK_TO_DISK,disk,0),NULL);
      jbod_operation(combine(JBOD_SEEK_TO_BLOCK,0,block),NULL);
      }
    // check cache with key info
    if (cache_enabled() && cache_lookup(disk,block,out)==1){
      if(end -address > 256){
        // When content crosses block, copy current block
        memcpy(buf+address-addr,out+off,256-off);
        }
      else{
        // When content within block, copy current block then return
        memcpy(buf+address-addr,out+off,end -address);
        return len;
        }
      // update block and address, move to the first bit of next block.
      block += 1;
      address = address + 256-off;
      // if move to next disk, move I/O
      if (block == 256){
        block = 0;
        disk += 1;
        jbod_operation(combine(JBOD_SEEK_TO_DISK,disk,0),NULL);
        jbod_operation(combine(JBOD_SEEK_TO_BLOCK,0,block),NULL);
        }
      else{
      // if within the block, move I/O
        jbod_operation(combine(JBOD_SEEK_TO_BLOCK,0,block),NULL);
        }  
      continue;
      }
    // not in cache
    //read data and put into temp
    jbod_operation(combine(JBOD_READ_BLOCK,0,0),temp);
    //process data, copy data from temp to buf
    if(end -address > 256){
      // When content crosses block, copy current block
      memcpy(buf+address-addr,temp+off,256-off);
      }
    else{
      // When content within block, copy current block
      memcpy(buf+address-addr,temp+off,end -address);
      }
    // if cache exists, insert data not in cache  
    if (cache_enabled()){
      cache_insert(disk,block,temp);
      }
    block += 1;
    address = address + 256-off;
    }
  return len;
  }

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  // Check for unreasonable data
  if (mo_counter == 0){
    return -1;
    }
  if (addr+len > 1048576){
    return -1;
    }
  if (len>1024){
    return -1;
    }
  if (buf == NULL && len>0){
    return -1;
    }
  int address = addr;
  int end = addr + len;
  int disk = address / 65536;
  int block = address % 65536 / 256;
  jbod_operation(combine(JBOD_SEEK_TO_DISK,disk,0),NULL);
  jbod_operation(combine(JBOD_SEEK_TO_BLOCK,0,block),NULL);
  while (address < end){
    int off = address % 256;
    uint8_t temp[500];
    if (block == 256){
      block = 0;
      disk += 1;
      jbod_operation(combine(JBOD_SEEK_TO_DISK,disk,0),NULL);
      jbod_operation(combine(JBOD_SEEK_TO_BLOCK,0,block),NULL);
      }
    //check cache
    if (cache_enabled() && cache_lookup(disk,block,temp)==1){      
      if (end -address > 256){
        memcpy(temp + off,buf+address-addr,256-off);
        cache_update(disk,block,temp);
        jbod_operation(combine(JBOD_WRITE_BLOCK,0,0),temp);
        } 
      else{
        memcpy(temp+off,buf+address-addr,end-address);
        cache_update(disk,block,temp);
        jbod_operation(combine(JBOD_WRITE_BLOCK,0,0),temp);
        return len;
        }
      block += 1;
      address = address + 256 - off;
      continue;
      }
    //not in cache
    mdadm_read(address-off,256,temp);
    jbod_operation(combine(JBOD_SEEK_TO_BLOCK,0,block),NULL);
    //process data, put selected data from buff to temp
    if(end -address > 256){
      memcpy(temp+off,buf+address-addr,256-off);
      }
    else{
      memcpy(temp+off,buf+address-addr,end-address);
      }
    address = address + 256 - off;
    //write data of temp in current position
    jbod_operation(combine(JBOD_WRITE_BLOCK,0,0),temp);
    if (cache_enabled()){
      cache_insert(disk,block,temp);
      }
    block += 1;
    }
  return len;
}
