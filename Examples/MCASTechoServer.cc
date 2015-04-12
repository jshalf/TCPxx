#include <stdio.h>
#include <stdlib.h>
#ifdef DARWIN
#include <iostream>
using namespace std;
#else
#include <stream.h>
#endif

#include "RawMCAST.hh"

/* Implements connectionless UDP echo service */

int main(int argc,char *argv[]){
  RawMCAST echo("224.2.177.155",55524);
  echo.bind();
  echo.join();
  while(1){
    char buffer[128],machname[128];
    IPaddress returnaddr;
    echo.recv(buffer,128,returnaddr);
    cout << "received: " << buffer << endl;
    echo.send(buffer,strlen(buffer)+1,returnaddr);
    cout << "echo back to source" << endl;
  }
  echo.leave();
  return 1;
}
