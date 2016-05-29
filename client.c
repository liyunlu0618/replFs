/****************/
/* Your Name	*/
/* Date		*/
/* CS 244B	*/
/* Spring 2014	*/
/****************/

#define DEBUG

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include "main.h"
#include "network.h"
#include <client.h>

#define MAXBLOCKLEN	512
#define MAXWRITES	64
#define MAXSERVERS	16

static struct sockaddr client_addr;
static int client_sock;
static uint32_t pkt_drop;

typedef struct write_request {
	uint32_t fd;
	uint32_t commit_no;
	uint32_t write_no;
	uint32_t byte_offset;
	uint32_t blocksize;
	char buffer[MAXBLOCKLEN];
} write_request_t;

typedef struct server {
	uint32_t server_id;
	bool available;
	uint32_t missing_writes[2];
	bool vote;
} server_t;

static write_request_t* write_log[MAXWRITES];
static server_t *server_list[MAXSERVERS];

static int
client_init(unsigned short portNum, int packetLoss, int numServers)
{
	pkt_drop = packetLoss;
	int i, ret;

	for (i = 0; i < MAXWRITES; i++)
		write_log[i] = NULL;
	for (i = 0; i < MAXSERVERS; i++)
		server_list[i] = NULL;

	ret = network_init(portNum, &client_addr, &client_sock);
	debug_printf("client_sock %d\n", client_sock);
	return ret;
}
/* ------------------------------------------------------------------ */

int
InitReplFs(unsigned short portNum, int packetLoss, int numServers)
{
#ifdef DEBUG
	printf( "InitReplFs: Port number %d, packet loss %d percent, %d servers\n", 
		portNum, packetLoss, numServers );
#endif

	/****************************************************/
	/* Initialize network access, local state, etc.     */
	/****************************************************/

	if (client_init(portNum, packetLoss, numServers) < 0)
		return -1;

	return 0;  
}

/* ------------------------------------------------------------------ */

int
OpenFile( char * fileName ) {
  int fd;

  ASSERT( fileName );

#ifdef DEBUG
  printf( "OpenFile: Opening File '%s'\n", fileName );
#endif

  fd = open( fileName, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR );

#ifdef DEBUG
  if ( fd < 0 )
    perror( "OpenFile" );
#endif

  return( fd );
}

/* ------------------------------------------------------------------ */

int
WriteBlock( int fd, char * buffer, int byteOffset, int blockSize ) {
  //char strError[64];
  int bytesWritten;

  ASSERT( fd >= 0 );
  ASSERT( byteOffset >= 0 );
  ASSERT( buffer );
  ASSERT( blockSize >= 0 && blockSize < MaxBlockLength );

#ifdef DEBUG
  printf( "WriteBlock: Writing FD=%d, Offset=%d, Length=%d\n",
	fd, byteOffset, blockSize );
#endif

  if ( lseek( fd, byteOffset, SEEK_SET ) < 0 ) {
    perror( "WriteBlock Seek" );
    return -1;
  }

  if ( ( bytesWritten = write( fd, buffer, blockSize ) ) < 0 ) {
    perror( "WriteBlock write" );
    return -1;
  }

  return( bytesWritten );

}

/* ------------------------------------------------------------------ */

int
Commit( int fd ) {
  ASSERT( fd >= 0 );

#ifdef DEBUG
  printf( "Commit: FD=%d\n", fd );
#endif

	/****************************************************/
	/* Prepare to Commit Phase			    */
	/* - Check that all writes made it to the server(s) */
	/****************************************************/

	/****************/
	/* Commit Phase */
	/****************/

  return 0;

}

/* ------------------------------------------------------------------ */

int
Abort( int fd )
{
  ASSERT( fd >= 0 );

#ifdef DEBUG
  printf( "Abort: FD=%d\n", fd );
#endif

  /*************************/
  /* Abort the transaction */
  /*************************/

  return 0;
}

/* ------------------------------------------------------------------ */

int
CloseFile( int fd ) {

  ASSERT( fd >= 0 );

#ifdef DEBUG
  printf( "Close: FD=%d\n", fd );
#endif

	/*****************************/
	/* Check for Commit or Abort */
	/*****************************/

  if ( close( fd ) < 0 ) {
    perror("Close");
    return -1;
  }

  return 0;
}

/* ------------------------------------------------------------------ */




