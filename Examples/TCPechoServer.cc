#include "RawTCP.hh"
#include <iostream>
using namespace std;
#include "stdio.h"

int main() {
  char buffer[128];
  RawTCPserver ServerPort(7052);
  RawTCPport *Server;

  Server = ServerPort.accept();

  Server->read(buffer,128);
  cout << buffer <<"\n";
  sprintf(buffer,"Howdy, Partner!");
  Server->write(buffer,128);
  return 1;
}
