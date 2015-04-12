#include <stdio.h>
#include <stdlib.h>
#ifdef DARWIN
#include <iostream>
using namespace std;
#else
#include <stream.h>
#endif

#include "RawUDP.hh"

/* Implements connectionless UDP echo service */

int main(int argc,char *argv[]){
  RawUDPserver echo(7000);
  while(1){
    int sz;
    char buffer[128],machname[128];
    IPaddress returnaddr;
    sz=echo.recv(buffer,128,returnaddr);
    cout << "received: " << buffer << " : sz=" << sz;
    cout << " : srcport=" << returnaddr.getPort();
    returnaddr.getHostname(machname);
    cout << " : srchost=" << machname << endl;
    
    echo.send(buffer,strlen(buffer)+1,returnaddr);
    cout << "echo back to source" << endl;
  }
  return 1;
}
