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

/* Implements connectionless UDP echo service */
struct CommandLine { 
  char host[64];
  int port;
  int isserver;
  int dims;
  void init(){
    strcpy(host,"localhost");
    port=7000;
    isserver=0; //usually client
  }
  CommandLine(){init();}
  CommandLine(int argc,char *argv[]){
    init();
    parse(argc,argv);
  }
  void parse(int argc,char *argv[]){
    for(int i=1;i<argc;i++){
      if(*(argv[i])=='-'){
	char *arg=argv[i]+1;
	char *param=argv[i+1];
	if(*arg=='p') { port=atoi(param); i++; } // set port number
	else if(*arg=='h') { strcpy(host,param); i++; }
	else if(*arg=='s'); { isserver=1; }
	else if(*arg=='d'); { dims=atoi(param);}
	
      }
    }
  }
};

void server(IPaddress destaddress,int port){
  RawUDPserver echo(port);
  while(1){
    int sz;
    char buffer[128],machname[128];
    IPaddress returnaddr;
    // Marghoob: Replace this with the reciever for raw packets
    // perhaps add a poll for recv.
    sz=echo.recv(buffer,128,returnaddr);
    cout << "received: " << buffer << " : sz=" << sz << endl;
    echo.send(buffer,strlen(buffer)+1,returnaddr);
    cout << "echo back to source" << endl;
  }
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stream.h>

#include "RawUDP.hh"

void client(char *host,int port){
  RawUDPclient echo(host,port); // implict connect
  
  while(1){
    char buffer[128];
    int i;
    
    puts("prompt:");
    gets(buffer);
    i=echo.write(buffer,strlen(buffer)+1);
    *buffer=0;
    i=echo.read(buffer,128);
    printf("\nEcho returned %u bytes.  string=[%s]\n",i,buffer);
  }
}

void main(int argc,char *argv[]){
  CommandLine cmdln(argc,argv);
  if(cmdln.isserver) server(cmdln.port);
  else client(cmdln.host,cmdln.port);
}
