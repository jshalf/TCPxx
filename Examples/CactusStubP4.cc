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

/*

Alternative,
read in the data as a single buffer, 
then broadcast the results,
then parse the completed buffer.

*/

class MPI_Message {
  int master,pid;
  read(int size){
    if(pid==master){
      readSock(msg,size);
    }
    MPIbcast(msg,size);
    parse(msg,size);
  }

  readserial(int size){
    readSock(msg,size);
    parse(msg,size);
  }
  {
