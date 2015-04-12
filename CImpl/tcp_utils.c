/* tcp_utils.c */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/errno.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include "tcp_utils.h"

/* #if defined(ANSI) || defined(__STDC__) */
#ifdef __STDC__
/*
u_long htonl(u_long hostlong);
u_short htons(u_short hostshort);
u_long ntohl(u_long netlong);
u_short ntohs(u_short netshort);
 
int inet_aton(const char *cp, struct in_addr *pin);
unsigned long inet_addr(const char *cp);
unsigned long inet_network(const char *cp);
char *inet_ntoa(struct in_addr in);
struct in_addr inet_makeaddr(int net, int lna);
unsigned long inet_lnaof(struct in_addr in);
unsigned long inet_netof(struct in_addr in);
*/
#else
u_short htons(); 
u_short ntohs();
u_long inet_addr();
#endif

extern int errno;

int TCPOpenClientSock(char *hostname,int port)
{
  struct hostent *phe;
  /* struct servent *pse; */
  struct protoent *ppe;
  struct sockaddr_in sin;
  int s /*,type*/;
  
  bzero((char *)&sin, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr=INADDR_ANY;
  sin.sin_port = htons((u_short)port);
  if(phe=gethostbyname(hostname))
    bcopy(phe->h_addr, (char *)&sin.sin_addr, phe->h_length);
  else if((sin.sin_addr.s_addr = inet_addr(hostname)) == INADDR_NONE)
    fprintf(stderr,"can\'t find host %s \n",hostname);
  if((ppe=getprotobyname("tcp")) == 0)
    {
      perror("can\'t find tcp protocol\n");
      return -1;
    }
  s=socket(PF_INET,SOCK_STREAM,ppe->p_proto);
  if(s<0)
    {
      perror("");
      fprintf(stderr,"couldn\'t allocate socket on host %s: port %u\n",hostname,port);
      return s;
    }
  if(connect(s,(struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
      perror("");
      fprintf(stderr,"couldn\'t connect to host %s: port %u\n",hostname,port);
      return -1;
    }
  return s;
}

int UDPOpenClientSock(char *hostname,int port)
{
  struct hostent *phe;
  /*        struct servent *pse; */
  struct protoent *ppe;
  struct sockaddr_in sin;
  int sock/*,type*/;
  
  bzero((char *)&sin, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr=INADDR_ANY;
  sin.sin_port = htons((u_short)port);
  if(phe=gethostbyname(hostname))
    bcopy(phe->h_addr, (char *)&sin.sin_addr, phe->h_length);
  else if((sin.sin_addr.s_addr = inet_addr(hostname)) == INADDR_NONE)
    fprintf(stderr,"can\'t find host %s \n",hostname);
  if((ppe=getprotobyname("udp")) == 0)
    {
      perror("can\'t find udp protocol\n");
      return -1;
    }
  sock=socket(PF_INET,SOCK_DGRAM,ppe->p_proto);
  if(sock<0)
    {
      perror("");
      fprintf(stderr,"couldn\'t allocate socket on host %s: port %u\n",hostname,port);
      return sock;
    }
  if(bind(sock,&sin,sizeof(sin)) <0)
    {
      close(sock);
      perror("client: bind failed");
      return -1;
    }
  
  
  /*        if(connect(s,(struct sockaddr *)&sin, sizeof(sin)) < 0)
	    {`
	    perror("");
	    fprintf(stderr,"couldn\'t connect to host %s: port %u\n",hostname,port);
	    return -1;
	    }
  */
  return sock;
}

int TCPOpenServerSock(int port)
{
  /*	struct servent *pse; */
  struct protoent *ppe;
  struct sockaddr_in sin;
  int s/*,type*/;
  bzero((char *)&sin, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons((u_short)port);
  if((ppe=getprotobyname("tcp")) == 0)
    {
      perror("can\'t find tcp protocol\n");
      return -1;
    }
  s=socket(PF_INET,SOCK_STREAM,ppe->p_proto);
  if(s<0)
    {
      fprintf(stderr,"couldn\'t create socket on port %u\n",port);
      return s;
    }
  if(bind(s,(struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
      fprintf(stderr,"couldn\'t bind to port %u\n",port);
      return s;
    }
  if(listen(s,5) < 0) /* note, server connection qlen fixed to 5 */
    fprintf(stderr,"couldn\'t listen on port %u\n",port);
  return s;
}

int UDPOpenServerSock(int port)
{
  /* struct servent *pse; */
  struct protoent *ppe;
  struct sockaddr_in sin;
  int s/*,type*/;
  bzero((char *)&sin, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons((u_short)port);
  if((ppe=getprotobyname("udp")) == 0)
    {
      perror("can\'t find udp protocol\n");
      return -1;
    }
  s=socket(PF_INET,SOCK_DGRAM,ppe->p_proto);
  if(s<0)
    {
      fprintf(stderr,"couldn\'t create socket on port %u\n",port);
      return s;
    }
  if(bind(s,(struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
      fprintf(stderr,"couldn\'t bind to port %u\n",port);
      return s;
    }
  return s;
}

TCPBlockingWrite(int fd,char *buffer, int buflen)
{
  register int n;
  int nstore;
  n=write(fd,buffer,buflen);
  if(n>=0) return n;
  else switch(errno) /* note use of global variable errno */
    {
    case EBADF:
#ifdef PERRORS
      perror("invalid file descriptor");
#endif
      break;
    case EPIPE:
#ifdef PERRORS
      perror("attemped to write to an unconnected socket");
#endif
      break;
    case EFBIG:
#ifdef PERRORS
      perror("datasize too large to write.");
      perror("will attempt recovery by sending in smaller pieces");
#endif
      /* subdivide buffer and call TCPBlockingWrite() recursively */
      nstore=n; /* preserve register variable */
      TCPBlockingWrite(fd,buffer,buflen>>1);
      TCPBlockingWrite(fd,buffer+(buflen>>1),buflen-(buflen>>1));
      n=nstore; /* restore register variable */
      break;
    case EFAULT:
#ifdef PERRORS
      perror("invalid buffer address");
#endif
      break;
    case EINVAL:
#ifdef PERRORS
      perror("file descriptor points to unusable device.");
#endif
      break;
    case EIO:
#ifdef PERRORS
      perror("an io error occured");
#endif
      break;
    case EWOULDBLOCK:
#ifdef PERRORS
      perror("Non-blocking I/O is specified by ioctl");
      perror("but socket has no data (would have blocked).");
#endif
      break;
    }
  return n; /* default, don't know what happened */
}

int TCPBlockingRead(int fd,char *buffer, int buflen)
{
  /* do errno only if types.h has been included */
  /* how can I tell if this has been included ? */
  register int n,accum=0;
  while((n=read(fd,buffer,buflen)) > 0)
    {
      if(n>=0)
	{
	  buffer = buffer + n;
	  buflen -= n;
	  accum+=n;
	}
    }
#ifdef PERRORS
  if(n<0) switch(errno) /* note use of global variable errno */
    {
    case EBADF:
#ifdef PERRORS
      perror("invalid file descriptor");
#endif
      break;
    case EFAULT:
#ifdef PERRORS
      perror("invalid buffer address");
#endif
      break;
    case EINTR:
#ifdef PERRORS
      perror("operation interrupted by a signal");
      perror("disable signals and try again");
#endif
      break;
    case EWOULDBLOCK:
#ifdef PERRORS
      perror("Non-blocking I/O is specified by ioctl");
      perror("but socket has no data (would have blocked).");
#endif
      break;
    }
#endif
  if(n<0) 
    return n;
  else
    return accum;
}
