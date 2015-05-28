#include <RawTCP.hh>
#include <vatoi.hh>
#include <AlignedAlloc.hh>
#include <Timer.hh>
#include <stdio.h>

/*********************************************************************
program: DataClient [hostname] [-h <hostname>] [-p <portnumber>] 
                   [-w <tcp_windowsize>] [-s <data buffer size>]
purpose: Sends large blocks of data through a TCP socket to a 
         DataServer.  Used to test the affect of TCP window
	  sizes and receiver blocking sizes on network transfer 
	  performance.
usage: Works in conjunction with program DataServer.  You must
       run DataServer before you run the DataClient.  The following
       flags can be used
       -h <hostname>: Select a hostname to connect to.  This is
                      the host where the server is being run.
		        (default if you don't  specify a hostname is 
                      "localhost").
	-f : Use TCP_FASTACK on SGI systems
       -p <portnumber>: Select the portnumber to connect to on 
                        specified host.  The default is 7052.
	-s <buffersize>: Size of data to send to the target host in
	                 bytes.  It also understands the modifiers
			   'M' and 'K' to mean multiply by 1024*1024
			   and multiply by 1024 respectively.  (eg.
			   you can use -s 128k to specify that 128k
			   of data should be sent to the server.
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
       -r : Reflection mode.  This tells the Server to reflect
            the data back to the client to measure round trip
	     time.
       -v : verbose mode.  Indicates all data being received as
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
  long nbytes=1024*1024; // default send is 1meg
  char *buffer;
  const char *hostname="localhost";
  int port = 7052;
  int windowsize = -1;
  int sendblocksize = DEFAULTBLOCKSIZE;
  int recvblocksize = -1;
  int alignment=16*1024;
  int reflect=0;
  int verbose=0;
  int fastack=0;
  int recvpause,sendpause;
  RawTCPclient *Client;
  int i;

  // Parse commandline options.
  for(i=1;i<argc;i++){
    if(*(argv[i])=='-'){
      char *arg=argv[i]+1;
      char *param=argv[i+1];
      if(*arg=='h') { hostname=param; i++; } // set hostname
      else if(*arg=='p') { port=atoi(param); i++; } // set port number
      else if(*arg=='a') { alignment=vatoi(param); i++; } // set port number
      else if(*arg=='w') { windowsize=vatoi(param); i++; } // set TCP window size
      else if(*arg=='s') { nbytes=vatoi(param); i++; }     // set buffer size to send
      else if(*arg=='f') { fastack=1;}
      else if(*arg=='r') { reflect=1; } // turn reflection mode on
      else if(*arg=='b'){
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
    else
      hostname=argv[i];
  }
  
  // socket with big tcpwindows if specified.  Otherwise
  // open normal tcp socket.
  if(windowsize>0) {
    printf("Send %ld bytes to %s:%d using windowsize of %u\n",
	   nbytes,hostname,port,windowsize);
    Client = new FastTCPclient(hostname,port,windowsize);
  }
  else {
    printf("Send %ld bytes to %s:%u\n",nbytes,hostname,port);
    Client = new RawTCPclient(hostname,port);
  }
  if(fastack) Client->setTCPfastAck(1);
  // local variables for aquiring stats on transfer speed
  Timer stimer,rtimer,ttimer; // timers
  double rt,st,ut; // realtime systemtime usertime
	printf("write nbytes=%ld in %ld:%ld bytes\n",
	nbytes,sizeof(u_short),sizeof(u_long));

  // malloc buffer memory and initialize
  // Originally, it allocated and sent a buffer that was as
  // large as the send size... now it just uses blocksize
  // to determine buffer size.
  // allocate buffer for maximum size
  buffer=(char *)AlignedAlloc(nbytes,alignment,0);
  for(i=0;i<nbytes;i++) buffer[i]=i; // make certain it is all mapped
  // Tell the other side how many bytes are being sent to it	
  Client->write(nbytes); // send 4-byte package size
  
  // This next write acts to synchronize the client and server after malloc.
  Client->write(reflect); // tell server if it should reflect data
  
  printf("Begin writing %ld bytes data\n",nbytes);
  stimer.start(); ttimer.start();
  if(sendblocksize>0){
    // break the send up into smaller blocks
    int sent;
    for(sent=0;sent<nbytes;sent+=sendblocksize){
      int rv,sendsize;
      sendsize=sendblocksize;
      if(sendsize>(nbytes-sent)) sendsize=nbytes-sent;
      // bogus send because we aren't shifting the buffer by sent
      if(Client->write(buffer,sendsize)<0){ // this is to eliminate cache effects
	perror("error sending");
	break;
      }
    }
  }
  else
    i=Client->write(buffer,nbytes); // write all of the data out in one shot
  stimer.stop();  ttimer.stop();
  // Now if the reflect flag is true, we should be expecting the
  // data to come right back at us.
  if(reflect){ // get the return data
    long count,recvd; // variables for counting receive stats
    // Must receive TCP stream in pieces as they arrive.
    rtimer.start(); ttimer.start();
    for(count=0,recvd=0;recvd<nbytes;count++){
      long nrec;
      long rsize = nbytes-recvd;
      if(recvblocksize>0) // break the transfer up into smaller blocks
	if(rsize>recvblocksize) rsize=recvblocksize;
      // bogus recv because we aren't shifting the buffer by recvd
      nrec=Client->read(buffer,rsize); // this is to eliminate cache effects
      if(nrec<0) {
	printf("Read Error!!! terminating read\n");
	break;
      }
      if(verbose)
	fprintf(stderr,"nrec=%ld\n",nrec);
      recvd+=nrec;
      if(verbose)
	fprintf(stderr,"Received %ld of %ld.  %ld remain\n",recvd,nbytes,nbytes-recvd);
    }
    rtimer.stop(); ttimer.stop();
  }
  stimer.elapsedTimeSeconds(st,ut,rt);
  printf("Wrote %ld bytes in \n\tuser: %lfS\n\tsystem: %lfS\n\t real: %lfS\n\t%lf Kbytes/sec\n",
	 nbytes,ut,st,rt,(double)nbytes/(1000*rt));
  if(reflect){
    rtimer.elapsedTimeSeconds(st,ut,rt);
    printf("Got %ld bytes of data back from reflection\nReceive Times",nbytes);
    printf("\tuser: %lfS\n\tsystem: %lfS\n\t real: %lfS\n\t%lf Kbytes/sec\n",
	   ut,st,rt,(double)nbytes/(1000*rt));
    ttimer.elapsedTimeSeconds(st,ut,rt);
    printf("Total Time\n");
    printf("\tuser: %lfS\n\tsystem: %lfS\n\t real: %lfS\n\t%lf Kbytes/sec\n",
	   ut,st,rt,(double)(2*nbytes)/(1000*rt));
  }
  delete Client;
  delete buffer;
    return 1;
}
