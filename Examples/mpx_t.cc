#include "RawMCAST.hh"
#include "RawUDP.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
using namespace std;
#include <sys/types.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>

#define _USE_BSD
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h> 


/*******************************************************
  This receives unicast packets on the unicast_server_port.
  The packets are then sent out to the multicast group as well
  as to a peer'ed unicast host.  
  
  The overall communication topology bridges two unicast vic/rat
  sessions (ie. the robot controller and the robot itself) with
  a multicast group (ie. an accessgrid virtual venue).

  There are a total of 4 processes launched to manage 
  the resulting traffic.  1 process is required to manage
  each unicast peer.  However, each of these unicast peers
  is in fact producing two data streams.  The primary data 
  stream for the vic/vat data is carried on even port numbers
  whereas the next consecutive odd-port number carries the
  associated RTP data.
********************************************************/
struct PacketExchanger_t {
  char *multicast_group;
  int multicast_port;
  char *unicast_host;
  int unicast_port;
  int unicast_server_port;
  int *done;
};

void *PacketExchanger(void *v){  
  PacketExchanger_t *t=(PacketExchanger_t*)v; 
  RawMCAST *mc=0;
  RawUDPclient *uc=0;
  RawUDPserver *us=0;
  
  mc = new RawMCAST(t->multicast_group,t->multicast_port);
  us = new RawUDPserver(t->unicast_server_port); // unicast receiver
  uc = new RawUDPclient(t->unicast_host,t->unicast_port);
  
  cerr << "Start\treceive from port " << t->unicast_server_port << 
    " and send to mcast" << t->multicast_group << '/' << t->multicast_port  <<
    "\t and to unicast" << t->unicast_host << '/' << t->unicast_port << endl;
  /* configure the multicast socket */
  mc->setTTL(127);
  // mc->setLoopback(0); // loopback enabled so that local AG can bcast the data
  mc->join(); // unbound.  We are a sender
  while(!t->done){ // has to get a packet to quit properly (ugh)
    char buffer[65556];
    int packetsize=0;
    IPaddress returnaddr;
    packetsize = us->recv(buffer,sizeof(buffer),returnaddr);
    mc->send(buffer,packetsize); // send to mc address
    // initially loopback to source
    if(uc) uc->send(buffer,packetsize);
  }
  mc->leave();
  delete mc;
  if(us) delete us;
  if(uc) delete uc;
  return 0;
}

int main(int argc,char *argv[]){
  char *unicast_host1,*unicast_host2,*mcast_grp;
  int unicast_port1,unicast_port2,mcast_port;
  int i,done=0;
  pthread_t thread[4];
  PacketExchanger_t threadargs[4];

  // pthread_set_concurrency(2);
  if(argc<4 ||
     !strchr(argv[1],'/') ||
     !strchr(argv[2],'/') ||
     !strchr(argv[3],'/')) {
    printf("Usage: %s mcast_addr/mcast_port host1/port1 host2/port2\n",
	   argv[0]);
    exit(0);
  }
  // parse mcast group/port
  mcast_grp=argv[1]; 
  mcast_port=atoi(strchr(argv[1],'/')+1); 
  *(strchr(argv[1],'/'))='\0';
  // parse first unicast group/port
  unicast_host1=argv[2]; 
  mcast_port=atoi(strchr(argv[2],'/')+1); 
  *(strchr(argv[2],'/'))='\0';
  // parse second unicast group/port
  unicast_host2=argv[3]; 
  mcast_port=atoi(strchr(argv[3],'/')+1); 
  *(strchr(argv[3],'/'))='\0';
  //========= now launch subthreads =========
  // main host2 to host1 link-------------
  threadargs[0].done=&done;
  threadargs[0].multicast_group = mcast_grp;
  threadargs[0].multicast_port  = mcast_port;
  threadargs[0].unicast_host    = unicast_host1;
  threadargs[0].unicast_port    = unicast_port1;
  threadargs[0].unicast_server_port = unicast_port2;
  pthread_create(&(thread[0]),NULL,PacketExchanger,(void*)(&(threadargs[0])));
  // main host 1 to host 2 link-----------
  threadargs[1].done=&done;
  threadargs[1].multicast_group = mcast_grp;
  threadargs[1].multicast_port  = mcast_port;
  threadargs[1].unicast_host    = unicast_host2;
  threadargs[1].unicast_port    = unicast_port2;
  threadargs[1].unicast_server_port = unicast_port1;
  pthread_create(&(thread[1]),NULL,PacketExchanger,(void*)(&(threadargs[1])));
  // RTP host2 to host1 link-------------
  threadargs[2].done=&done;
  threadargs[2].multicast_group = mcast_grp;
  threadargs[2].multicast_port  = mcast_port+1;
  threadargs[2].unicast_host    = unicast_host1;
  threadargs[2].unicast_port    = unicast_port1+1;
  threadargs[2].unicast_server_port = unicast_port2+1;
  pthread_create(&(thread[2]),NULL,PacketExchanger,(void*)(&(threadargs[2])));
  // RTP host 1 to host 2 link-----------
  threadargs[3].done=&done;
  threadargs[3].multicast_group = mcast_grp;
  threadargs[3].multicast_port  = mcast_port+1;
  threadargs[3].unicast_host    = unicast_host2;
  threadargs[3].unicast_port    = unicast_port2+1;
  threadargs[3].unicast_server_port = unicast_port1+1;
  pthread_create(&(thread[3]),NULL,PacketExchanger,(void*)(&(threadargs[3])));
  // signal capture in pthreads?
  // otherwise, wait for keyboard "quit" command
  // and set "done" for all subthreads
  for(i=0;i<4;i++){
    pthread_join(thread[i],NULL); // wait for completion
  }
}

