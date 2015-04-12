#ifndef __RAWUDP_H_
#define __RAWUDP_H_

#include <stdlib.h>

#include <sys/types.h>
#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#else
#include <windows.h>
#endif

//#include "NewString.hh"
#include "IPaddress.hh"
#include "RawPort.hh"

/*=========================================================
class:    RawUDP
inherits: RawPort
purpose:  The purpose is not completely clear at this point.
          This particular class implements connectionless
	  UDP messaging, however,
          there are some ambiguities in how to use it with
	  streaming data.  It is inherited from a basetype that
	  is clearly streaming, but you can't really use it in
	  that manner safely.  Perhaps there should be a "message"
	  baseclass which can determine max-avail messagesize
	  for the purpose of sending packets instead of streaming
	  data.
===========================================================*/
class TCPXX_API RawUDP : public RawPort {
protected:
  IPaddress address;
  int flags;
public:
  RawUDP(); // for connectionless service
  // need to eliminate the port & host,pt constructors)
  RawUDP(char *host,int pt); // for connected service
  RawUDP(int pt); // port on localhost (good argument for a server) 
  int connect(){return this->connect(this->address);}
  int connect(IPaddress &addr);
  int connect(char *host,int port);
  int setDSCP(int dscp);
  
  virtual ~RawUDP();
  /*
    Not sure what to do with these stream-like functions.
    Technically this can't be a proper rawport without these.
    Perhaps it should just give an error if the message is
    too long... or perhaps it should just break the message
    into the maximum sized packets availible (although it
    will be meaningless you wouldn't know how the packets
    order.  Fragments will be reassembled, but there is a
    danger of high lossage if you are way below the MTU.
    */
  virtual int write(void *buffer,int bufsize){
    return RawPort::write(buffer,bufsize);
  }
  virtual int read(void *buffer,int bufsize){
    return RawPort::read(buffer,bufsize);
  }
/*
  virtual int write(void *buffer,int bufsize,char *hostname,int prt){
    return ::write(prt,buffer,bufsize);
  } */
  // The 'flgs' argument is just passing on standard
  // flags to the OS ::recv() call.  This needs to be
  // made more intuitive.... (right now its there to match
  // the functionality of the underlying system calls).
  virtual int recv(char *buffer,int bufsize,int flgs=-1);
  // perhaps this should be for the UDP server?
  virtual int recv(char *buffer,int bufsize,
		   IPaddress &from,int flgs=-1);
  virtual int send(char *buffer,int bufsize,
		   IPaddress &to,int flgs=-1);
  virtual int send(char *buffer,int bufsize,int flgs=-1);
  virtual int bind();
  virtual int bind(int pt);
};

/*==========================================================
class: RawUDPserver
inherits: RawUDP
purpose: Implemented server end of connected UDP service.
         The benefit to the programmer is that you no longer 
	 need to specify a portnumber when you send messages.
============================================================*/

class RawUDPserver : public RawUDP {
private:
  virtual int bind(){/* puts("now bind");*/ return RawUDP::bind(); }
public:
  // pt will bind to first availible
  RawUDPserver(int pt=0); // port on localhost (good argument for a server)
  virtual ~RawUDPserver();
  virtual inline int recv(char *buffer,int bufsize,IPaddress &from,int flgs=-1){
    return RawUDP::recv(buffer,bufsize,from,flgs);
  }
  virtual inline int recv(char *buffer,int bufsize,int flgs=-1){
    return RawUDP::recv(buffer,bufsize,flgs);
  }
  virtual inline int send(char *buffer,int bufsize,IPaddress &to,int flgs=-1){
    return RawUDP::send(buffer,bufsize,to,flgs);
  }
  virtual inline int send(char *buffer,int bufsize,int flgs=-1){
    return RawUDP::send(buffer,bufsize,flgs);
  }
  virtual inline int write(void *buffer,int bufsize){
    return RawPort::write(buffer,bufsize);
  }
  virtual inline int read(void *buffer,int bufsize){
    return RawPort::read(buffer,bufsize);
  }
};

/*==========================================================
class: RawUDPclient
inherits: RawUDP
purpose: Implemented client end of connected UDP service.
         The benefit to the programmer is that you no longer 
	 need to specify a portnumber when you send messages.
============================================================*/
class RawUDPclient : public RawUDP {
private:
  int connect() {return RawUDP::connect();}
public:
  RawUDPclient(char *host,int pt); // port on localhost (good argument for a server)
  virtual ~RawUDPclient();

  virtual inline int recv(char *buffer,int bufsize,IPaddress &from,int flgs=-1){
    return RawUDP::recv(buffer,bufsize,from,flgs);
  }
  virtual inline int recv(char *buffer,int bufsize,int flgs=-1){
    return RawUDP::recv(buffer,bufsize,flgs);
  }
  virtual inline int send(char *buffer,int bufsize,IPaddress &to,int flgs=-1){
    return RawUDP::send(buffer,bufsize,to,flgs);
  }
  virtual inline int send(char *buffer,int bufsize,int flgs=-1){
    return RawUDP::send(buffer,bufsize,flgs);
  }
  virtual inline int write(void *buffer,int bufsize){
    return RawPort::write(buffer,bufsize);
  }
  virtual inline int read(void *buffer,int bufsize){
    return RawPort::read(buffer,bufsize);
  }
};

#endif
