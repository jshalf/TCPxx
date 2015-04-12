#include "RawTCP.hh"
#include "PortMux.hh"
#include <iostream>
#include "stdio.h"

int main() {
  char buffer[128];
  int portnumber = 7777;  // port 7777
  RawTCPserver server(portnumber);
  RawTCPport *client=0;
  PortMux newconnection;
  newconnection.addInport(&server);
  int cnt = 0,work=0;
  while(cnt <= 100) {  // accept 100 connections
    cerr << "Work "  << work++ << '\r';
    if(newconnection.poll()){
      cnt++;
      cout << endl;  
      cout << cnt << ": Accepting...." << endl;
      // we have a new client that wants to connect
      if(client) delete client; // close old client
      client = server.accept(); // accept new client
      client->read(buffer,128);
      cout << buffer << "\n";
    }
  }
  return 0;
}
