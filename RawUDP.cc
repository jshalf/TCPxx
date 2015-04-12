#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <assert.h>

#include <sys/types.h>
#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

#include <sys/stat.h>
#include <fcntl.h>
// #include <sys/errno.h>

//extern int errno;
//extern char *sys_errlist[];

using namespace std;

//#include "NewString.hh"
#include "IPaddress.hh"
#include "RawPort.hh"
#include "RawUDP.hh"

RawUDP::RawUDP(){
#ifndef WIN32
  int sock;
#else
  SOCKET sock;
#endif
  flags=0;
  sock=socket(PF_INET,SOCK_DGRAM,0);
#ifndef WIN32
  if(sock<0) 
#else
  if (sock == INVALID_SOCKET)
#endif
      {
	reportError();
	cerr << "RawUDP::socket(): error " << getError() << endl;
	cerr << "\terror: " << errorString() << endl;
      }
  setPort(sock);
}

RawUDP::RawUDP(char *host,int pt){
#ifndef WIN32
  int sock;
#else
  SOCKET sock;
#endif
  
  flags=0;
  //cerr << "RawUDP::init()... next setAddress(host,port)" << endl;
  this->address.setAddress(host,pt);
  //cerr << "host " << host << ':' << pt << endl;
  //cerr << "RawUDP::init: socket PF_INET, SOCK_DGRAM" << endl;
  sock=socket(PF_INET,SOCK_DGRAM,0);
#ifndef WIN32
  if(sock<0) 
#else
  if (sock == INVALID_SOCKET)
#endif
      {
	reportError();
	cerr << "RawUDP::socket(): error " << getError() << endl;
	cerr << "\terror: " << errorString() << endl;
      }
  setPort(sock);
  // can connect ... but would be easier to respec address each time
  //cerr << "socket(PF_INET,SOCK_DGRAM) has been allocated" << endl;
}

RawUDP::RawUDP(int pt){ // localhost
#ifndef WIN32
  int sock;
#else
  SOCKET sock;
#endif
  
  flags=0;
  address.setAddress(pt);
  /* So we cannot get host by address when
     we are not really using an address field here.
     (just a portnum).  What do we really need for bind
     on a server socket anyways?  Probably just localhost.
     Need to ditch the send and use the std localhost addr?
     Or just change /etc/hosts.conf to do local res first. 
     But current host resolution seems broken. Need to bind when
     we are actively receiving from a client. */
  sock=socket(PF_INET,SOCK_DGRAM,0);
#ifndef WIN32
  if(sock<0) 
#else
    if(sock == INVALID_SOCKET)
#endif
      {
	reportError();
	cerr << "RawUDP::socket(): error " << getError() << endl;
	cerr << "\terror: " << errorString() << endl;
      }
  setPort(sock);
}

RawUDP::~RawUDP(){}

int RawUDP::recv(char *buffer,int bufsize,int flgs){
  if(flgs>=0) flags=flgs;
  // puts("now we get... why do we have flags sep from flgs?");
  int r=::recv(port,buffer,bufsize,flags);
#ifndef WIN32
  if(r<0) reportError();
#else
  if(r == SOCKET_ERROR) reportError();
#endif
  return r;
}

// perhaps this should be for the UDP server?
int RawUDP::recv(char *buffer,int bufsize,
		 IPaddress &from,int flgs){
//#if defined(LINUX) || defined(AIX) || 
  socklen_t fromlen=(socklen_t)sizeof(struct sockaddr_in);
//#else
//  int fromlen=sizeof(struct sockaddr_in);
//#endif
  struct sockaddr_in sin;
  if(flgs>=0) flags=flgs;
  /* fromlen? */
  int n = ::recvfrom(port,buffer,bufsize,flags,(sockaddr *)&sin,&fromlen);
  from.setAddress(sin);
  return n;
}

int RawUDP::send(char *buffer,int bufsize,int flgs){
  // how to sendto???
  if(flgs>=0) flags=flgs;
  // will fail if not bound or connected
  int n=::send(port,buffer,bufsize,flags);
  return n;
}

int RawUDP::send(char *buffer,int bufsize,
		 IPaddress &to,int flgs){
  // how to sendto???
  if(flgs>=0) flags=flgs;
  
  return sendto(port,buffer,bufsize,flags, to.getSocketAddress(),sizeof(struct sockaddr_in));
}

int RawUDP::setDSCP(int dscp){
  int n;
  
  dscp <<= 2;
  n = ::setsockopt(port, IPPROTO_IP, IP_TOS, &dscp, sizeof(dscp));

#ifndef WIN32
  if(n<0) 
#else
    if (n == SOCKET_ERROR) 
#endif
      {
       reportError();
       cerr << "RawUDP::setDSCP(): error " << errno ;
       cerr << " (" << errorString() << ") " << endl;
      }
  return n;
}

//-------------------> Server <--------------------
int RawUDP::bind(){
  return this->bind(this->address.getPort());
}
int RawUDP::bind(int pt){
  int n; // can use bind to find first availible port
  // puts("bind");
  IPaddress addr(pt);
  //  this->address.setAddress(pt);
  // printf("OK, I'm binding sock=%u to port %u sz=%u\n",
  //	 port,addr.getPort(),addr.getSockaddrSize());
  n=::bind(port,addr.getSocketAddress(),
	   addr.getSockaddrSize());
  //printf("afterBind n=%d\n",n);
#ifndef WIN32
  if(n<0) 
#else
    if (n == SOCKET_ERROR)
#endif
      {
	reportError();
	cerr << "RawUDP::bind(): error " << getError() << endl;
	cerr << "\terror: " << errorString() << endl;
	switch(failed()){
#ifndef WIN32
	case EBADF: cerr << "EBADF" << endl; break;
	case ENOTSOCK: cerr << "ENOTSOCK" << endl; break;
	case EADDRNOTAVAIL: cerr << "EADDRNOTAVAIL" << endl; break;
	case EADDRINUSE: cerr << "EADDRINUSE" << endl; break;
	case EINVAL: cerr << "EINVAL" << endl; break;
	case EACCES: cerr << "EACCES" << endl; break;
	case EFAULT: cerr << "EFAULT" << endl; break;
#else
	case WSAEBADF: cerr << "WSAEBADF" << endl; break;
	case WSAENOTSOCK: cerr << "WSAENOTSOCK" << endl; break;
	case WSAEADDRNOTAVAIL: cerr << "WSAEADDRNOTAVAIL" << endl; break;
	case WSAEADDRINUSE: cerr << "WSAEADDRINUSE" << endl; break;
	case WSAEINVAL: cerr << "WSAEINVAL" << endl; break;
	case WSAEACCES: cerr << "WSAEACCES" << endl; break;
	case WSAEFAULT: cerr << "WSAEFAULT" << endl; break;
#endif 
	default: cerr << "dont know nuthin man" << endl; break;
	}
	/* actually, this is bad...
	   we should permit more graceful failure */
	/*	close(this->port);
		this->port=-1; */
      }
  return n;
}

RawUDPserver::RawUDPserver(int pt):RawUDP(pt){
  if(this->bind()<0){
    if(this->port>=0) close(this->port);
    this->port=-1; // make it such that isValid=0;
  }
}

RawUDPserver::~RawUDPserver(){}


void PrintSin(sockaddr_in &sin){
  cerr << "sin_family  "; 
  if(sin.sin_family == AF_INET)
    cerr << "AF_INET" << endl;
  else if(sin.sin_family==PF_INET)
    cerr << "PF_INET" << endl;
  else
    cerr << "invalid" << endl;
  cerr << "sin_port " << sin.sin_port << endl;
}


//-------------------> Client <---------------------
int RawUDP::connect(char *host,int pt){
  this->address.setAddress(host,pt);
  return this->connect(this->address);
}
int RawUDP::connect(IPaddress &addr){
  int n; // can use bind to find first availible port
  // puts("now we connect");
  n=::connect(port, addr.getSocketAddress(), addr.getSockaddrSize());
  //puts("done connect");
  //printf("\tretval = %d\n",n);
#ifndef WIN32
  if(n<0) 
#else
    if (n == SOCKET_ERROR) 
#endif
      {
	reportError();
	cerr << "UDPclient::connect(): error " << errno << endl;
#ifdef NO_STRERROR
	cerr << "\terror: " << sys_errlist[errno] << endl;
#else
	cerr << "\terror: " << strerror(errno) << endl;
#endif
	switch(errno){
#ifndef WIN32
	case EBADF: cerr << "EBADF" << endl; break;
	case ENOTSOCK: cerr << "ENOTSOCK" << endl; break;
	case EADDRNOTAVAIL: cerr << "EADDRNOTAVAIL" << endl; break;
	case EADDRINUSE: cerr << "EADDRINUSE" << endl; break;
	case EINVAL: cerr << "EINVAL" << endl; break;
	case EACCES: cerr << "EACCES" << endl; break;
	case EFAULT: cerr << "EFAULT" << endl; break;
#else
	case WSAEBADF: cerr << "WSAEBADF" << endl; break;
	case WSAENOTSOCK: cerr << "WSAENOTSOCK" << endl; break;
	case WSAEADDRNOTAVAIL: cerr << "WSAEADDRNOTAVAIL" << endl; break;
	case WSAEADDRINUSE: cerr << "WSAEADDRINUSE" << endl; break;
	case WSAEINVAL: cerr << "WSAEINVAL" << endl; break;
	case WSAEACCES: cerr << "WSAEACCES" << endl; break;
	case WSAEFAULT: cerr << "WSAEFAULT" << endl; break;
#endif 
	default: cerr << "dont know nuthin man" << endl; break;
	}
      }
  return n;
}

RawUDPclient::RawUDPclient(char *host,int pt):RawUDP(host,pt){
  this->connect();
}

RawUDPclient::~RawUDPclient(){}
