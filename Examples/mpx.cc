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
#include <setjmp.h>       

#define _USE_BSD
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>   

static jmp_buf env;
static int proc[4];

void subproc_quit(int i){
  puts("subproc_killed");
  longjmp(env,1);
}

void master_quit(int i){
  puts("killed");
  for(int i=0;i<4;i++)
    if(proc[i]>0){
      kill(proc[i],SIGINT);
      wait4(proc[i],0,0,0);
      proc[i]=0;
    }
  longjmp(env,1);
}

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
void PacketExchanger(char *multicast_group, int multicast_port,
		     char *unicast_host,int unicast_port,
		     int unicast_server_port){  
  int done=0;  
  RawMCAST *mc=0;
  RawUDPclient *uc=0;
  RawUDPserver *us=0;
  
  signal(SIGINT,subproc_quit);
  mc = new RawMCAST(multicast_group,multicast_port);
  us = new RawUDPserver(unicast_server_port); // unicast receiver
  uc = new RawUDPclient(unicast_host,unicast_port);
  
  cerr << "Start\treceive from port " << unicast_server_port << 
    " and send to mcast" << multicast_group << '/' << multicast_port  <<
    "\t and to unicast" << unicast_host << '/' << unicast_port << endl;
  /* configure the multicast socket */
  mc->setTTL(127);
  // mc->setLoopback(0); // loopback enabled so that local AG can bcast the data
  mc->join(); // unbound.  We are a sender
  done = setjmp(env); // returns 0 initially and 1 after the longjmp
  while(!done){
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
}

int main(int argc,char *argv[]){
  char *unicast_host1,*unicast_host2,*mcast_grp;
  int unicast_port1,unicast_port2,mcast_port;
  int i,done=0;
  if(setjmp(env)) exit(0); // catch premature death
  for(i=0;i<4;i++) proc[i]=0;
  if(argc<4 ||
     !strchr(argv[1],'/') ||
     !strchr(argv[2],'/') ||
     !strchr(argv[3],'/')){
    printf("Usage: %s mcast_addr/mcast_port host1/port1 host2/port2\n",argv[0]);
    exit(0);
  }
  mcast_grp=argv[1]; mcast_port=atoi(strchr(argv[1],'/')+1); *(strchr(argv[1],'/'))='\0';
  unicast_host1=argv[2]; mcast_port=atoi(strchr(argv[2],'/')+1); *(strchr(argv[2],'/'))='\0';
  unicast_host2=argv[3]; mcast_port=atoi(strchr(argv[3],'/')+1); *(strchr(argv[3],'/'))='\0';
  signal(SIGINT,master_quit); 
  for(i=0;i<4;i++){
    proc[i]=fork();
    if(proc[i]==0) {
      switch(i) {
      case 0: /* host2 to host1 link */
	PacketExchanger(mcast_grp,mcast_port,
			unicast_host1,unicast_port1,
			unicast_port2);
	exit(0); // exit on return
	break;
      case 1: /* host1 to host2 link */
	PacketExchanger(mcast_grp,mcast_port,
			unicast_host2,unicast_port2,
			unicast_port1);
	exit(0); // exit on return
	break;
      case 2: /*RTP: host2 to host1 link */
	PacketExchanger(mcast_grp,mcast_port+1,
			unicast_host1,unicast_port1+1,
			unicast_port2+1);
	exit(0); // exit on return
	break;
      case 3: /*RTP: host1 to host2 link */
	PacketExchanger(mcast_grp,mcast_port+1,
			unicast_host2,unicast_port2+1,
			unicast_port1+1);
	exit(0); // exit on return
	break;
      }
    }
    else if(proc[i]<0){
      perror("Fork Failed... must shut down");
      master_quit(1);
      exit(0);
    }
  }
  // now master watches for child processes to die
  done=setjmp(env);
  while(!done){
    wait3(0,0,0); // sleep until killed
  }
}
