#include "RawTCP.hh"
#include "stdio.h"
#include <iostream>
using namespace std;

int main(int argc,char *argv[]) {
  char buffer[128];
  const char *hostname="localhost";
  RawTCPclient *client=0;
  if(argc==2)
    hostname=argv[1];
  for (int i=1; i<=100; i++) {
    cout << i << ": Creating a client... " << endl;
    if(client) delete client; // close old connection
    client = new RawTCPclient(hostname,7777);
    snprintf(buffer,sizeof(buffer),"%d: Hi There...", i);
    client->write(buffer,128);
    sleep(1);    
  }
}

