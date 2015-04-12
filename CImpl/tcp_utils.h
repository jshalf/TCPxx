/* #if defined(ANSI) || defined(__STDC__) */
#ifdef __STDC__
/*
  TCP utilities library (1991) 
	John Shalf VPI&SU 
  Last modification Dec 1995
*/
/*==================================================================
** name: OpenClientSockTCP
** purpose: Given the hostname of the server and the portnumber of the
**	service to connect to, it opens a TCP socket to the host and returns
**	the file descriptor of the socket.
** parameters:
**	hostname: the name of the server host as a character string (not ip number)
**	port: The port number for the server.
** returns:
**	integer file descriptor for low level io.  Can convert to FILE * 
**	using the fdopen() system call.
**=================================================================*/
int TCPOpenClientSock(char *hostname, int port);
int UDPOpenClientSock(char *hostname, int port);

/*==================================================================
** name: OpenServerSockTCP
** purpose: Opens a passive TCP connection on "port".  After opening
**	the socket, the server is automatically set to listen on that socket.
**	The programmer need only call accept() to allow connections by clients.
** parameters:
**	port: The port number for the server.
** returns:
**	integer file descriptor for low level io.  Needs to remain a file
**	descriptor to accept() client connections.  Can convert the client
**	connection file descriptor to a FILE * using fdopen();
**=================================================================*/
int TCPOpenServerSock(int port);
int UDPOpenServerSock(int port);

/*==================================================================
** name: TCPBlockingWrite
** purpose: Handles error conditions when writing to a file descriptor.
**	If a buffer is too large to write in one block, this routine will
**	divide the buffer into two segments and recursively call itself to
**	send each of the halves.
** parameters:
**	fd: file descriptor to write the buffer data to.
**	buffer: an array of bytes write to the file stream.
**	buflen: number of bytes in the buffer
** returns:
**	either the number of characters written or constant describing
**	why the write failed.
** notes: If you want the routine to print identifiable errors
**	to stderr, define PERRORS.
**=================================================================*/
int TCPBlockingWrite(int fd, char *buffer, int buflen);

/*==================================================================
** name: TCPBlockingRead
** purpose: Handles error conditions when reading from a file descriptor.
**	With TCP stream connections, the record size of the recieved stream
**	may not coincide with the record written.  This routine assembles
**	a fragmented record into a single array by blocking until buflen
**	characters are recieved, or write returns a premature EOF.
** parameters:
**	fd: file descriptor to read the buffer data from.
**	buffer: an array of bytes read from the file stream.
**	buflen: number of bytes in the buffer
** returns:
**	either the number of characters read into the buffer or constant describing
**	why the read failed.
** notes: If you want the routine to print out identifiable errors
**	to stderr, define PERRORS.
**=================================================================*/
int TCPBlockingRead(int fd,char *buffer,int buflen);
#else
int TCPOpenClientSock();
int TCPOpenServerSock();
int TCPBlockingWrite();
int TCPBlockingRead();
#endif

#define PERRORS
#ifndef  __IN_HEADER
#include <sys/types.h>
#include <netinet/in.h>
#endif

