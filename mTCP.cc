/*******************************************************
MTCP: mTCPclient mTCPserver
Date: Dec. 1996

Purpose: For high-performance networks, the TCP windowsize
 must be manipulated

Based on Work by Brian Toonen (ptTCP pthreaded TCP) from
Argonne National Lab.  This rev. eliminates threads from the
round-robin case

1999: Brian's work may be related to 1992 paper by D.J. Iannucci
and J. Lekashman entitled "MFTP: Virtual TCP window scaling using
multiple connections".  


Could do current_target_offset (on each) and current_nbytes (on each)?

packetnum,packetoffset (offset is modulo MTU), finalbuf=number?
// just find the one with an incomplete offset

********************************************************/
#include "mTCP.hh"
#include <math.h>
unsigned char mTCPmagic[8] = {0x08,0xFE,0x08,0xFF,0x08,0xFE,0x00,0x00};

#define mTCP_MTU 1024 // not necessarily true, but it will do for now
// might change to 8192 since it works so well for single sockets
// and might decrease subroutine overhead
		     
mTCPserver::mTCPserver(int port,int nports):RawTCPserver(port){
  // Use Brian's auto-negotiation method to determine #ports
  // RawTCPport *master = this->accept(); // accept master connection
  //char magic[8];
  //if(!master.isValid() || master.read(magic,8)<8) {
  // setup for single socket connection
  // or dump core.  This sucker ain't valid
  // exit(0); // poor recovery method
  // }
  // if(!strncmp(magic,master,8))
  // if it is just a single socket, copy to tmp buffer
  // and piggyback on first read...  set socket_is_clueless flag
  ports.setSize(nports);
  ready_to_read.setSize(nports);
  ready_to_write.setSize(nports);
  read_bytes.setSize(nports);
  write_bytes.setSize(nports);
  total_read_bytes=total_write_bytes=0;
  mtu=mTCP_MTU;
  for(int i=0;i<ports.getSize();i++){
    read_bytes[i]=write_bytes[i]=0;
    ports[i] = this->accept(); // accept clients one-at-a-time
    ready_to_read[i].addInport(ports[i]); // allows nonblocking check if ready-to-read
    ready_to_write[i].addOutport(ports[i]);// allows nonblocking check if ready-to-write
  }
  // OK, we are ready to rock-n-roll
}

mTCPserver::~mTCPserver(){
  //delete master;
  for(int i=0;i<ports.getSize();i++){
    if(ports[i]){
      delete ports[i];
      ports[i]=0;
    }
  }
}
// gotta think of what to do with accept()?
int mTCPserver::read(void *buffer,size_t size){
  char *cbuf=(char*)buffer;
  int startoffset=total_read_bytes;
  int endoffset=total_read_bytes+size;
  int ctr=0;
  while(ctr<size){
    for(int i=0;i<ports.getSize();i++){
      if(ready_to_read[i].poll()){
	// lets read stuff
	// must compute offset 
	int readsize = read_bytes[i]%mtu; // for incomplete reads
	if(readsize==0) readsize=mtu; // read at least an mtu''s 
	// worth of stuff
	// test against total read size
	int varraypos = i*mtu + (int)(floor((float)(read_bytes[i])/(float)mtu))*mtu*ports.getSize() + read_bytes[i]%mtu;
	int offset = varraypos-startoffset; // offset into current buffer
	if((varraypos+readsize)>endoffset)
	  readsize = endoffset-varraypos; // possible danger here...
	if(readsize<=0) continue;
	int rval = ports[i]->RawTCPport::read(cbuf+offset,readsize);
	if(rval<0) continue;
	else {
	  (read_bytes[i]) += rval;
	  ctr += rval;
	}
      }
    }
  }
  total_read_bytes+=ctr;
  return ctr;
}
int mTCPserver::write(void *buffer,int size){
  char *cbuf=(char*)buffer;
  int startoffset=total_write_bytes;
  int endoffset=total_write_bytes+size;
  int ctr=0;
  while(ctr<size){
    for(int i=0;i<ports.getSize();i++){
      if(ready_to_write[i].poll()){
	// lets write stuff
	// must compute offset 
	int writesize = write_bytes[i]%mtu; // for incomplete writes
	if(writesize==0) writesize=mtu; // write at least an mtu''s 
	// worth of stuff
	// test against total write size
	int varraypos = i*mtu + (int)(floor((float)(write_bytes[i])/(float)mtu))*mtu*ports.getSize() + write_bytes[i]%mtu;
	int offset = varraypos-startoffset; // offset into current buffer
	if((varraypos+writesize)>endoffset)
	  writesize = endoffset-varraypos;
	if(writesize<=0) continue;
	int rval = ports[i]->RawTCPport::write(cbuf+offset,writesize);
	if(rval<0) continue;
	else {
	  (write_bytes[i]) += rval;
	  ctr += rval;
	}
      }
    }
  }
  total_write_bytes+=ctr;
  return ctr;  
}



mTCPclient::mTCPclient(char *host,int port,int nports):
  RawTCPclient(host,port){
  ports.setSize(nports);
  ready_to_read.setSize(nports);
  ready_to_write.setSize(nports);
  read_bytes.setSize(nports);
  write_bytes.setSize(nports);
  total_read_bytes=total_write_bytes=0;
  mtu=mTCP_MTU;
  {
    for(int i=0;i<ports.getSize();i++){
      //printf("mTCP::constructor for client... gen connection %u\n",i);
      read_bytes[i]=write_bytes[i]=0;
      if(i)
	ports[i] = new RawTCPclient(host,port); // create clients one-at-a-time
      else 
	ports[i] = this;
      //puts("\tcreated\n");
      //printf("\tValidity=%u\n",ports[i]->isValid());
      ready_to_read[i].addInport(ports[i]); // allows nonblocking check if ready-to-read
      ready_to_write[i].addOutport(ports[i]);// allows nonblocking check if ready-to-write
    }
  }
  // OK, we are ready to rock-n-roll
  puts("ready to go");
}

mTCPclient::~mTCPclient(){
  for(int i=1;i<ports.getSize();i++){
    if(ports[i]){
      delete ports[i];
      ports[i]=0;
    }
  }
}
// gotta think of what to do with accept()?
int mTCPclient::read(void *buffer,size_t size){
  char *cbuf=(char*)buffer;
  int startoffset=total_read_bytes;
  int endoffset=total_read_bytes+size;
  int ctr=0;
  // puts("begin read");
  while(ctr<size){
    for(int i=0;i<ports.getSize();i++){
      if(ready_to_read[i].poll()){
	// lets read stuff
	// must compute offset 
	int readsize = read_bytes[i]%mtu; // for incomplete reads
	if(readsize==0) readsize=mtu; // read at least an mtu''s 
	// worth of stuff
	// test against total read size
	int varraypos = i*mtu + (int)(floor((float)(read_bytes[i])/(float)mtu))*mtu*ports.getSize() + read_bytes[i]%mtu;
	int offset = varraypos-startoffset; // offset into current buffer
	if((varraypos+readsize)>endoffset)
	  readsize = endoffset-varraypos; // possible danger here...
	if(readsize<=0) continue;
	int rval = ports[i]->RawTCPport::read(cbuf+offset,readsize);
	if(rval<0) continue;
	else {
	  (read_bytes[i]) += rval;
	  ctr += rval;
	}
      }
    }
  }
  total_read_bytes+=ctr;
  return ctr;
}

int mTCPclient::write(void *buffer,int size){
  char *cbuf=(char*)buffer;
  int startoffset=total_write_bytes;
  int endoffset=total_write_bytes+size;
  int ctr=0;
  //puts("begin write");
  while(ctr<size){
    for(int i=0;i<ports.getSize();i++){
      if(ready_to_write[i].poll()){
	// lets write stuff
	// must compute offset 
	int writesize = write_bytes[i]%mtu; // for incomplete writes
	if(writesize==0) writesize=mtu; // write at least an mtu''s 
	// worth of stuff
	// test against total write size
	int varraypos = i*mtu + (int)(floor((float)(write_bytes[i])/(float)mtu))*mtu*ports.getSize() + write_bytes[i]%mtu;
	int offset = varraypos-startoffset; // offset into current buffer
	if((varraypos+writesize)>endoffset)
	  writesize = endoffset-varraypos;
	if(writesize<=0) continue;
	int rval = ports[i]->RawTCPport::write(cbuf+offset,writesize);
	if(rval<0) continue;
	else {
	  (write_bytes[i]) += rval;
	  ctr += rval;
	}
      }
    }
  }
  total_write_bytes+=ctr;
  return ctr;  
}
