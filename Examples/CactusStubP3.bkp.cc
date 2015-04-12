#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef DARWIN
#include <iostream>
using namespace std;
#else
#include <stream.h>
#endif

#include "RawUDP.hh"
#include "Timer.hh"
#include "Delay.hh"

struct MessageBase {
  RawPort *sock; /* replace this with your socket data structure */
  MessageBase(RawPort *control_socket):sock(control_socket){}
  /* replace this with your own socket Read routine */
  int read(char *seg,int segsize){
    int n,total=0;
    for(total=0,n=0;total<segsize;total+=n;) {
      n=sock->read(seg+total,segsize-total);
      if(n<0) return -1; // error
    }
    /* null-terminate the message (assumes buffer is large enough for this) */
    // seg[segsize]='\0';  bad juju on small page sizes
    return total;
  }
  /* replace this with your own socket write routine (to write out of the control socket) */
  int write(char *seg,int segsize){
    return sock->write(seg,segsize);
  }
};

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
      if(!c) { perror("Parse Procinfo Failed"); return;}
      *c='\0';
      origin[i]=atoi(msg);
      msg=c+1;
    }
    c=strchr(msg,':');
    if(!c) return;
    *c='\0';
    origin[2]=atoi(msg);
    msg=c+1;
    for(i=0;i<2;i++){
      c=strchr(msg,',');
      if(!c) { perror("Parse Procinfo Failed"); return;}
      *c='\0';
      dims[i]=atoi(msg);
      msg=c+1;
    }
    dims[2]=atoi(msg);
    return 128;
  }
  int serialize(char *msg){
    sprintf("%u,%u,%u:%u,%u,%u\n",
	    origin[0],origin[1],origin[2],
	    dims[0],dims[1],dims[2]);
    return 128;
  }
};

struct CactusMessage : public MessageBase {
  int npes;
  int bytes_per_element;
  char byte_order;
  int dims[3],pdims[3];
  ProcInfo *pinfo;
  CactusMessage(RawPort *p):npes(1),MessageBase(p){pinfo=new ProcInfo[1];}
  void setSize(int np){npes=np; delete pinfo[]; pinfo=new ProcInfo[npes];}
  void readMessage(){
    char msg[128];
    if(read(msg,16)>0) this->setSize(atoi(msg));
    if(read(msg,1)>0) {msg[1]='\0'; bytes_per_element=atoi(msg);}
    if(read(msg,1)>0) {
      if(msg[0]='b' || msg[0]=='B')
	byte_order='b';
      else if(msg[0]='l' || msg[0]='L')
	byte_order='l';
      else{
	puts("bogus byte order");
	byte_order=0;
      }
    }
    for(int i=0;i<npes;i++){
      pinfo[i]->read(this);
    }
  }
  void writeMessage(){
    char msg[128];
    sprintf(msg,"%u",npes); write(msg,16);
    sprintf(msg,"%c%c","0"+bytes_per_element,byte_order);
    for(int i=0;i<npes;i++){
      pinfo[i]->write(this);
    }
  }
};

struct HostInfo {
  char *hostname;
  int port;
  int parse(char *msg){
    char *c;
    c=strchr(msg,':');
    if(!c) return; // error
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
  VisapultMessage(RawPort *p):npes(1),MessageBase(p) { 
    hosts=new HostInfo[1];
  }
  void setSize(int np){ 
    npes = np; 
    delete hosts[]; 
    hosts = new HostInfo[npes];
  }
  int parse(char *msg){
  }
  int serialize(char *msg){
  }
  int getMessageSize(){
    return 128*npes;
  }
  void getMessage(char *msg){
    /* have to have setsize first */
    this->read(
    for(int i=0;i<npes;i++){
      hosts[i]->read(this);
    }
  }
  void sendMessage(){
    for(int i=0;i<npes;i++){
      hosts[i]->write(this);
    }
  }
};

int main(int argc,char *argv[]){
  // wait for connection
  // Accept
  // protocol engine
  //   fills out datastructure?
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
