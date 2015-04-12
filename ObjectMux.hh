#ifndef __OBJECTMUX_H
#define __OBJECTMUX_H
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
#include "OrderedList.H"

/*=============================================================
class:   ObjectMux
purpose: Hides complexity of using the select() system call
         to multiplex a set of objects indexed by the ports
         they contain.  So we can associate any arbitrary 
         object with the port selection.  This could have
         been templated, but using void* is simply a lazy
         way of acheiving the same effect.
===============================================================*/
class ObjectMux {
private:
  struct Object {
  	void *object;
  	RawPort *port;
  	Object(RawPort *p,void *o):object(o),port(p){}
  };
  //OrderedList<RawPort *> ports;
  OrderedList<Object *> inportlist,outportlist,exceptportlist;
  struct timeval timeout;
  //fd_set inports,outports,exceptions;
  int nfds;
  int enabletimeout;
public:
  ObjectMux();
  ~ObjectMux();
 
  void addInport(RawPort *port,void *object); // watches port for input availible
  void addOutport(RawPort *port,void *object); // watches port for output ready
  void addPortException(RawPort *port,void *object); // watches for errors/exceptions
  void add(RawPort *port,void *object); // watches for activity on a port in all categories

  /// Remove associated object from the input port list
  void delInport(void *object); 
  /// Remove associated object from the output port list
  void delOutport(void *object); 
  /// Remove associated object from the exception port list
  void delException(void *object); 
  /// Remove associated object from all port lists
  void del(void *object); 

  // All of the functions below are various ways of setting
  // blocking timeout for select() (how long it will wait for activity)
  // Timeout must be enabled using the enableTimeout() method.
  // Otherwise, the default is to block indefinitely
  void setTimeoutSeconds(long seconds);
  void setTimeoutMicroseconds(long useconds); 
  void setTimeout(long sec,long usec);
  void setTimeout(struct timeval t);
  void enableTimeout(int et=1);

  // Select will return the the next availible port that
  // is in need of servicing.  It will return NULL if select()
  // times out without finding an active port.
  int poll(); // just poll to see how many ports are ready
  RawPort *selectPort(); // actually get next availible port
  void *selectObject();
};

#endif // __OBJECTMUX_HH_
