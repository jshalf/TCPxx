#ifndef __RAWMCAST_H_
#define __RAWMCAST_H_

#include <stdlib.h>

#include <sys/types.h>
#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#else
#include <windows.h>
#endif

#include "IPaddress.hh"
#include "RawPort.hh"

/*=========================================================
class:    RawMCAST
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
class TCPXX_API RawMCAST : public RawPort {
protected:
  IPaddress address;
  int flags;
  int isbound,isjoined;
public:
  // RawMCAST(); // for connectionless service
  // in this case, the host name will be an addr
  // of 224.0.0.32 or above except in the case of
  // MBone which uses sap.mcast.net for group and
  // 9875 for port
  RawMCAST(){this->isbound=0; this->isjoined=0;}
  RawMCAST(char *host,int pt); // for connected service
  // RawMCAST(int pt); // port on localhost (good argument for a server)  
  virtual ~RawMCAST();
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
  int connect(){return this->connect(this->address);}
  int connect(IPaddress &addr);
  int join();
  int leave();
  // not messing with multi-IFC yet
  // virtual int setInterface(char *ifname);
  int setTTL(int ttl);
  int setLoopback(int truthval=1);
};


#endif
