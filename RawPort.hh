#ifndef __RAWPORT_H
#define __RAWPORT_H
#include "TCPXXWinDLLApi.h"

#include <stdio.h>
#ifndef WIN32
#include <unistd.h>
#else
#include <windows.h>
#include <io.h> // For _read and _write
#endif

#include <string.h>
#include <fcntl.h>
#include "ErrorReport.hh"


/*=================================================
class: MetaPort
purpose: Any port-like object (anything that
        you can read or write to in a stream).
	This is just a pure virtual base base class 
	that allows files & sockets to be treated
	generically.
==================================================
class MetaPort {
public:
  virtual int write(void *buffer,int size)=0;
  virtual int read(void *buffer,int size)=0;
};
*/

/*==================================================
class: RawPort
inherits: MetaPort
purpose: Encapsulates Unix low-level file descriptors.
 ===================================================*/
//class RawPort : public MetaPort {
class TCPXX_API RawPort : public ErrorReport 
{
private:
// OrderedList<RawPort> aliases;
// static OrderedList<RawPort> allPorts; // check here for aliases, then store locally 
protected:
#ifndef WIN32
  int port;
#else
  SOCKET port;
#endif
  RawPort(); /* Allows you to allocate this structure without
		setting a port.  This can only be done by friends
		of this class though (must remain very restricted) 
	     */
  /* semi-kludge to allow post-assignment
     of ports by some of the socket classes.
     I should eventually rethink this... 
  */
#ifndef WIN32
  void setPort(int fid);
#else
  void setPort(SOCKET fid);
#endif
  
public:
  /* constructor requires file ID.  Only the
     private RawPort() constructor can be used
     to post-set the port number */
#ifndef WIN32
  RawPort(const int fid);
#else
  RawPort(SOCKET fid);
#endif

  virtual ~RawPort();
  /* read/write supercede base class virtual members.  Read
     and write fixed buffers to the port.  Currently these
     are blocking reads & writes.  I will add fcnctl() goop
     to allow non-blocking reads & writes eventually. */

  virtual int read(void *buffer,const int bufsize) {
#ifndef WIN32
    int rval=::read(port,(char*)buffer,bufsize);
    if(rval<0) 
      reportError();
#else
    int rval= ::recv(port, (char*) buffer, (unsigned int) bufsize, 0);
    if (rval == SOCKET_ERROR)
      reportError();
#endif
    return rval;
  }

  virtual int write(const void *buffer,const int bufsize){
#ifndef WIN32
    int rval = ::write(port,(char*)buffer,bufsize);
    if(rval<0) 
      reportError();
#else
    int rval = ::send(port, (char*)buffer, bufsize, 0);
    if (rval == SOCKET_ERROR)
      reportError();
#endif
    return rval;
  }

  virtual int writeString(char *buffer){
    return write(buffer,strlen(buffer));
  }

  /* Bascially getPortNum allows you to unencapsulate the ports */
  // should be getFileID()
#ifndef WIN32
  inline int getPortNum() {return port;} 
#else
  inline SOCKET getPortNum() {return port;} 
#endif

  /* Indicates if port has been opened successfully */
  inline bool isValid() {
#ifndef WIN32
    return (port>=0);
#else
    return (port != INVALID_SOCKET);
#endif
  }
  /* Converts the port into a filedescriptor 
     (eventually will encapsulate) */
  FILE *fdopen(const char *access);
  /* Dups a port when you fork a process that is pointing
     to the same open file descriptor */
  RawPort *dup();
  /* dup2() is a Berkeley feature which is not universally
     availible... I'll have to figure a different way to
     do this */
  /* inline RawPort &redirect() { return RawPort(dup2(port)); } */

#ifdef SYSV
  int ioctl(int cmd,termio &tio);
  termio &getTermio();
  void setTermio(termio &tio);
  // setup various line disciplines
  // can hide Berkeley and SysV differences
  // if we get rid of ioctl() as a method.
  // will need our own datastructures to store
  // the line discipline though...
  void setCanonical(int on);
#endif

  void setNonBlocking() const { 
#ifndef WIN32
    fcntl(port, F_SETFL, O_NONBLOCK); 
#else
    u_long argp;
    int result;
    
    printf("NCSA: Enabling socket into non-blocking mode.\n");
    fflush(stdout);
    
    argp = 1;
    
    result = ioctlsocket(port, FIONBIO, &argp);
    if (result == SOCKET_ERROR){
      printf("*****NCSA: Unable to set socket into non-blocking mode! Error %d\n", WSAGetLastError());
      fflush(stdout);
    }

#endif
  }
};

#endif
