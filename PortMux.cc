#include <stdlib.h>
#include <math.h>

// Headers for select()
#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#endif
#include <sys/types.h>

#ifdef AIX
#include <strings.h>
#endif

// Local Headers
#include "PortMux.hh"
#include "RawPort.hh"

#ifdef USE_STL
#ifdef	_CRAYMPP
#include <algo.h>
#else
#include <algorithm>
#endif

#else
#include "OrderedList.H"
#endif

#if defined(__STL_USE_NAMESPACES) || defined(WIN32)
using namespace std;
#endif

PortMux::PortMux(){
  /*FD_ZERO(&inports);
  FD_ZERO(&outports);
  FD_ZERO(&exceptions);*/
  nfds=0;
  enabletimeout=1;
  timeout.tv_sec=0;
  timeout.tv_usec=0;
}

PortMux::~PortMux(){
}

void PortMux::add(RawPort *port){
#ifdef USE_STL
  inportlist.push_back(port);
  outportlist.push_back(port);
  exceptportlist.push_back(port);
#else
	inportlist.add(port);
	outportlist.add(port);
	exceptportlist.add(port);
#endif
}

void PortMux::addInport(RawPort *port){
#ifdef USE_STL
  inportlist.push_back(port);
#else
	inportlist.add(port);
#endif
}

void PortMux::addOutport(RawPort *port){
#ifdef USE_STL
  outportlist.push_back(port);
#else
	outportlist.add(port);
#endif
}

void PortMux::addPortException(RawPort *port){
#ifdef USE_STL
  exceptportlist.push_back(port);
#else
	exceptportlist.add(port);
#endif
}

void PortMux::removeInport(RawPort *port)
{
	inportlist.remove(port);
}

void PortMux::removeOutport(RawPort *port)
{
	outportlist.remove(port);
}

void PortMux::removeExceptionport(RawPort *port)
{
	exceptportlist.remove(port);
}

void PortMux::remove(RawPort *port)
{
	inportlist.remove(port);
	outportlist.remove(port);
	exceptportlist.remove(port);
}

void PortMux::setTimeoutSeconds(long seconds){
  // must create timeval
  timeout.tv_sec=seconds;
  //timeout.tv_usec=0; // shouldn't be zeroing usec
  enabletimeout=1;
}

void PortMux::setTimeoutMicroseconds(long useconds){
  //timeout.tv_sec=0;
  timeout.tv_usec=useconds;
  enabletimeout=1;
}

void PortMux::setTimeout(long sec,long usec){
  timeout.tv_sec=sec;
  timeout.tv_usec=usec; // shouldn't be zeroing usec
  enabletimeout=1;
}

void PortMux::setTimeout(struct timeval t){
  timeout=t; // will it copy properly?
}

void PortMux::setTimeout(double v){
  struct timeval r;
  r.tv_sec = (int64_t)floor(v);
  r.tv_usec = (int64_t)((v - floor(v))*(double)1000000.0);
  this->setTimeout(r);
}

void PortMux::enableTimeout(int et){
  enabletimeout=et;
}

/*
  This is currently not a fair algorithm.  Lower order ports
  will get served to the exclusion of the higher order ones.
  Need to put in a fairness procedure to increase the rank 
  of unused ports or return an OrderedList of availible
  ports to process. (have select() and selectList())
  OrderedList will require a copy constructor...
 */
int PortMux::poll(){
  int status;
  struct timeval tmr;
  fd_set inports,outports,exceptions;

  tmr.tv_sec=0;
  tmr.tv_usec=0;
  nfds=FD_SETSIZE; // the lazy way to get nfds
  FD_ZERO(&inports);
  FD_ZERO(&outports);
  FD_ZERO(&exceptions);

  

#ifdef USE_STL
  PortList::iterator it;
  for(it=outportlist.begin(); it!=outportlist.end(); it++)
    FD_SET( (*it)->getPortNum(),&outports);

  for(it=inportlist.begin(); it!=inportlist.end(); it++)
    FD_SET( (*it)->getPortNum(),&inports);

  for(it=exceptportlist.begin(); it!=exceptportlist.end(); it++)
    FD_SET( (*it)->getPortNum(),&exceptions);
#else
  RawPort *port;
  FOREACH(port,outportlist)
    FD_SET( port->getPortNum(),&outports);

  FOREACH(port,inportlist)
    FD_SET( port->getPortNum(),&inports);

  FOREACH(port,exceptportlist)
    FD_SET( port->getPortNum(),&exceptions);
#endif

  status=::select(nfds,&inports,&outports,&exceptions,&tmr);
  if(status>0) return status;
  else return 0;
}

struct	CheckPort
{
	fd_set&ports;

	CheckPort(fd_set&Ports) : ports(Ports) {}

	bool operator()(RawPort*p) { return FD_ISSET(p->getPortNum(),&ports); }
};



RawPort *PortMux::select(){
  int status,portnum;
  RawPort *port=0;
  fd_set inports,outports,exceptions;
  nfds=0; // the lazy way to get nfds (FD_SETSIZE)
  FD_ZERO(&inports);
  FD_ZERO(&outports);
  FD_ZERO(&exceptions);

#ifdef USE_STL
  PortList::iterator it;
  for(it=outportlist.begin(); it!=outportlist.end(); it++){
    portnum=(*it)->getPortNum();
    if(portnum>nfds) nfds=portnum;
    FD_SET( (*it)->getPortNum(),&outports);
  }

  for(it=inportlist.begin(); it!=inportlist.end(); it++){
    portnum=(*it)->getPortNum();
    if(portnum>nfds) nfds=portnum;
    FD_SET( (*it)->getPortNum(),&inports);
  }

  for(it=exceptportlist.begin(); it!=exceptportlist.end(); it++){
    portnum=(*it)->getPortNum();
    if(portnum>nfds) nfds=portnum;
    FD_SET( (*it)->getPortNum(),&exceptions);
  }
#else
 
  FOREACH(port,outportlist){
    portnum=port->getPortNum();
    if(portnum>nfds) nfds=portnum;
    FD_SET( portnum,&outports);
  }

  FOREACH(port,inportlist){
    portnum=port->getPortNum();
    if(portnum>nfds) nfds=portnum;
    FD_SET( portnum,&inports);
  }

  FOREACH(port,exceptportlist){
    portnum=port->getPortNum();
    if(portnum>nfds) nfds=portnum;
    FD_SET( portnum,&exceptions);
  }
#endif
  nfds++; // must be one more than the max fds
  if(enabletimeout){
    struct timeval tmr;
    //puts("Blocking on Select");
    tmr.tv_sec=timeout.tv_sec;
    tmr.tv_usec=timeout.tv_usec;
    status=::select(nfds,&inports,&outports,&exceptions,&tmr);
  }
  else{
    //puts("Polled Select");
    status=::select(nfds,&inports,&outports,&exceptions,0);
  }
#ifdef USE_STL
  if(status>0) {
    it = find_if( exceptportlist.begin(), exceptportlist.end(), CheckPort(exceptions) );
    if (*it && it!=exceptportlist.end() ) return *it;

    it = find_if( inportlist.begin(), inportlist.end(), CheckPort(inports) );
    if (*it && it!=inportlist.end() ) return *it;

    it = find_if( outportlist.begin(), outportlist.end(), CheckPort(outports) );
    if (*it && it!=outportlist.end() ) return *it;
  }
#else
  if(status>0){
    FOREACH(port,exceptportlist)
      if(port && FD_ISSET(port->getPortNum(),&exceptions))
        return port;
    FOREACH(port,inportlist)
      if(port && FD_ISSET(port->getPortNum(),&inports))
        return port;
    FOREACH(port,outportlist)
      if(port && FD_ISSET(port->getPortNum(),&outports))
        return port;
  }
#endif
  return 0;
}


