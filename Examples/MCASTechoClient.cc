#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef DARWIN
#include <iostream>
using namespace std;
#else
#include <stream.h>
#endif

#include "RawMCAST.hh"

int main(int argc,char *argv[]){
  RawMCAST echo("224.2.177.155",55524); // implict connect
  echo.join();
  while(1){
    char buffer[128];
    int i;
    
    puts("prompt:");
    gets(buffer);
    // must use send and recv because sock is unbound
    i=echo.send(buffer,strlen(buffer)+1);
    *buffer=0;
    i=echo.recv(buffer,128);
    printf("\nEcho returned %u bytes.  string=[%s]\n",i,buffer);
  }
  return 1;
}

