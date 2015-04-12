#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <assert.h>

using namespace std;

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
#include "RawMCAST.hh"

RawMCAST::RawMCAST(char *host,int pt):RawPort(){
#ifndef WIN32
  int sock;  
  const int on = 1;
#else
  SOCKET sock;  
  const SOCKET on = 1;
#endif
  this->flags=0;
  this->isbound=0;
  this->isjoined=0;
  //cerr << "RawMCAST::init()... next setAddress(host,port)" << endl;
  if(host)
    this->address.setAddress(host,pt);
  else
    this->address.setAddress(pt);
  //cerr << "host " << host << ':' << pt << endl;
  //cerr << "RawMCAST::init: socket PF_INET, SOCK_DGRAM" << endl;
  sock=::socket(PF_INET,SOCK_DGRAM,0);
#ifndef WIN32
  if(sock<0) 
#else
  if (sock == INVALID_SOCKET)
#endif
      {
	reportError();
	cerr << "RawMCAST::socket(): error " << getError() << endl;
	cerr << "\terror: " << errorString() << endl;
      }
  setPort(sock);
  if(::setsockopt(port,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on))!=0){
    cerr << "Failed to set SO_REUSEADDR socket option!" << endl;
  }
  /*
    If we are a sender, then we just sendTo the mcast addr
    If we are a receiver, then we must 
        * bind() to sockaddr and port
	 * join multicast group
	 * turn loopback off
   */
}

RawMCAST::~RawMCAST(){
  if(this->isjoined) this->leave();
  // send shutdown request to RawPort?
} // should close sock?  or handled by port?

int RawMCAST::recv(char *buffer,int bufsize,int flgs){
  if(flgs>=0) flags=flgs;
  // puts("now we get... why do we have flags sep from flgs?");
  int r;
  if(isbound)
    r=::recv(port,buffer,bufsize,flags);
  else{
//#if defined(LINUX) || defined(AIX)
    socklen_t fromlen=sizeof(struct sockaddr_in);
//#else
//    int fromlen=sizeof(struct sockaddr_in);
//#endif
    sockaddr sin;
    ::memcpy(&sin,this->address.getSocketAddress(),sizeof(sockaddr));
    r=::recvfrom(port,buffer,bufsize,flags,&sin,&fromlen);
  }
#ifndef WIN32
  if(r<0) reportError();
#else
  if(r == SOCKET_ERROR) reportError();
#endif
  return r;
}

// perhaps this should be for the UDP server?
int RawMCAST::recv(char *buffer,int bufsize,
		 IPaddress &from,int flgs){
//#if defined(LINUX) || defined(AIX)
  socklen_t fromlen=sizeof(struct sockaddr_in);
//#else
//  int fromlen=sizeof(struct sockaddr_in);
//#endif
  struct sockaddr_in sin; // KLUDGE??? What is this initialized from?
  if(flgs>=0) flags=flgs;
  /* fromlen? */
  int n = ::recvfrom(port,buffer,bufsize,flags,
		     (sockaddr *)&sin,
		     &fromlen);
  from.setAddress(sin);
  return n;
}

int RawMCAST::send(char *buffer,int bufsize,int flgs){
  // how to sendto???
  if(flgs>=0) flags=flgs;
  // will fail if not bound or connected
  int n;
  if(isbound){
    //cerr << "isbound send" << endl;
    n=::send(port,buffer,bufsize,flags);
  }
  else { // socket is not bound to port or IP
    socklen_t addrlen=sizeof(struct sockaddr_in);
    // cerr << "unbound send" << endl;
    n=::sendto(port,buffer,bufsize,flags,
	       this->address.getSocketAddress(),
	       sizeof(struct sockaddr_in));
    // cerr << "\tN=" << n <<endl;
  }
  return n;
}

int RawMCAST::send(char *buffer,int bufsize,
		 IPaddress &to,int flgs){
  // how to sendto???
  if(flgs>=0) flags=flgs;
  
  return ::sendto(port,buffer,bufsize,flags,to.getSocketAddress(),sizeof(struct sockaddr_in));
}

//-------------------> Server <--------------------
int RawMCAST::bind(){
  int n; // can use bind to find first availible port
  // puts("bind");
  n=::bind(port,this->address.getSocketAddress(),address.getSockaddrSize());
  //puts("afterBind");
  this->isbound=1;
#ifndef WIN32
  if(n<0) 
#else
    if (n == SOCKET_ERROR) 
#endif
      {
	reportError();
	isbound=0;
	cerr << "UDPserver::bind(): error " << getError() << endl;
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
      }
  return n;
}

//-------------------> Client <---------------------
int RawMCAST::connect(IPaddress &addr){
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
	//cerr << "\terror: " << sys_errlist[errno] << endl;
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


int RawMCAST::join(){
  struct ip_mreq mreq; /* in_addr imr_multiaddr= mcast addr */
  int n;
  //  struct ifreq ifrq;
  // address.setAddress((sockaddr_in*)(addr),sizeof(sockaddr_in));
  //address.getSocketAddress(),address.getSockaddrSize()
  ::memcpy(&mreq.imr_multiaddr,
	 &(this->address.getSockaddrPtr()->sin_addr),
	 sizeof(struct in_addr));
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  n=::setsockopt(port, IPPROTO_IP,IP_ADD_MEMBERSHIP,
		    &mreq,sizeof(mreq));
  if(n!=0) cerr << "MCAST JOIN failed!" << endl;
  else this->isjoined=1;
  return n;
} 

int RawMCAST::leave(){
  struct ip_mreq mreq; /* in_addr imr_multiaddr= mcast addr */
  //struct ifreq ifrq;
  
  ::memcpy(&mreq.imr_multiaddr,
	 &(this->address.getSockaddr_in()->sin_addr),
	 sizeof(struct in_addr));
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  this->isjoined=0; // even if it fails...
  return (::setsockopt(port, IPPROTO_IP,IP_DROP_MEMBERSHIP,
		    &mreq,sizeof(mreq)));
}
int RawMCAST::setTTL(int ttl){
  int flag = ttl;
  return (::setsockopt(port,IPPROTO_IP,IP_MULTICAST_TTL,
		     &flag,sizeof(flag)));
}
int RawMCAST::setLoopback(int truthval){
  int flag = truthval;
  return (::setsockopt(port,IPPROTO_IP,IP_MULTICAST_LOOP,
		     &flag,sizeof(flag)));
}
