/*
TestCommandProcessor <-s>
-s is an optional parameter that tells it to be the source of the 
data sends.  Otherwise, it will sit passively and recieve the data.
*/
#include <stdio.h>
#include <stdlib.h>
#include "CommandProcessor.hh"

void main(int argc,char *argv[]){
  if(argc<2){
    perror("specify sender or receiver");
    exit(0);
  }
  
  if(*(argv[1])=='s'){
    Command c("isosurface[0]","gxx","1.035");
    CommandSender s("localhost",7052);
    s.sendCommand(c);
  }
  else {
    Command c;
    CommandReceiver r("localhost",7052);
    int i=0;
    while(!r.getCommand(c))
      fprintf(stderr,"Waiting for command %5u\r",i++);
    printf("got command [%s]:[%s]:[%s]\n",c.operation,c.object,c.value);
  }
}
