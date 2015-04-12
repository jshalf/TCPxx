#include "RawTCP.hh"
#include "stdio.h"
#include <iostream>
using namespace std;

int main(int argc,char *argv[]) {
  char buffer[128];
  char *hostname="localhost";
  if(argc==2)
    hostname=argv[1];
  
  RawTCPclient *Client = new RawTCPclient(hostname,7052);
  sprintf(buffer,"Hi There");
  Client->write(buffer,128);
  Client->read(buffer,128);
  cout << buffer << "\n";
  return 1;
}
