#include <stdio.h>
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif
#include "RawPort.hh"
#include <stdio.h>
#include <errno.h>
extern int errno;

RawPort::RawPort(){
#ifndef WIN32
  port=-1;
#else
  port = INVALID_SOCKET;
#endif
}

#ifndef WIN32
void RawPort::setPort(int fid){
  if(port>=0) close(port);
#else
void RawPort::setPort(SOCKET fid){
  if(port != INVALID_SOCKET) 
    closesocket(port);
#endif
  port=fid;
}

#ifndef WIN32
 RawPort::RawPort(int fid):port(fid){}
#else
 RawPort::RawPort(SOCKET fid):port(fid){}
#endif


RawPort::~RawPort(){
//  puts("Close RawPort");
#ifndef WIN32
  if(port>=0){ // need refcount for dup-ed ports
    close(port);
#else
  if(port != INVALID_SOCKET){ // need refcount for dup-ed ports
    closesocket(port);
#endif
 //   puts("*****Closed");
    // can manage refcount in central (static var) db.
  }
}

/*
int RawPort::write(void *str,int size)
{
  if(port<0)
    return 0; // which is number of bytes read, user must check for reason
  return ::write(port,(char*)str,size);
}

int RawPort::read(void *buffer,int size){
  long bytes=0;
 
  if(port<0)
    return 0;
  // printf("portnumber = %d\n",port);
  bytes = ::read(port,(char*)buffer, size);
  // printf("RawPort::read() nbytes=%ld\n",bytes);
  if(bytes<0){
    printf("errno=%d\n",errno);
    perror("RawPort::read()\n");
  }
  return bytes;
}
*/

FILE *RawPort::fdopen(const char *access){
#ifndef WIN32
  return ::fdopen(port,access);
#else
  /* NCSA: How do we do this for NT?? */
  return NULL;
#endif
}

RawPort *RawPort::dup() {
#ifndef WIN32
  return new RawPort(::dup(port));
#else
  /* NCSA: How do we do this for NT?? */
  return NULL;
#endif
}


