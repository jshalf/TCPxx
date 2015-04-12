#include <stdlib.h>

// Headers for select()
#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#else
#include <windows.h>
#endif
#include <sys/types.h>

#ifdef AIX
#include <strings.h>
#endif

// Local Headers
#include "ObjectMux.hh"
#include "RawPort.hh"
#include "OrderedList.H"

ObjectMux::ObjectMux(){
  nfds=0;
  enabletimeout=1;
  timeout.tv_sec=0;
  timeout.tv_usec=0;
}

ObjectMux::~ObjectMux(){
	Object *object; 
	// need to delete all objects
	// would be better if this was automatic
	// but this will have to do for now
	FOREACH(object,inportlist)
		delete object;
	FOREACH(object,outportlist)
		delete object;
	FOREACH(object,exceptportlist)
		delete object;
}

void ObjectMux::add(RawPort *port,void *object){
  inportlist.add(new Object(port,object));
  outportlist.add(new Object(port,object));
  exceptportlist.add(new Object(port,object));
}

void ObjectMux::addInport(RawPort *port,void *object){
  inportlist.add(new Object(port,object));
}

void ObjectMux::addOutport(RawPort *port,void *object){
  outportlist.add(new Object(port,object));
}

void ObjectMux::addPortException(RawPort *port,void *object){
  exceptportlist.add(new Object(port,object));
}


void ObjectMux::delInport(void *fObject)
{
Object *elem; 

	FOREACH(elem,inportlist)
	{
		if (elem->object == fObject)
		{
			inportlist.del_last();
			break;
		}
	}
}

void ObjectMux::delOutport(void *fObject)
{
Object *object; 

	FOREACH(object,outportlist)
	{
		if (object->object == fObject)
		{
			outportlist.del_last();
			break;
		}
	}
}

void ObjectMux::delException(void *fObject)
{
Object *object; 

	FOREACH(object,exceptportlist)
	{
		if (object->object == fObject)
		{
			exceptportlist.del_last();
			break;
		}
	}
}

void ObjectMux::del(void *object)
{
	delInport(object);
	delOutport(object);
	delException(object);
}


void ObjectMux::setTimeoutSeconds(long seconds){
  // must create timeval
  timeout.tv_sec=seconds;
  //timeout.tv_usec=0; // shouldn't be zeroing usec
  enabletimeout=1;
}

void ObjectMux::setTimeoutMicroseconds(long useconds){
  //timeout.tv_sec=0;
  timeout.tv_usec=useconds;
  enabletimeout=1;
}

void ObjectMux::setTimeout(long sec,long usec){
  timeout.tv_sec=sec;
  timeout.tv_usec=usec; // shouldn't be zeroing usec
  enabletimeout=1;
}

void ObjectMux::setTimeout(struct timeval t){
  timeout=t; // will it copy properly?
}

void ObjectMux::enableTimeout(int et){
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
int ObjectMux::poll(){
  int status;
  Object *p;
  struct timeval tmr;
  fd_set inports,outports,exceptions;

  tmr.tv_sec=0;
  tmr.tv_usec=0;
  nfds=FD_SETSIZE; // the lazy way to get nfds
  FD_ZERO(&inports);
  FD_ZERO(&outports);
  FD_ZERO(&exceptions);

  FOREACH(p,outportlist)
    FD_SET(p->port->getPortNum(),&outports);
  FOREACH(p,inportlist)
    FD_SET(p->port->getPortNum(),&inports);
  FOREACH(p,exceptportlist)
    FD_SET(p->port->getPortNum(),&exceptions);

  status=::select(nfds,&inports,&outports,&exceptions,&tmr);
  if(status>0) return status;
  else return 0;
}

RawPort *ObjectMux::selectPort(){
  int status;
  Object *p;
  fd_set inports,outports,exceptions;
  nfds=FD_SETSIZE; // the lazy way to get nfds
  FD_ZERO(&inports);
  FD_ZERO(&outports);
  FD_ZERO(&exceptions);

  FOREACH(p,outportlist)
    FD_SET(p->port->getPortNum(),&outports);
  FOREACH(p,inportlist)
    FD_SET(p->port->getPortNum(),&inports);
  FOREACH(p,exceptportlist)
    FD_SET(p->port->getPortNum(),&exceptions);

  if(enabletimeout){
    //puts("Polled Select");
    status=::select(nfds,&inports,&outports,&exceptions,&timeout);
  }
  else{
    puts("Blocking on Select");
    status=::select(nfds,&inports,&outports,&exceptions,0);
  }
  if(status>0) {
    FOREACH(p,exceptportlist)
      if(FD_ISSET(p->port->getPortNum(),&exceptions))
	return p->port;
    FOREACH(p,inportlist)
      if(FD_ISSET(p->port->getPortNum(),&inports))
	return p->port;
    FOREACH(p,outportlist)
      if(FD_ISSET(p->port->getPortNum(),&outports))
	return p->port;
  }
  return 0;
}

void *ObjectMux::selectObject(){
  int status;
  Object *p;
  fd_set inports,outports,exceptions;
  nfds=FD_SETSIZE; // the lazy way to get nfds
  FD_ZERO(&inports);
  FD_ZERO(&outports);
  FD_ZERO(&exceptions);

  FOREACH(p,outportlist)
  {
    FD_SET(p->port->getPortNum(),&outports);
    printf("Listening to outport %d\n", p->port->getPortNum() );
  }
  FOREACH(p,inportlist)
  {
    FD_SET(p->port->getPortNum(),&inports);
    printf("Listening to inport %d\n", p->port->getPortNum() );
  }
  FOREACH(p,exceptportlist)
    FD_SET(p->port->getPortNum(),&exceptions);

  if(enabletimeout){
    //puts("Polled Select");
    status=::select(nfds,&inports,&outports,&exceptions,&timeout);
  }
  else{
    puts("Blocking on Select");
    status=::select(nfds,&inports,&outports,&exceptions,0);
  }
  if(status>0) {
    FOREACH(p,exceptportlist)
      if(FD_ISSET(p->port->getPortNum(),&exceptions))
	return p->object;
    FOREACH(p,inportlist)
      if(FD_ISSET(p->port->getPortNum(),&inports))
	return p->object;
    FOREACH(p,outportlist)
      if(FD_ISSET(p->port->getPortNum(),&outports))
	return p->object;
     puts("ObjectMux::selectObject(): Warning: object associated with select() not found!");
  }
  perror("ObjectMux::selectObject()");
  return 0;
}

