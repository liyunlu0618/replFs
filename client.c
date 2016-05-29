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
#include <sys/select.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include "main.h"
#include "network.h"
#include "packet.h"
#include <client.h>

#define MAXBLOCKLEN	512
#define MAXWRITES	64
#define MAXSERVERS	16
#define MAXNAMELEN	128

#define RESEND		500
#define TIMEOUT		5000

#define NFDS		16

static struct sockaddr client_addr;
static int client_sock;
static uint32_t pkt_drop;
static int server_cnt;
static uint32_t open_fd;
static char open_fname[MAXNAMELEN];

typedef struct write_request {
	uint32_t fd;
	uint32_t commit_no;
	uint32_t write_no;
	uint32_t byte_offset;
	uint32_t blocksize;
	char buffer[MAXBLOCKLEN];
} write_request_t;

static bool
has_open_file()
{
	return open_fname[0] != '\0';
}

static bool
timeout_triggered(struct timeval *now, struct timeval *last, int diff)
{
	return ((now->tv_sec - last->tv_sec) * 1000
		+ (now->tv_usec - last->tv_usec) / 1000) >= diff;

}

static write_request_t* write_log[MAXWRITES];

static int
client_init(unsigned short portNum, int packetLoss, int numServers)
{
	server_cnt = numServers;
	pkt_drop = packetLoss;
	int i;

	for (i = 0; i < MAXWRITES; i++)
		write_log[i] = NULL;

	if (network_init(portNum, &client_addr, &client_sock) < 0)
		return -1;

	debug_printf("client_sock %d\n", client_sock);

	return 0;
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

static int
client_process_open_ack(pkt_openack_t *p, uint32_t *act_cnt)
{
	uint32_t id = ntohl(p->server_id);
	int i = 0;

	for (i = 0; i < MAXSERVERS; i++) {
		if (act_cnt[i] == id) break;

		if (act_cnt[i] == 0) {
			act_cnt[i] = id;
			break;
		}
	}

	for (i = 0; i < MAXSERVERS; i++) {
		if (act_cnt[i] == 0) break;
	}

	return i;
}

static int
client_open_file(char *filename)
{
	uint32_t ack_cnt[MAXSERVERS];
	memset(ack_cnt, 0, sizeof (ack_cnt));

	pkt_open_t out;
	pkt_openack_t in;
	struct timeval orig, last, now;

	fd_set fdset;
	FD_ZERO(&fdset);
	FD_SET(client_sock, &fdset);

	struct timeval resend;
	resend.tv_sec = 0;
	resend.tv_usec = RESEND * 1000;

	if (has_open_file()) {
		if (strcmp(open_fname, filename) == 0) return open_fd;
		debug_printf("Already opened a file\n");
		return -1;
	}

	if (strlen(filename) >= MAXNAMELEN) {
		debug_printf("File name too long\n");
		return -1;
	}

	srand(time(NULL));
	open_fd = rand();
	debug_printf("open fd: %d\n", open_fd);

	out.type = htonl(PKT_OPEN);
	out.fd = htonl(open_fd);
	strncpy(out.filename, filename, strlen(filename));

	sendto(client_sock, &out, sizeof (out), 0, &client_addr, sizeof (struct sockaddr));
	gettimeofday(&last, NULL);
	gettimeofday(&orig, NULL);

	while (1) {
		gettimeofday(&now, NULL);
		if (timeout_triggered(&now, &orig, TIMEOUT)) break;

		if (timeout_triggered(&now, &last, RESEND)) {
			sendto(client_sock, &out, sizeof (out), 0, &client_addr, sizeof (struct sockaddr));
			gettimeofday(&last, NULL);
		}

		if (select(NFDS, &fdset, NULL, NULL, &resend) <= 0)
			continue;

		if (network_recvfrom(client_sock, &in, sizeof (in), 0, NULL, NULL, pkt_drop) < 0)
			continue;

		if (ntohl(in.type) != PKT_OPENACK || ntohl(in.fd) != open_fd)
			continue;

		int ret = client_process_open_ack(&in, ack_cnt);
		if (ret == server_cnt) {
			strncpy(open_fname, filename, strlen(filename));
			debug_printf("Open file %s success\n", filename);
			return open_fd;
		}
	}

	return -1;
}

/* ------------------------------------------------------------------ */

int
OpenFile(char * fileName)
{
	return client_open_file(fileName);
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




