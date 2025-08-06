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

int main(int argc,char *argv[]){
  RawUDPclient echo("localhost",7000); // implict connect
  
  while(1){
    char buffer[128];
    int i;
    
    puts("prompt:");
    fgets(buffer,sizeof(buffer),stdin);// gets(buffer);
    i=echo.write(buffer,strlen(buffer)+1);
    *buffer=0;
    i=echo.read(buffer,128);
    printf("\nEcho returned %u bytes.  string=[%s]\n",i,buffer);
  }
  return 0;
}

