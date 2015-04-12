#ifndef IPADDRESS_H
#define IPADDRESS_H

#include "TCPXXWinDLLApi.h"

#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#else
#include <windows.h>
#endif

#include <sys/types.h>

#include "ErrorReport.hh"

//#if !defined(LINUX) && !defined(AIX) && !defined(DARWIN)
//typedef size_t socklen_t;
//#endif
//=======
//#if !defined(linux) && !defined(AIX) && !defined(SGI)
//typedef size_t socklen_t;
//#endif

#define FIRST_AVAIL -1 // get first availible port
/*======================================================
class: IPaddress
purpose: Encapsulates socket address information 
         and all of logic to convert either direction 
	 between addresses and names.
========================================================*/
class TCPXX_API IPaddress : public ErrorReport
{
private: 
  sockaddr_in sin;
  int sinlen;
  int port;
  hostent *he;
  char hostname[64];
  int flags; // which fields are valid
#define NAMEVALID 0x01
#define ADDRVALID 0x02
  void name2addr(); // fill out name info from sockaddr info
  void addr2name(); // fill out sockaddr info from name info
  void setlocaladdr(int port); // sets address to be localhost
public:
  /* Various ways of setting the IP address on construction
     you can also set the IP address at any time during
     the life of the class using setAddress() methods */
  IPaddress();
  IPaddress(const char *host,int pt);
  IPaddress(sockaddr_in &sin);
  IPaddress(int pt); // opens on localhost
  // copy constructor and operator
  IPaddress(IPaddress &ipaddr); // copy constructor
  IPaddress &operator =(IPaddress &ipaddr);
  /* A variety of ways of setting the socket address after
   construction. */
  void setAddress(sockaddr_in &fsin,int flen=sizeof(struct sockaddr_in));
  void setAddress(const char *host,int pt); 
  void setAddress(int pt); // localhost IPANY
  inline void setAddress(IPaddress &addr){setAddress(*(addr.getSockaddr_in()));} 

  // Query methods...............
  /** gets the hostname and copys it into the string you
     provide.  The "size" argument indicates the maximum
     length of the string "name". */
  void getAddress(sockaddr_in *addr);
  void getAddress(sockaddr_in &addr){ getAddress(&addr);}
  void getAddress(in_addr &addr){getAddress(&addr);}
  void getAddress(in_addr *addr);
  void getHostname(char *name,int size);
  /** Same thing as above, but for lazy people aren't
     worried about buffer overruns */
  void getHostname(char *name);
  /** Gets the IP port number.  This is not to be
     confused with a file descriptor which is encapsulated
     by the RawPort class. */
  int getPort();
  /** Some things need this nasty sockaddr_in... */
  sockaddr_in *getSockaddrPtr();
  sockaddr *getSocketAddress();
  inline sockaddr_in *getSockaddr_in(){return getSockaddrPtr();}
  inline sockaddr *getSockaddr(){return getSocketAddress();}
  inline socklen_t getSockaddrSize(){return (socklen_t)(sizeof(sockaddr));}
  inline socklen_t getSockaddr_inSize(){return (socklen_t)(sizeof(sockaddr));}
  inline in_addr getIn_Addr(){return (this->getSockaddrPtr()->sin_addr);}

  /* Also necessary for unencapsulating the sockaddr */
  // inline int getSockaddrSize() {
  //if(!(flags & ADDRVALID)) name2addr();
  //return sinlen;}
  
};
    

#endif
