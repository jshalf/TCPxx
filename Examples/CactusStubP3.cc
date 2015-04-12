#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef DARWIN
#include <iostream>
using namespace std;
#else
#include <stream.h>
#endif

#include <RawUDP.hh>
#include <RawTCP.hh>

#include "Timer.hh"
#include "Delay.hh"
#ifdef HAS_MPI
#include <mpi.h>
#endif

struct TCP_MessageBase {
  RawTCPport *sock; /* replace this with your socket data structure */
  TCP_MessageBase(RawTCPport *control_socket):sock(control_socket){}
    /* replace this with your own socket write routine 
       (to write out of the control socket) */
  virtual int write(char *seg,int segsize){
    if(!sock) return 0; // no socket
    return sock->write(seg,segsize);
  }
  /* replace this with your own socket Read routine */
  virtual int read(char *seg,int segsize){
    int n,total=0;
    if(!sock) return 0; // no socket
    for(total=0,n=0;total<segsize;total+=n) {
      n=sock->read(seg+total,segsize-total);
      if(n<0) return -1; // error
    }
    /* null-terminate the message (assumes buffer is large enough for this) */
    // seg[segsize]='\0';  bad juju on small page sizes
    return total;
  }
  
};

#ifdef HAS_MPI
struct MPI_MessageBase : public TCP_MessageBase {
  int master;
  MPI_Comm comm;
  MPI_MessageBase(RawTCPport *control_socket,MPI_Comm comm_world,int mstr):
    TCP_MessageBase(control_socket),comm(comm_world),master(mstr){
    /* master socket has been selected */
    /* use selectMaster to pre-test for a valid master socket */

  }
  static int selectMaster(MPI_Comm comm,int connection_ready){
    int i,nprocs;
    char *all;
    char local = connection_ready?1:0;
    MPI_Comm_size(comm,&nprocs);
    all = new char[nprocs];
    MPI_Allgather(&local,1,MPI_CHAR,all,1,MPI_CHAR,comm);
    /* now scan for master */
    for(i=0;i<nprocs;i++){
      if(all[i]>0) {
	delete all;
	return i;
      }
    }
    delete all;
    return -1; // no master found
  }
  /* broadcast key ints (like segsize) from the master */
  int getMaster() {return master;}
  int bcast(int segsize){
    /* get the proper size for the segment */
    MPI_Bcast(&segsize,1,MPI_INT,master,comm);
    return segsize; /* distribute to everyone for mem alloc */
  }
  virtual int read(char *seg,int segsize){
    int r=TCP_MessageBase::read(seg,segsize);
    if(r<0) *seg='*'; /* reserved char for errors */
    /* get from one and then bcast to all */
    MPI_Bcast(seg,segsize,MPI_CHAR,master,comm);
    /* then return (should do error checks here) */
    if(*seg=='*') return -1;
    else return segsize; /* it was OK (no errors) */
  }
  virtual int write(char *seg,int segsize){
    // just pass through to the superclass
    int r=TCP_MessageBase::write(seg,segsize);
    // make sure errors on master socket are broadcast to all
    MPI_Bcast(&r,1,MPI_INT,master,comm);
    return r;
  }
};

#endif

struct ProcInfo {
  int dims[3]; /* dims of the local domain */
  int origin[3];
  /* parse for ox,oy,oz:nx,ny,nz */
  int parse(char *msg){
    char *c;
    int i;
    // strsep would be better, but not widely available yet
    // strtok is unsafe.  sscanf was too brittle
    for(i=0;i<2;i++){
      c=strchr(msg,',');
      if(!c) { perror("Parse Procinfo Failed"); return -1;}
      *c='\0';
      origin[i]=atoi(msg);
      msg=c+1;
    }
    c=strchr(msg,':');
    if(!c) return -1;
    *c='\0';
    origin[2]=atoi(msg);
    msg=c+1;
    for(i=0;i<2;i++){
      c=strchr(msg,',');
      if(!c) { perror("Parse Procinfo Failed"); return -1;}
      *c='\0';
      dims[i]=atoi(msg);
      msg=c+1;
    }
    dims[2]=atoi(msg);
    return 128;
  }
  int serialize(char *msg){
    sprintf(msg,"%u,%u,%u:%u,%u,%u\n",
	    origin[0],origin[1],origin[2],
	    dims[0],dims[1],dims[2]);
    return 128;
  }
};

struct CactusMessage {
  int npes;
  int bytes_per_element;
  char byte_order;
  int dims[3],pdims[3];
  ProcInfo *pinfo;
  CactusMessage():npes(1){
    pinfo=new ProcInfo[1];
  }
  void setSize(int np){
    npes=np; 
    delete pinfo;
    pinfo=new ProcInfo[npes];
  }
  int getMessageSize(char *head){
    if(head){
      head[15]='\0';
      this->setSize(atoi(head));
    }
    return npes*128+1+1+16+128;
  }
  int parse(char *msg){
    msg[15]='\0';
    this->setSize(atoi(msg));
    msg+=16;
    bytes_per_element=(int)(msg[0]-'0'); 
    msg++;
    if(msg[0]=='b' || msg[0]=='B')
      byte_order='b';
    else if(msg[0]=='l' || msg[0]=='L') {
      byte_order='l';
    }
    else{
      puts("bogus byte order");
      byte_order=0;
    }
    msg++;
	/* needs to send out dims and pdims */
    sprintf(msg,"%u,%u,%u:%u,%u,%u",
	dims[0],dims[1],dims[2],
	pdims[0],pdims[1],pdims[2]);
	msg+=128;
    for(int i=0;i<npes;i++){
      msg+=(pinfo[i]).parse(msg);
    }
    return getMessageSize(0);
  }
  int serialize(char *msg){
    sprintf(msg,"%u",npes); /* write(msg,16); */
    msg+=16;
    sprintf(msg,"%c%c","0"+bytes_per_element,byte_order);
    msg+=2;
    for(int i=0;i<npes;i++){
      msg+=(pinfo[i]).serialize(msg);
    }
    return 16+2+128*npes;
  }
};

struct HostInfo {
  char *hostname;
  int port;
  int parse(char *msg){
    char *c;
    c=strchr(msg,':');
    if(!c) return -1; // error
    *c='\0';
    strcpy(hostname,msg);
    port=atoi(c+1);
    return 128;
  }
  int serialize(char *msg){
    /* send out in 128 byte section */
    sprintf(msg,"%s:%u",hostname,port);
    return 128;
  }
};

struct VisapultMessage {
  int npes; /* not part of Visapult Reply, but useful to 
	       know the number of entries in the reply */
  HostInfo *hosts;
  VisapultMessage():npes(1) { 
    hosts=new HostInfo[1];
  }
  void setSize(int np){ 
    npes = np; 
    delete hosts; 
    hosts = new HostInfo[npes];
  }
  int parse(char *msg){
    for(int i=0;i<npes;i++){
      msg+=hosts[i].parse(msg);
    }
    return 128*npes;
  }
  int serialize(char *msg){
    for(int i=0;i<npes;i++){
      msg+=hosts[i].serialize(msg);
    }
    return 128*npes;
  }
  int getMessageSize(char *head){
    // nothing to do with head
    return 128*npes;
  }
};

int main(int argc,char *argv[]){
  // wait for connection
  // Accept
  // protocol engine
  //   fills out datastructure?
  RawTCPport *tcp=0;
  TCP_MessageBase mbase(tcp);
  VisapultMessage vmsg;
  CactusMessage cmsg;
}

/* 

Procedure:

1) Cactus accepts a new backEnd TCP connection
2) Cactus sends
	nbytes   	:  message contents
	-----------	:  --------------------------
	16 bytes	:  <number of processors>
	1 bytes   :  <nbytes per element {4 or 8}>
	1 bytes   :  <byte order {L for Little Endian and B for Big Endian}> 
	128 bytes : <nx>,<ny>,<nz> : <px>,<py>,<pz> 
 where n * is the number of dimensions in each direction of the 
overall grid and p * is the number of processors in each direction  
for the domain decomposition of said grid  

	npes * 128 bytes :  <ox>,<oy>,<oz> <lnx>,<lny>,<lnz> 
o* is the logical origin for each block and ln* is the number 
of points for each block.  These blocks are numbered canonically  
as the number of PE's in the system 

3) backEnd replies to Cactus
	nbytes   			:  message contents
	---------------------	:  --------------------------
	npes * 128 bytes	:  <hostname> : <portnumber>
This list provides each CPU of the Cactus job the host and port 
 to send its local block of data to.  So the send pattern is completely  
defined by the backEnd (it can do round-robin assignment if it wants

4) Cactus will then start sending UDP data to the specified host:port  
pairs that the backEnd specified.  From here on, Cactus and visapult  
will exchange a token to each other via this TCP control port to test  
whether the connection is still alive and to share information like  
loss statistics.  So all tokens are shared using a 64byte  
fixed-length message. 
	* every 1-5 seconds, visapult should send a message  
containing current loss-rate estimates.  I have an algorithm  
for inferring this, but for now, we will just send 0. 
	* every 1-5 seconds Cactus will send a 64byte token  
	back to the backEnd.  That will also be an essentially  
	empty message -- until we can think of something sensible to send
*/
