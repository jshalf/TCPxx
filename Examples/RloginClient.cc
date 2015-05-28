#include "RloginClient.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
Implements basic rlogin protocol
	Does not trap ctrl-c
	Does not trap ctrl-z
	does not trap '~' (if you send it, you lose the connection)
*/

#define DEBUG


#ifdef DEBUG
// #define debugprint(x) puts(x)
void debugprint(const char *s) { fprintf(stderr,"%s\n",s);}
#else
#define debugprint(x)
#endif

void RloginClient::login(const char *name,const char *localname,const char *termtype){
  char zero = 0;
  if(!localname) localname=name;
  debugprint("send 0");
  write(&zero,1); // send initial 0 
  int rv;
  do {
    printf("Ack retval=%u\n",rv=read(&zero,1)); // get return ack.
    printf("Ack value = %c\n",zero);
  } while(rv>0);
  debugprint("sendname and termtype");
  write((void*)name,strlen(name)+1);
  write((void*)localname,strlen(localname)+1); // should be uid
  write((void*)termtype,strlen(termtype)+1);
  debugprint("senda zero");
  //read(&zero,1); // get return ack.
  do {
    printf("Ack retval=%u\n",rv=read(&zero,1)); // get return ack.
    printf("Ack value = %c\n",zero);
  } while(rv>0);
}
void RloginClient::sendPasswdInClear(const char *passwd){
  // scan for Password:
  const char *matchstring="sword:";
  int matchlength=0;
  char c;
  debugprint("match passwd");
  while(matchlength<6){
    read(&c,1); // read a char
    if(c==matchstring[matchlength]) matchlength++;
    else matchlength=0; // restart matching
  }
  debugprint("passwd matched");
  write((void *)passwd,strlen(passwd));
  debugprint("passwd sent");
  c='\n';
  write(&c,1); // complete the line
}
#ifdef KRB5
void RloginClient::sendPasswdKerberos5(const char *passwd){
}
#endif
void RloginClient::init(const char *name,const char *passwd){
  debugprint("init");
  CEscape='~';
  CPause=0x10;
  CResume=0x20;
  SFlush=0x02;
  SStartFlow=0x20;
  SStopFlow=0x10;
  SWindowSize=0x80;
  CCmd[0]=CCmd[1]=0xff; CCmd[2]='\0';
  strcpy(CWindowSize,"ss");
  paused=0;
  debugprint("login");
  login(name);
  debugprint("sendpasswd");
  if(passwd) sendPasswdInClear(passwd);
  debugprint("init done");
}

RloginClient::RloginClient(const char *hostname,const char *name,const char* passwd):
  RawTCPclient(hostname,513){
    // login=513
    // telnet=23
    // nterm=1026
    // klogin=543 // kerberso authenticated
    printf("init host=%s name=%s\n",hostname,name);
    init(name,passwd);
}
RloginClient::~RloginClient(){
  write(&CEscape,1);
} // send EOF to rlogin server?
	
int RloginClient::handleCommand(char &c){
  if(c==SFlush){
    // flush the brute-force way
    while(nonblock.poll()) read(&c,1);
  }
  else if(c==SStopFlow){
    // disable flow control?
    if(paused<2) paused+=2;
  }
  else if(c==SStartFlow){
    // enable flow control?
    if(paused==2) paused=0;
    else if(paused==3) paused=1;
    else paused=0;
  }
  else if(c==SWindowSize){
    // send windowsize info
    // really shouldn't be necessary though
    write(CCmd,strlen(CCmd));
    write(CWindowSize,strlen(CWindowSize));
    // send 16bytes 
    // rows,cols,xpix,ypix
    // fails on T3e because short=32bits
    short rows=24,cols=80,xpix=0,ypix=0;
    // TCP++ converts shorts automatically
    // to network byte order
    write(rows);
    write(cols);
    write(xpix);
    write(ypix);
  }
  return 0;
}
int RloginClient::getChar(){
  // use select to block
  char c=0;
  while(!block.select()){
    if(read(&c,1)<=0) return -1;
    if(!handleCommand(c)) break;
  }
  return c;
}
int RloginClient::getCharNonBlock(){
  char c;
  while(!nonblock.poll()){
    if(read(&c,1)<=0) break;
    if(!handleCommand(c)) return (int)c;
  }
  return -1;
}
int RloginClient::put(char c){
  return write(&c,1);
}
int RloginClient::put(char *c,int size){
  return write(c,size);
}
int RloginClient::pause(){
  if(paused) return -1;
  paused=1;
  return write(&CPause,1);
}
int RloginClient::resume(){
  if(!paused || paused>1) return -1;
  paused=0;
  return write(&CResume,1);
}

