#include "NewString.hh"
#include <stdio.h>
#include <string.h>

char *NewString(char *s)
{
	char *ns;
	if(!s) return 0;
	ns = new char[strlen(s)+1];
	if(ns) strcpy(ns,s);
	return ns;
}
