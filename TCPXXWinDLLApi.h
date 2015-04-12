/***************************************************************
 *
 * $Id: TCPXXWinDLLApi.h,v 1.1 2000/02/18 00:35:14 jshalf Exp $
 *
 * $Log: TCPXXWinDLLApi.h,v $
 * Revision 1.1  2000/02/18 00:35:14  jshalf
 *
 * Initial checking to repository.
 *
 * Revision 1.1  2000/01/19 13:23:51  werner
 * Port to Windows NT
 *
 * Revision 1.1  1999/11/25 11:26:20  bzfgrafs
 * .
 *
 * Revision 1.1  1999/07/29 17:31:32  bzfstall
 * More windows stuff...
 *
 *
 *
 ***************************************************************/
#ifndef TCPXX_WIN_DLL_API_H
#define TCPXX_WIN_DLL_API_H

#ifdef _WIN32
#  ifdef TCPXX_EXPORTS
#     define TCPXX_API __declspec(dllexport)
#  else
#     define TCPXX_API __declspec(dllimport)
#  endif
#else
#   define TCPXX_API 
#endif

#endif
