#ifndef __ERRRORREPORT_HH_
#define __ERRRORREPORT_HH_
#include <errno.h>
#if defined(__linux) || defined(__ppc__)
#include <stdio.h>     // for sys_errlist[]
#endif
#ifdef WIN32
#include <windows.h>
#endif
#include <string.h>
//extern int errno;
//extern char *sys_errlist[];

class ErrorReport 
{
	int nerrors;
	int errornumber;
static	char nullerror[1];

public:
	ErrorReport():errornumber(0),nerrors(0){}
#ifndef WIN32
	void reportError(int err=errno){ // can fail for multithreaded programs
#else
	  void reportError(int err=WSAGetLastError()){
#endif
		errornumber=err;
		nerrors++; // if we ever want to store an error stack
	}

	void clearError(){ nerrors=errornumber=0; }
	int getError(){return errornumber;}
	int numErrors() { return nerrors; }
	int failed(){return errornumber;}

	const char *errorString()
	{
#if defined(NO_STRERROR)
	if(errornumber) return sys_errlist[errornumber];
#elif !defined(WIN32)
       if(errornumber) return strerror(errornumber);
#endif /* we have no solution for WIN32 syserrors as of yet */
		return nullerror;
	}
};
#endif // __ERRRORREPORT_HH_

