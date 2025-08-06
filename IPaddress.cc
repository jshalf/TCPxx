#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#ifndef WIN32
#include <unistd.h>
#include <iostream>
using namespace std;
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#else
#include <windows.h>
#endif

#include <sys/types.h>

#include "IPaddress.hh"

void IPaddress::name2addr(){
  //cerr << "convert name2addr" << endl;
#ifndef WIN32
  memset((caddr_t)&sin,0,sizeof(sin));
#else
  memset(&sin, 0, sizeof(sin));
#endif
  //cerr << " Hostname" << hostname << endl;
  he=gethostbyname(hostname);
  if(!he){
    // bigtime error!  Host lookup failed!
    reportError();
    *hostname='\0'; // null it out
    flags &= (~ADDRVALID);
    return;
  }
  sin.sin_family=AF_INET;
  sin.sin_port=htons(port);
  // bcopy((he->h_addr),&(sin.sin_addr),he->h_length);
  memcpy(&(sin.sin_addr), he->h_addr, he->h_length);
  sinlen = sizeof(sin);
  //cerr << "IPaddress::name2addr() size= " << sinlen << endl;
  flags|=ADDRVALID;
  //cerr << "Address2name: " << hostname << ':' << port << "\n";
}

void IPaddress::setlocaladdr(int pt){
  port=pt;
#ifndef WIN32
  memset((caddr_t)&sin,0,sizeof(sin));
#else
  memset(&sin, 0, sizeof(sin));
#endif
  sin.sin_family=AF_INET;
  sin.sin_port=htons(port);
  sin.sin_addr.s_addr=INADDR_ANY;
  *hostname=0;
  sinlen=sizeof(sin);
  flags=ADDRVALID;
  // puts("setlocaladdr()");
}

void IPaddress::addr2name(){
  //cerr << "addr2name" << endl;
  port=ntohs(sin.sin_port);
  // now lookup hostname
#ifndef WIN32
  he=gethostbyaddr((caddr_t)&(sin.sin_addr),sizeof(struct in_addr),AF_INET);
#else
  he=gethostbyaddr((const char*) &(sin.sin_addr),sizeof(struct in_addr),AF_INET);
#endif
  // choose first name from list (even though this ain't correct
  if(he){
    strcpy(hostname,he->h_name);
  }
  else {
    //cerr << "IPaddress::addr2name(): no hostent" << endl;
    *hostname=0;
    
  }
  if(*hostname==0){// just use IP address
    unsigned char *c;
    unsigned int arry = (unsigned int)(sin.sin_addr.s_addr);
    c=(unsigned char*)((void*)(&arry));
    snprintf(hostname,sizeof(hostname),"%u.%u.%u.%u",(unsigned int)(c[0]),
	    (unsigned int)(c[1]),(unsigned int)(c[2]),(unsigned int)(c[3]));
  }
  flags|=NAMEVALID;
  // printf("Address2name: %s:%u\n",hostname,port);
}

IPaddress::IPaddress(){ // could init to localhost
  //bzero((caddr_t)&sin,sizeof(sin));
#ifndef WIN32
  memset((caddr_t)&sin,0,sizeof(sin));
#else
  memset(&sin,0,sizeof(sin));
#endif
  *hostname=0;
  port=0;
  he=0;
  flags=0;
}

IPaddress::IPaddress(const char *host,int pt){
  //cerr << "IPaddress::constructor(): host=" << host << " : port=" << port << endl;
  setAddress(host,pt);
  flags=NAMEVALID;
}

IPaddress::IPaddress(int pt){
  // INADDR_ANY ???
  setlocaladdr(pt);
  // addr valid (and sort of namevalid)
}

/* potential problem:
   sockaddr_in->sin_addr is a member which is of type in_addr.
   This is what MCAST needs.  Need to see if other things need this
*/
IPaddress::IPaddress(sockaddr_in &sin){
  setAddress(sin);
  flags=ADDRVALID;
}

IPaddress::IPaddress(IPaddress &ipaddr){
  this->setAddress(ipaddr);
}

IPaddress &IPaddress::operator=(IPaddress &ipaddr){
  if(&ipaddr!=this)
    this->setAddress(ipaddr);
  return *this;
}

void IPaddress::getHostname(char *name,int size){
  if(!(flags&NAMEVALID)) 
    addr2name();
  strncpy(name,hostname,size);
}
void IPaddress::getHostname(char *name){
  if(!(flags&NAMEVALID)) 
    addr2name();
  strcpy(name,hostname);
}

int IPaddress::getPort(){
  if(!(flags&NAMEVALID))
    addr2name();
  return port;
}

void IPaddress::getAddress(sockaddr_in *addr){
  if(!(flags&ADDRVALID))
    name2addr();
  memcpy(addr,&sin,sizeof(sockaddr_in));
}

void IPaddress::getAddress(in_addr *a){
  
  if(!(flags&ADDRVALID))
    name2addr();
  a->s_addr = sin.sin_addr.s_addr;
}
sockaddr_in *IPaddress::getSockaddrPtr(){
  if(!(flags&ADDRVALID))
    name2addr();
  return &sin;
} 

sockaddr *IPaddress::getSocketAddress()
{
  if(!(flags&ADDRVALID))
    name2addr();

  return reinterpret_cast<sockaddr*>(&sin);
} 

void IPaddress::setAddress(sockaddr_in &fsin,int flen){
  flags=ADDRVALID;
  //bcopy((caddr_t)&fsin,(caddr_t)&sin,flen);
#ifndef WIN32
  memcpy((caddr_t)&sin, (caddr_t)&fsin, flen);
#else
  memcpy(&sin, &fsin, flen);
#endif
  sinlen = flen;
}

void IPaddress::setAddress(const char *host,int pt){
  flags=NAMEVALID;
  if(host)
    strcpy(hostname,host);
  else
#ifndef WIN32
    cerr << "IPaddress::setAddress(): Error! Null Host name." << endl;
#else
  fprintf(stderr, "IPaddress::setAddress(): Error! Null Host name.\n");
  fflush(stderr);
#endif
  port=pt; 
}

void IPaddress::setAddress(int pt){
  setlocaladdr(pt);
  flags=ADDRVALID;
}


