#include "vatoi.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

long long vatoi(char *p){
  char param[128];
  strcpy(param,p);
  char *offset;
  long long n;
  if((offset=strchr(param,'k')) || (offset=strchr(param,'K')) ){
    *offset='\0';
    n=1024;
  }
  else if((offset=strchr(param,'m')) || (offset=strchr(param,'M')) ){
    *offset='\0';
    n=1024*1024;
  }
  else if((offset=strchr(param,'g')) || (offset=strchr(param,'G')) ){
    *offset='\0';
    n=1024*1024*1024;
  }
  else n=1;
  n*=atoi(param);
  return n;
}

double vatof(char *p){
  char param[128];
  strcpy(param,p);
  char *offset;
  double n;
  if((offset=strchr(param,'k')) || (offset=strchr(param,'K')) ){
    *offset='\0';
    n=1024.0;
  }
  else if((offset=strchr(param,'m')) || (offset=strchr(param,'M')) ){
    *offset='\0';
    n=1024.0*1024.0;
  }
  else if((offset=strchr(param,'g')) || (offset=strchr(param,'G')) ){
    *offset='\0';
    n=1024.0*1024.0*1024.0;
  }
  else n=1.0;
  n*=atof(param);
  return n;
}
