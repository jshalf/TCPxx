#ifndef __RAWTCPSTREAM_H
#define __RAWTCPSTREAM_H

#include "TCPXXWinDLLApi.h"
#include <stdlib.h>
// System/Network
#include <sys/types.h>
#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

#include "RawPort.hh"
#include "IPaddress.hh"

#ifndef Megs
#define Megs(x) (x*1024*1024)
#endif
#ifndef K
#define K(x) (x*1024)
#endif
#ifndef SIG_PF /* if no prototype for signal handlers exists, then create one */
typedef void (*SignalHandler)(int);
#else /* POSIX has its own standard C++ prototype for Signal Handlers */
typedef SIG_PF SignalHandler;
#endif
/*=================================================
class:    RawTCPport
inherits: RawPort
purpose:  Base class for all TCP stream stuff.
          Decided to make this independent of 
	  the RawUDPport due to packet size
	  & reliability issues.
===================================================*/
class TCPXX_API RawTCPport : public RawPort {
protected:
  int qsize;
  IPaddress address;
  int alive; // flag true if socket is alive 
  // (will tie into keepalive eventually)
public: 
  RawTCPport(const char *host,int pt); // give hostname and port number
  RawTCPport(int pt,int q=5); // port number on localhost
  // RawTCPport(char *host,char *service); // hostname and servicename
  // RawTCPport(struct sockaddr_in *s,int fromlen,int pt);
#ifndef WIN32
  RawTCPport(IPaddress &addr,int sock);
#else
  RawTCPport(IPaddress &addr,SOCKET sock);
#endif

  virtual ~RawTCPport();

  /** gets the hostname and copys it into the string you
     provide.  The "size" argument indicates the maximum
     length of the string "name". */
  void getIPaddress(IPaddress &addrinout){ addrinout = address; }
  void getHostname(char *name,int size) { address.getHostname(name,size); }
  // could get read/write methods directly from parent class?
  // should have an aligned allocate built-in...
  // valloc() garauntees page-aligned allocation for
  // DMA transfer to network according to welch notes...
  // However the T3E does not have valloc() so I can't
  // do this!!!!

  //-----------------------------------------
  // Read and write are inlined for maximum performance!!
  //virtual int read(void *b,int len){ return ::read(port,b,len);}
  //virtual int write(void *b,int len){ return ::write(port,b,len);}
  
  // having send and recv don't seem to help performance any...
  	virtual int read(void *b,int len)
	{ 
#ifndef WIN32
	  int	r=::recv(port,b,len,0); 
	  if(r<0) 
	    reportError(); 
#else
	  int	r=::recv(port,(char*)b,len,0); 
	  if(r == SOCKET_ERROR)
	    reportError(); 
#endif
	  return r;
	}
	  /// (should be const void*b)

	virtual int write(void *b,int len)
	{
#ifndef WIN32
	int	r=::send(port,b,len,0); 
	if(r<0) reportError(); 
#else
	int r=::send(port,(char*)b,len,0); 
	if(r == SOCKET_ERROR)
	  reportError(); 
#endif
	return r;
	}


	virtual int writeOutOfBand(void *b,int len)
	{
#ifndef WIN32
	int	r=::send(port,b,len,MSG_OOB); 
	if(r<0) reportError(); 
#else
	int	r=::send(port,(char*)b,len,MSG_OOB); 
	if(r == SOCKET_ERROR) reportError(); 
#endif
	return r;
	}
	
	virtual int writeDontRoute(void *b,int len) 
	{
#ifndef WIN32
	  int	r=::send(port,b,len,MSG_DONTROUTE); 
	  if(r<0) reportError(); 
#else
	  int	r=::send(port,(char*)b,len,MSG_DONTROUTE); 
	  if(r == SOCKET_ERROR) reportError(); 
#endif
	  return r;
	}

  /* for sending integers in network byte-order */
  /* just convenience functions... can't do this reliably
     for floats.
     These are not inlined because if you feel the need to use them
     you are clearly not interested in maximum network performance.
     These are just convenience methods...
  */
  int read(int &i);
  int write(int i);
  int read(short &i);
  int write(short i);
  int read(long &i);
  int write(long i);
  // Standard operations that can be performed on a socket
  int connect();
  int bind();
  int listen();

  RawTCPport *accept();

  inline int isAlive(){return alive;} // will tie into keepalive eventually
  
  // Socket options:  This is the raw way of setting the socket options
  // You should use the specific methods for particular socket options
  // because the implementation differs on different machines.
  int setSockOpt(int level,int optname,void *optval,int optlen);
  int getSockOpt(int level,int optname,void *optval,int &optlen);
  int getSockOpt(protoent *level,int optname,void *optval,int &optlen);
  // Could set windowSize from setSockOpt(), but it would be better
  // to use this method because the Cray windowsize args differ.
  
  //***************Standard TCP options**************
  int setReuseAddr(int on=1);
  int setTCPnoDelay(int on=1);
  int setTCPfastAck(int on=1);

  //***************RFC 1323 options******************
  int setTCPwindowSize(int size); // size in bytes
  int getTCPmaxSegSize(); // Read-only (gets the TCP seg size) Only avail after connect()
  int setTCPtimestamp(int size) {return -1;} // not yet implemented

  //**************Standard Socket Options************
  int getSendBufSize(int &size); // sets size in bytes
  int getRecvBufSize(int &size); // size in bytes
  int setSendBufSize(int size); // sets size in bytes
  int setRecvBufSize(int size); // size in bytes
  // If KeepAlive is turned on, then periodic messages are sent in
  // the background to determine whether if the connection is up.  
  // If the messages are not responded to, then the connection is
  // closed and a SIGPIPE signal is generated.
  int setKeepAlive(int on=1);
  // This version turns keepalive on and installs a signal handler
  // to catch SIGPIPE if the connection dies.
  int setKeepAlive(SignalHandler handler);
  int setDebug(int on=1); // toggles recording of debugging info
  int setLinger(int on=1);
};

/*=================================================
class:    RawTCPclient
inherits: RawTCPport
purpose:  Hides TCP connection establishment etc...
===================================================*/
class TCPXX_API RawTCPclient : public RawTCPport
{
protected: 
  inline int connect(){ return RawTCPport::connect(); }
  RawTCPclient(const char *host,int port,int optionflags); // delayed startup option
  void start();
public:
  RawTCPclient(const char *host,int port); // give hostname and port number
  virtual ~RawTCPclient();
};

/*===============================================
class:    RawTCPserver
inherits: RawTCPport
purpose:  Hides TCP server socket setup
=================================================*/
class TCPXX_API RawTCPserver : public RawTCPport
{
protected:
  // int qsize;
#ifndef WIN32
  inline int bind() { return RawTCPport::bind();  }
  inline int listen(){ return RawTCPport::listen();}
#else
  inline SOCKET bind() { return RawTCPport::bind();  }
  inline SOCKET listen(){ return RawTCPport::listen();}
#endif
  RawTCPserver(int port,int q,int optionflags); // late startup option 
  bool start(); // actual startup
public:
  RawTCPserver(int port,int q=5); // give hostname and port number
  virtual ~RawTCPserver();
  inline RawTCPport *accept(){ return RawTCPport::accept(); }
};

/*===============================================
class:    RawTCPconnection
inherits: RawTCPport
purpose:  One end of a 2-sided connection.  If it
		  is first to start up then it creates a
		  server socket that awaits a client connection.
		  Otherwise, if it is last to start up, it
		  will connect to the server on the other end
		  as a client.  Only one client allowed...
Note: Maybe it would be more intelligent to have
	  a separate start() method to actually begin
	  a connection instead of making it part of
	  the construction because so many options
	  need to be set after construction time.
	  Of course it may be smarter yet to force the
	  user to specify optionlists as part of the
	  constructor to enfore option ordering????
=================================================*/
#if 0
/* NCSA: We haven't converted all this yet to NT. Do we really need all this
   or is this legacy code lying around? 
*/
class RawTCPconnection : public RawTCPport {
protected:
	inline int connect(){ return RawTCPport::connect(); }
	inline int bind() { return RawTCPport::bind();  }
  	inline int listen(){ return RawTCPport::listen();}
  	inline RawTCPport *accept(){ return RawTCPport::accept(); }
  RawTCPport *connection,*server;
public:
	RawTCPconnection(const char *hostname,int port);
	~RawTCPconnection();
};
#endif
/*===============================================
class:    FastTCPclient
inherits: RawTCPclient
purpose:  Hides TCP client socket setup and allows
          setting of TCP windowsize for high speed
	   networks with large bandwidth-delay product.
=================================================*/
class TCPXX_API FastTCPclient : public RawTCPclient
{
public:
  FastTCPclient(const char *host,int port,int windowsize); // give hostname and port number
  FastTCPclient(const char *host,int port):RawTCPclient(host,port){} // use default size
  virtual ~FastTCPclient();
};

/*===============================================
class:    FastTCPserver
inherits: RawTCPserver
purpose:  Hides TCP server socket setup and allows
          setting of TCP windowsize for high speed
	   networks with large bandwidth-delay product.
=================================================*/
class TCPXX_API FastTCPserver : public RawTCPserver
{
public:
  FastTCPserver(int port,int windowsize,int q=5); // give hostname and port number
  FastTCPserver(int port):RawTCPserver(port) {}   // default window size
  virtual ~FastTCPserver();
};

#endif
