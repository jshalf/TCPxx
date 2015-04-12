#ifndef __PORTMUX_H
#define __PORTMUX_H
#ifdef WIN32
#include "TCPXXWinDLLApi.h"
#endif

// std headers
#include <stdlib.h>

#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#else
#include <windows.h>
#endif
#include <sys/types.h>
// Local Headers
#include "RawPort.hh"

#ifndef WIN32
#ifdef USE_STL
#include <list>
#endif
#include <iostream>
using namespace std;
#else
#include <list>
#include <iostream>
#endif
#ifdef USE_STL
#if defined(__STL_USE_NAMESPACES)
typedef std::list<RawPort*> PortList;
#else
typedef list<RawPort*> PortList;
#endif

#else // !USE_STL
#include "OrderedList.H"
typedef OrderedList<RawPort*> PortList;
#endif// !USE_STL

/*=============================================================
class:   PortMux
purpose: Hides complexity of using the select() system call
         to multiplex a set of socket or file ports.
===============================================================*/
class TCPXX_API PortMux {
private:
  PortList inportlist,outportlist,exceptportlist;

  struct timeval timeout;

  int nfds;
  int enabletimeout;
public:
  PortMux();
  ~PortMux();
 
  void addInport(RawPort *port); // watches port for input availible
  void addOutport(RawPort *port); // watches port for output ready
  void addPortException(RawPort *port); // watches for errors/exceptions
  void add(RawPort *port); // watches for activity on a port in all categories

  void removeInport(RawPort *port); // remove port from input if found
  void removeOutport(RawPort *port); // remove port from input if found
  void removeExceptionport(RawPort *port); // remove port from input if found
  void remove(RawPort *port); // remove port from any category where it is found

  // All of the functions below are various ways of setting
  // blocking timeout for select() (how long it will wait for activity)
  // Timeout must be enabled using the enableTimeout() method.
  // Otherwise, the default is to block indefinitely
  void setTimeoutSeconds(long seconds);
  void setTimeoutMicroseconds(long useconds); 
  void setTimeout(long sec,long usec);
  void setTimeout(struct timeval t);
  void setTimeout(double t);
  void enableTimeout(int et=1);

  // Select will return the the next availible port that
  // is in need of servicing.  It will return NULL if select()
  // times out without finding an active port.
  int poll(); // just poll to see how many ports are ready
  RawPort *wait() {return this->select();}
  RawPort *select(); // actually get next availible port
};

#endif
