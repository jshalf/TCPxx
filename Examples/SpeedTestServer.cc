#include "RawTCP.hh"
#include "vatoi.hh"
#include "Timer.hh"
#include "AlignedAlloc.hh"
#include <stdio.h>
/*********************************************************************
program: DataServer [-v] [-p <portnumber>] [-w <tcp_windowsize>] [-b <blocksize>]
purpose: Waits for DataClient connection and then receives 
         large blocks of data through a TCP socket from the
         DataClient.  Used to test the affect of TCP window
	  sizes and receiver blocking sizes on network transfer 
	  performance.
usage: Works in conjunction with program DataClient.  You must
       run DataServer before you run the DataClient.  The DataServer
       receives info about how much data it will be receiving from
       the DataClient and automatically configures itself accordingly.  
       The following flags can be used
       -p <portnumber>: Select the portnumber to connect to on 
                        local host.  The default is 7052.
	-f : use TCP_FASTACK on SGI systems
	-bs -br -b:  Sets the blocksize for decomposing a large send
                    into smaller packages. 
		    -bs sets the blocksize for sends
		    -br sets a blocking size for recvs (may not be useful)
		    -b sets the blocking size for both sends and recvs
		    If any are given a value of -1, then blocking is turned off.
		    It also understands the modifiers
			'M' and 'K' to mean multiply by 1024*1024
			and multiply by 1024 respectively.  (eg.
			you can use -s 128k to specify that 128k
			of data should be sent to the server.
	-w <tcp_windowsize>: Sets the TCP windowsize for the 
	                 socket connection.  Used for networks
			   with a high bandwidth-delay product.
	-v :  verbose mode.  Indicates all data being received as
	      it is being received.  This hurts performance, but it
	      is useful for debugging.
	-ds -dr -d:  Adds a delay into the send loop, receive loop, 
	     or both respectively.  This is to cover the possibility
	     that the OS needs a small pause in the loops in order
	     to process the TCP packages effectively.  Unfortunately
	     this uses sginap() which is only availible on sgi...
	     (currently disabled).
	     Perhaps this should be replaced with a "select".
	     But this increases system call overhead.
*********************************************************************/
#define DEFAULTBLOCKSIZE 64*1024 /* 64k default blocks */
int main(int argc,char *argv[]) {
  char *buffer;
  int port = 7052;
  int alignment=16*1024;
  int windowsize = -1;
  int sendblocksize = DEFAULTBLOCKSIZE; /* 16k blocksize == pagesize */
  int recvblocksize = -1;
  int verbose=0;
  int reflect=0;
  int fastack=0;
  long nbytes;  
  RawTCPserver *ServerPort;
  RawTCPport *Server;
  int recvpause,sendpause;
  int i;

  // Parse commandline arguments
  for(i=1;i<argc;i++){
    if(*(argv[i])=='-'){
      char *arg=argv[i]+1;
      char *param=argv[i+1];
      if(*arg=='p') { port=atoi(param); i++; } // set port number
      if(*arg=='a') { alignment=vatoi(param); i++; } // set memory alignment
      if(*arg=='f') { fastack=1; }
      else if(*arg=='w') { windowsize=vatoi(param); i++; } // set TCP window size
      else if(*arg=='b') {
	if(arg[1]=='s')      { sendblocksize=vatoi(param); i++; } // set send buffer block sizeg
	else if(arg[1]=='r') { recvblocksize=vatoi(param); i++; } // set recv buffer block sizeg
	else { sendblocksize=recvblocksize=vatoi(param); i++; }   // set both
      }      
      else if(*arg=='v') { verbose=1; }
      else if(*arg=='d'){
	if(arg[1]=='s')      { sendpause=vatoi(param); i++; } // set send buffer block sizeg
	else if(arg[1]=='r') { recvpause=vatoi(param); i++; } // set recv buffer block sizeg
	else { sendpause=recvpause=vatoi(param); i++; }   // set both
      }
    }
  }
  // Open the Server Socket
  if(windowsize>0){
    printf("Opening Server on localhost to port %u with windowsize %u\n",port,windowsize);
    ServerPort = new FastTCPserver(port,windowsize);
  }
  else {
    printf("Opening Server on localhost to port %u\n",port);
    ServerPort = new RawTCPserver(port);
  }
  // Catch socket open failure
  if(!ServerPort->isAlive()){
    printf("Server socket creation failed\n");
    exit(0); // exit somewhat ungracefully
  }
  if(fastack) ServerPort->setTCPfastAck(1);
  puts("Wait for connection");
  Server = ServerPort->accept();
  puts("Client Connected");
  i=Server->read(nbytes); // read number of bytes that are coming over the wire
  printf("Read %u bytes\n",nbytes);
  buffer=(char *)AlignedAlloc(nbytes,alignment,0); // would it be aligned better if I malloc'ed a long?
  for(i=0;i<nbytes;i++) buffer[i]=0; // make certain it is all mapped
  i=Server->read(reflect);
  printf("Reflect= %u bytes (retcode=%u)\n",reflect,i);
  long count,recvd; // variables for counting receive stats
  Timer rtimer,stimer,ttimer;
  double rt,st,ut;
  printf("Await receive of %u bytes\n",nbytes);
  ttimer.start(); rtimer.start(); // Start timing
  // Must receive TCP stream in pieces as they arrive.
  for(count=0,recvd=0;recvd<nbytes;count++){
    long nrec;
    long rsize = nbytes-recvd;
    if(recvblocksize>0) // break the transfer up into smaller blocks
      if(rsize>recvblocksize) rsize=recvblocksize;
    nrec=Server->read(buffer,rsize);
    if(nrec<0) {
	printf("Read Error!!! terminating read\n");
	break;
    }
    if(verbose)
      fprintf(stderr,"nrec=%d\n",nrec);
    recvd+=nrec;
    if(verbose)
      fprintf(stderr,"Received %d of %d.  %d remaining\n",recvd,nbytes,nbytes-recvd);
  }
  ttimer.stop(); rtimer.stop(); // stop timing
  if(reflect){
    ttimer.start(); stimer.start(); // start timing
    if(sendblocksize<=0)
      Server->write(buffer,nbytes); // send all of the data back
    else {
      int total;
      for(total=0;total<nbytes;total+=sendblocksize){
	int sendsize;
	sendsize=sendblocksize;
	if(sendsize>(nbytes-total)) sendsize=nbytes-total;
        if(Server->write(buffer,sendsize)<0){
		printf("Send Failure... exiting\n");
		break;
	}
      }
    }
    ttimer.stop(); stimer.stop(); // stop timing
  }
  rtimer.elapsedTimeSeconds(st,ut,rt);
  printf("%u bytes received in %u reads\nReceive Times",recvd,count);
  printf("\tuser: %lfS\n\tsystem: %lfS\n\t real: %lfS\n\t%lf Kbytes/sec\n",
	 ut,st,rt,(double)nbytes/(1000*rt));
  if(reflect){
    stimer.elapsedTimeSeconds(st,ut,rt);
    printf("Reflected %u bytes of data\nSend Times",nbytes);
    printf("\tuser: %lfS\n\tsystem: %lfS\n\t real: %lfS\n\t%lf Kbytes/sec\n",
	   ut,st,rt,(double)nbytes/(1000*rt));
    ttimer.elapsedTimeSeconds(st,ut,rt);
    printf("Total Time\n");
    printf("\tuser: %lfS\n\tsystem: %lfS\n\t real: %lfS\n\t%lf Kbytes/sec\n",
	   ut,st,rt,(double)(2*nbytes)/(1000*rt));
  }
  // Shut everything down
  delete Server;
  delete ServerPort;
  delete buffer;
}

