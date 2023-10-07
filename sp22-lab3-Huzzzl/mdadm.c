#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"

int mo_counter = 0;

uint32_t combine(int command,int disk,int block){ //this function is to encode the command,disk and block to build operation
  return command*67108864 + disk * 4194304 + block; // 4^13 , 4^11
  }

int mdadm_mount(void) {  //Use counter to check the mount status
  if (mo_counter == 0){
    mo_counter = 1;
    jbod_operation(combine(JBOD_MOUNT,0,0),NULL);
    return 1;
    }
  return -1;
  }

int mdadm_unmount(void) {
  if (mo_counter == 1){
    mo_counter = 0;
    jbod_operation(combine(JBOD_UNMOUNT,0,0),NULL);
    return 1;
    }
  return -1;
  }


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

  int address = addr;
  int end = addr + len;
  while (address < end){
    int disk = address / 65536;
    int block = address % 65536 / 256;
    int off = address % 256;
    uint8_t temp[256];
    //seek disk, then block
    jbod_operation(combine(JBOD_SEEK_TO_DISK,disk,0),NULL);
    jbod_operation(combine(JBOD_SEEK_TO_BLOCK,0,block),NULL);
    //read data and put into temp
    jbod_operation(combine(JBOD_READ_BLOCK,0,0),temp);
    //process data, put selected data from temp to buf
    if(end -address > 256){
      memcpy(buf+address-addr,temp,256-off);
      }
    else{
      memcpy(buf+address-addr,temp,end -address);
      }
    address = address + 256 - off;
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
  while (address < end){
    int disk = address / 65536;
    int block = address % 65536 / 256;
    int off = address % 256;
    uint8_t temp[256];
    //seek disk, then block
    jbod_operation(combine(JBOD_SEEK_TO_DISK,disk,0),NULL);
    jbod_operation(combine(JBOD_SEEK_TO_BLOCK,0,block),NULL);
    //process data, put selected data from buff to temp
    if(end -address > 256){
      memcpy(temp+off,buf+address-addr,256-off);
      }
    else{
      memcpy(temp+off,buf+address-addr,end -address);
      }
    address = address + 256 - off;
    //write data of temp in current position
    jbod_operation(combine(JBOD_WRITE_BLOCK,0,0),temp);
    }
  return len;
}
