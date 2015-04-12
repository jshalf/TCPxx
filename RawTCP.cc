#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
using namespace std;
#include <sys/types.h>
#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>
#else
#include <windows.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>


#include "IPaddress.hh"
#include "RawPort.hh"
#include "RawTCP.hh"

#define ERROR 0

// perhaps inherit both TCPport and etc. from RawPort
// and then have it contain FileDescriptor
// make RawPort a friend of FileDescriptor
// setPort(FileDescriptor);

RawTCPport::RawTCPport(const char *host,int pt)
: alive(0)
, qsize(5)
{
#ifndef WIN32
  int sock;
#else
  SOCKET sock;
#endif
  
  address.setAddress(host,pt); // !!!!!!!!!!!!
  //cout << "Set address at " << host << ":" << pt << "\n";

  sock=socket(AF_INET,SOCK_STREAM,0);
  setPort(sock); 
  setReuseAddr();

  // cout << "SOCK: " << sock << "\n";
}

RawTCPport::RawTCPport(int pt,int q):alive(0),qsize(q){
#ifndef WIN32
  int sock;
#else
  SOCKET sock;
#endif
  
  address.setAddress(pt); // !!!!!!!!!!!!
  //cout << "Set address on localhost:" << pt << "\n";
  sock=socket(AF_INET,SOCK_STREAM,0);
  setPort(sock);
  setReuseAddr();
  //cout << "SOCK: " << sock << "\n";
}

#ifndef WIN32
RawTCPport::RawTCPport(IPaddress &addr,int sock):address(addr){
#else
RawTCPport::RawTCPport(IPaddress &addr,SOCKET sock):address(addr){
#endif
  //address.setAddress(addr);
  setPort(sock);
  setReuseAddr();
}

RawTCPport::~RawTCPport(){
  // shutdown(port);
}

union netint8{
  u_long i;
  char c[8]; 
};

int RawTCPport::read(int &i){
  netint8 netint;
  int r=read(netint.c,8);
  i = (int)(ntohl(netint.i));
  return r;
}
int RawTCPport::write(int i){
  //u_long ui = htonl((u_long)i);
  netint8 netint;
  netint.i = htonl((u_long)i);
// if this is 4 bytes in big-endian, need a shift (do an if)
  return write(netint.c,8);
}
int RawTCPport::read(short &i){
  netint8 netint;
  int r=read(netint.c,8);
  i = (int)(ntohl(netint.i));
  return r;
}
int RawTCPport::write(short i){
  netint8 netint;
  netint.i = htonl((u_long)i);
  return write(netint.c,8);
}
int RawTCPport::read(long &i){
  netint8 netint;
  int r=read(netint.c,8);
  i = (int)(ntohl(netint.i));
  return r;
}
int RawTCPport::write(long i){
  netint8 netint;
  netint.i = htonl((u_long)i);
  return write(netint.c,8);
}

int RawTCPport::connect() 
{
#ifndef WIN32
  if(::connect(port,(sockaddr*)address.getSockaddrPtr(),
	       address.getSockaddrSize())<0)
#else
  if(::connect(port,(sockaddr*)address.getSockaddrPtr(),
	       address.getSockaddrSize()) == SOCKET_ERROR)
#endif
  {
    reportError();
    cerr << "class RawTCPport: connection failed because:" 
	 << errorString() << endl
	 << "connect:: failed" << endl;
    alive=0;
    return -1;
  }

  alive=1;
  return 1;
}

int RawTCPport::bind(){
#ifndef WIN32
  if(::bind(port,(sockaddr *)address.getSockaddrPtr(),sizeof(sockaddr))<0)
#else
  if(::bind(port,(sockaddr *)address.getSockaddrPtr(),sizeof(sockaddr)) == SOCKET_ERROR)
#endif
  {
	 reportError(); 
	 cerr << "class RawTCPserver: bind() failed because:" 
	      << errorString() << endl;
    return -1;
  }
  return 0;
}

int RawTCPport::listen(){
#ifndef WIN32
  if(::listen(port,qsize)<0)
#else
  if(::listen(port,qsize) == SOCKET_ERROR)
#endif
    {
      reportError();
      cerr << "class RawTCPserver: listen() failed because:" 
	   << errorString() << endl;
      return -1;
    }
  alive=1;
  return 0;
}

RawTCPport *RawTCPport::accept()
{
  sockaddr_in fsin;

//#if defined(LINUX) || defined(AIX)
  socklen_t fromlen = sizeof(sockaddr);
//#else
//  int fromlen = sizeof(sockaddr);
//#endif
  RawTCPport *p=0;
#ifndef WIN32
  int fid;
#else
  SOCKET fid;
#endif

  // **** should it be sizeof(sockaddr_in) or sizeof(sockaddr)
#ifndef WIN32
  if((fid=::accept(port,(sockaddr *)(&fsin),&fromlen))<=0)
#else
  if((fid=::accept(port,(sockaddr *)(&fsin),&fromlen)) == INVALID_SOCKET)
#endif
  {
	  reportError(); 
	  cerr << "class RawTCPserver: accept() failed because:" 
	       << errorString() << endl;
    return 0;
  }
  IPaddress addr(fsin);
  char buffer[128];
  addr.getHostname(buffer,sizeof(buffer));
  printf("Accepted connection from %s\n",buffer);
  p=new RawTCPport(addr,fid);
  return p;
}

int RawTCPport::setSockOpt(int level,int optname,void *optval,int optlen){
#ifndef WIN32
  return setsockopt(port,level,optname,optval,optlen);
#else
  return setsockopt(port,level,optname,(const char*) optval,optlen);
#endif
}

int RawTCPport::getSockOpt(int level,int optname,void *optval,int &optlen){
#ifndef WIN32
//#if defined(LINUX) || defined(AIX)
	socklen_t optl = optlen;
	int r = getsockopt(port,level,optname,optval,&optl);
	optlen=optl;
	return r;
//#else
//  return getsockopt(port,level,optname,optval,&optlen);
//#endif
#else
  return getsockopt(port,level,optname,(char*) optval,&optlen);
#endif
}
// Could set windowSize from setSockOpt(), but it would be better
  // to use this method because the Cray windowsize args differ.
int RawTCPport::setTCPwindowSize(int size){
  int rv;
  int testsize;
  // Everyone else depends on the send and recv buffer sizes
  if((rv=setSendBufSize(size))<0)
    perror("RawTCPport::setSendBufSize() failed\n");
  getSendBufSize(testsize);
  if(testsize!=size)
    fprintf(stderr,"setTCPwindowSize: Failed to set proper sendbuf size real=%u desired=%u\n",testsize,size);
  if((rv=setRecvBufSize(size))<0)
    perror("RawTCPport::setRecvBufSize() failed\n");
  getRecvBufSize(testsize);
  if(testsize!=size)
    fprintf(stderr,"setTCPwindowSize: Failed to set proper recvbuf size real=%u desired=%u\n",testsize,size);
#ifdef TCP_WINSHIFT /* Then its probably Cray Unicos or Irix 6.4 */
  int window_shift;
  //protoent *p=getprotobyname('tcp');
  // find proper window shift based on desired window size
  // window_shift = log2(size / 64k)
  window_shift=0;
  for(int i=0,v=64*1024;v<size;v<<=1){window_shift+=1;}
  rv=setSockOpt(IPPROTO_TCP,
	     TCP_WINSHIFT,
	     (char *)(&window_shift),
	     sizeof(window_shift));
  if(rv<0)
    perror("RawTCPport::setTCPwindowSize() setsockopt() for TCP_WINSHIFT failed\n");
#endif
  return rv;
}

int RawTCPport::setTCPfastAck(int on){
  int rv=1;
#ifdef TCP_FASTACK
  rv=setSockOpt(IPPROTO_TCP, TCP_FASTACK,(void *)(&on),sizeof(int));
#else
  fprintf(stderr,"No TCP_FASTACK on this platform\n");
#endif
  return rv;
}

int RawTCPport::setTCPnoDelay(int on){
  protoent *p;
  p = getprotobyname("tcp");
  reportError();
  if(!p)
  {
    perror("RawTCPport::setTCPnoDelay() failed to getprotobyname(tcp)");
    return -1; // failed
  }
  return setSockOpt(p->p_proto,TCP_NODELAY,(char *)(&on),sizeof(int));
}

int RawTCPport::getTCPmaxSegSize()
{
  protoent *p = getprotobyname("tcp"); 
  reportError();
  int size,retsz=sizeof(int);
#ifndef WIN32
  if(getSockOpt(p->p_proto,TCP_MAXSEG,(char *)(&size),retsz)<0)
    return -1;
  else 
    return size;
#else
  /* NCSA: Winsock does NOT support TCP_MAXSEG for getsockopt yet. */
  return -1;
#endif
}
int RawTCPport::getSendBufSize(int &size){
  int optlen=sizeof(int);
  return getSockOpt(SOL_SOCKET,SO_SNDBUF,&size,optlen);
}
int RawTCPport::getRecvBufSize(int &size){
  int optlen=sizeof(int);
  return getSockOpt(SOL_SOCKET,SO_RCVBUF,&size,optlen);
}
int RawTCPport::setReuseAddr(int on){
  return setSockOpt(SOL_SOCKET,SO_REUSEADDR,&on,sizeof(int));
}
int RawTCPport::setSendBufSize(int size){
  return setSockOpt(SOL_SOCKET,SO_SNDBUF,&size,sizeof(int));
}
int RawTCPport::setRecvBufSize(int size){
  return setSockOpt(SOL_SOCKET,SO_RCVBUF,&size,sizeof(int));
}
int RawTCPport::setKeepAlive(int on){
  return setSockOpt(SOL_SOCKET,SO_KEEPALIVE,(char *)(&on),sizeof(int));
}
int RawTCPport::setKeepAlive(SignalHandler handler){
  int on=1;
#ifndef WIN32
  signal(SIGPIPE,handler); // register the SIGPIPE handler
#else
  /* NCSA: Windows does not support SIGPIPE yet. Is this going to be a
     problem?? */
#endif
  // this handler will be called if the connection fails
  return setSockOpt(SOL_SOCKET,SO_KEEPALIVE,&on,sizeof(int));
}
int RawTCPport::setDebug(int on){
  return setSockOpt(SOL_SOCKET,SO_DEBUG,(char *)(&on),sizeof(int));
}

int RawTCPport::setLinger(int on){
  return setSockOpt(SOL_SOCKET,SO_LINGER,(char *)(&on),sizeof(int));
}

void RawTCPclient::start()
{
  if (connect()<=0)
    {
      reportError(); 
      perror("RawTCPclient::start() : socket connect failed");
    }
}

RawTCPclient::RawTCPclient(const char *host,int p)
: RawTCPport(host,p)
{
	printf("RawTCPclient(%s,%d)\n",host,p);
	start();
}

RawTCPclient::RawTCPclient(const char *host,int p,int optionflags)
:RawTCPport(host,p)
{
  // don't start... need to set some TCP options first
}

RawTCPclient::~RawTCPclient()
{
}

RawTCPserver::RawTCPserver(int pt,int q,int optionflags)
: RawTCPport(pt,q)
{
  // don't startup server connection
  // Use "startup to actually initiate
}

bool RawTCPserver::start()
{
  if (bind()>=0)
    {
      listen(); 
      return true;
    }
  else
    {
      reportError(); 
      perror("RawTCPserver: Bind failed"); 
      return false;
    } 
}

RawTCPserver::RawTCPserver(int pt,int q)
: RawTCPport(pt,q)
{
#ifndef WERNER_MODE
  start();
#else // WERNER_MODE
  // if start() failed (due to a already reserved address), 
  // increase port number until success (WB 2.10.98)
  int portcnt = 0;
  while( !start() && errno==EADDRINUSE && portcnt < 10){
    port += 2;
    printf("Address is already in use, trying port nr. %d\n", port );
    
    /// Same as in RawTCPport::RawTCPport()
      address.setAddress(port); // !!!!!!!!!!!!
#ifndef WIN32
      int	sock=socket(AF_INET,SOCK_STREAM,0);
#else
      SOCKET	sock=socket(AF_INET,SOCK_STREAM,0);
#endif
      setPort(sock);
  }
#endif
}

RawTCPserver::~RawTCPserver()
{
  // shutdown(port);??
}
#if 0
RawTCPconnection::RawTCPconnection(const char *host,int p)
: RawTCPport(host,p),server(0),connection(0)
{
  signal(SIGPIPE,SIG_IGN);  
  connection = new RawTCPclient(hostname,port);
  printf("Tried connect\n");
  if(!connection->isAlive()){
    printf("kill connection and begin Server\n");
    delete connection;
    connection = 0;
    server = new RawTCPserver(port);
    puts("began server");
    printf("server port=%u alive\n",server->getPortNum(),server->isValid());
    //server->setTCPnoDelay(); // nodelay before connect?
    connection = server->accept(); // wait for a connection
    printf("Accept completed");
    //connection->setTCPnoDelay(); // do this to accepted client sockets?
}

RawTCPconnection::~RawTCPconnection()
{
  // shutdown(port);??
}
#endif

FastTCPclient::FastTCPclient(const char *host,int p,int windowsize):
  RawTCPclient(host,p,windowsize)
{
  setTCPwindowSize(windowsize);
  setLinger();
  //setTCPnoDelay();
  start(); // start up the connection
}

FastTCPclient::~FastTCPclient(){}

FastTCPserver::FastTCPserver(int pt,int windowsize,int q):
  RawTCPserver(pt,q,windowsize)
{
  setTCPwindowSize(windowsize);
  setLinger();
  //setTCPnoDelay();
  start(); // start up the connection
}

FastTCPserver::~FastTCPserver(){}

