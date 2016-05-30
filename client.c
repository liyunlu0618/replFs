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
#define MAXPACKETSIZE	1024

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
	uint32_t byte_offset;
	uint32_t blocksize;
	char buffer[MAXBLOCKLEN];
} write_request_t;

static write_request_t* write_log[MAXWRITES];
static uint32_t write_no;

static void
clear_log()
{
	int i = 0;
	for ( ; i < MAXWRITES; i++) {
		free(write_log[i]);
		write_log[i] = NULL;
	}

	write_no = 0;
}

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

static int
client_init(unsigned short portNum, int packetLoss, int numServers)
{
	server_cnt = numServers;
	pkt_drop = packetLoss;

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
client_process_open_ack(pkt_openack_t *p, uint32_t *ack_cnt)
{
	uint32_t id = ntohl(p->server_id);
	debug_printf("new server id %d\n", id);

	int i = 0;

	for (i = 0; i < MAXSERVERS; i++) {
		if (ack_cnt[i] == id) break;

		if (ack_cnt[i] == 0) {
			ack_cnt[i] = id;
			break;
		}
	}

	for (i = 0; i < MAXSERVERS; i++) {
		if (ack_cnt[i] == 0) break;
		debug_printf("%d: server id %d\n", i, ack_cnt[i]);
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
	while ((open_fd = rand()) == 0) open_fd = rand();

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
			debug_printf("open resend triggered\n");
			sendto(client_sock, &out, sizeof (out), 0, &client_addr, sizeof (struct sockaddr));
			gettimeofday(&last, NULL);
		}

		FD_ZERO(&fdset);
		FD_SET(client_sock, &fdset);
		if (select(NFDS, &fdset, NULL, NULL, &resend) <= 0) {
			//debug_printf("wait for socket timed out\n");
			continue;
		}

		if (network_recvfrom(client_sock, &in, sizeof (in), 0, NULL, NULL, pkt_drop) <= 0) {
			debug_printf("packet drop\n");
			continue;
		}

		if (ntohl(in.type) != PKT_OPENACK) {
			debug_printf("open wrong type\n");
			continue;
		}

		if (ntohl(in.fd) != open_fd) {
			debug_printf("open wrong fd %d %d\n", ntohl(in.fd), open_fd);
			continue;
		}

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

static int
client_write_file(int fd, char *buffer, int offset, int blocksize)
{
	pkt_write_t out;
	memset(&out, 0, sizeof (out));
	write_request_t *wr = calloc(sizeof (char), sizeof(write_request_t));
	if (wr == NULL)
		return -1;

	if (fd != open_fd) {
		debug_printf("wrong file descriptor\n");
		return -1;
	}

	if (write_no >= 64 || blocksize > MAXBLOCKLEN) return -1;

	out.type = htonl(PKT_WRITE);
	out.fd = htonl(open_fd);
	out.write_no = htonl(write_no);
	out.byte_offset = htonl(offset);
	out.blocksize = htonl(blocksize);
	memcpy(out.data, buffer, blocksize);

	sendto(client_sock, &out, sizeof (out), 0, &client_addr, sizeof (struct sockaddr));

	wr->byte_offset = offset;
	wr->blocksize = blocksize;
	memcpy(wr->buffer, buffer, blocksize);
	write_log[write_no] = wr;

	write_no++;
	debug_printf("write no %d\n", write_no);

	return blocksize;
}

/* ------------------------------------------------------------------ */

int
WriteBlock( int fd, char * buffer, int byteOffset, int blockSize ) {
	ASSERT( fd >= 0 );
	ASSERT( byteOffset >= 0 );
	ASSERT( buffer );
	ASSERT( blockSize >= 0 && blockSize < MaxBlockLength );

#ifdef DEBUG
	printf( "WriteBlock: Writing FD=%d, Offset=%d, Length=%d\n",
		fd, byteOffset, blockSize );
#endif

	return client_write_file(fd, buffer, byteOffset, blockSize);
}

static void
client_resend_write(int i)
{
	pkt_write_t out;
	write_request_t *wr = write_log[i];

	out.type = htonl(PKT_WRITE);
	out.fd = htonl(open_fd);
	out.write_no = htonl(i);
	out.byte_offset = htonl(wr->byte_offset);
	out.blocksize = htonl(wr->blocksize);
	memcpy(out.data, wr->buffer, wr->blocksize);

	sendto(client_sock, &out, sizeof (out), 0, &client_addr, sizeof (struct sockaddr));
}

static int
client_process_checkres(void *packet, uint32_t *ack_cnt)
{
	int i;
	pkt_checkres_t *p = (pkt_checkres_t *)packet;
	p->type = ntohl(p->type);
	p->server_id = ntohl(p->server_id);
	p->fd = ntohl(p->fd);
	p->missing[0] = ntohl(p->missing[0]);
	p->missing[1] = ntohl(p->missing[1]);

	if (p->missing[0] == 0 && p->missing[1] == 0) {
		debug_printf("no write missing\n");
		for (i = 0; i < MAXSERVERS; i++) {
			if (ack_cnt[i] == p->server_id) break;
			if (ack_cnt[i] == 0) {
				ack_cnt[i] = p->server_id;
				break;
			}
		}
	} else {
		for (i = 0; i < write_no; i++) {
			if (i < 32) {
				if (p->missing[0] & (1 << i))
					client_resend_write(i);
			} else {
				if (p->missing[1] & (1 << (i - 32)))
					client_resend_write(i);
			}
		}
	}

	for (i = 0; i < MAXSERVERS; i++) {
		debug_printf("check write %dth server, id %d\n", i, ack_cnt[i]);
		if (ack_cnt[i] == 0) break;
	}

	return i;
}

static int
client_check_write_log()
{
	int ret = 0;
	uint32_t ack_cnt[MAXSERVERS];
	memset(ack_cnt, 0, sizeof (ack_cnt));

	pkt_check_t out;
	pkt_checkres_t in;
	memset(&in, 0, sizeof (in));

	struct timeval orig, last, now;

	fd_set fdset;

	struct timeval resend;
	resend.tv_sec = 0;
	resend.tv_usec = RESEND * 1000;

	out.type = htonl(PKT_CHECK);
	out.fd = htonl(open_fd);
	out.num_writes = htonl(write_no);

	sendto(client_sock, &out, sizeof (out), 0, &client_addr, sizeof (struct sockaddr));
	gettimeofday(&last, NULL);
	gettimeofday(&orig, NULL);

	while (1) {
		gettimeofday(&now, NULL);
		if (timeout_triggered(&now, &orig, TIMEOUT)) break;

		if (timeout_triggered(&now, &last, RESEND)) {
			debug_printf("check write resend triggered\n");
			sendto(client_sock, &out, sizeof (out), 0, &client_addr, sizeof (struct sockaddr));
			gettimeofday(&last, NULL);
		}

		FD_ZERO(&fdset);
		FD_SET(client_sock, &fdset);
		if (select(NFDS, &fdset, NULL, NULL, &resend) <= 0) {
			continue;
		}

		if (network_recvfrom(client_sock, &in, sizeof (in), 0, NULL, NULL, pkt_drop) <= 0) {
			debug_printf("packet drop\n");
			continue;
		}

		if (ntohl(in.type) != PKT_CHECKRES) {
			debug_printf("check wrong packet\n");
			continue;
		}

		if (ntohl(in.fd) != open_fd) {
			debug_printf("check wrong fd %d %d\n", ntohl(in.fd), open_fd);
			continue;
		}

		ret = client_process_checkres(&in, ack_cnt);
		if (ret == server_cnt) {
			debug_printf("all ready to commit\n");
			break;
		}
	}

	ASSERT(ret <= server_cnt);
	server_cnt = ret;
	return 0;
}

static int
client_process_commitabortack(void *packet, uint32_t *ack_cnt)
{
	pkt_commitabortack_t *p = (pkt_commitabortack_t *)packet;
	p->server_id = ntohl(p->server_id);

	int i = 0;
	for (i = 0; i < MAXSERVERS; i++) {
		if (ack_cnt[i] == p->server_id) break;
		if (ack_cnt[i] == 0) {
			ack_cnt[i] = p->server_id;
			break;
		}
	}

	for (i = 0; i < MAXSERVERS; i++) {
		if (ack_cnt[i] == 0) break;
	}

	return i;
}

static int
client_commit()
{
	int ret = 0;
	uint32_t ack_cnt[MAXSERVERS];
	memset(ack_cnt, 0, sizeof (ack_cnt));

	pkt_commit_t out;
	pkt_commitabortack_t in;

	struct timeval orig, last, now;

	fd_set fdset;

	struct timeval resend;
	resend.tv_sec = 0;
	resend.tv_usec = RESEND * 1000;

	out.type = htonl(PKT_COMMIT);
	out.fd = htonl(open_fd);

	sendto(client_sock, &out, sizeof (out), 0, &client_addr, sizeof (struct sockaddr));
	gettimeofday(&last, NULL);
	gettimeofday(&orig, NULL);

	while (1) {
		gettimeofday(&now, NULL);
		if (timeout_triggered(&now, &orig, TIMEOUT)) break;

		if (timeout_triggered(&now, &last, RESEND)) {
			debug_printf("commit resend triggered\n");
			sendto(client_sock, &out, sizeof (out), 0, &client_addr, sizeof (struct sockaddr));
			gettimeofday(&last, NULL);
		}

		FD_ZERO(&fdset);
		FD_SET(client_sock, &fdset);
		if (select(NFDS, &fdset, NULL, NULL, &resend) <= 0) {
			//debug_printf("wait for socket timed out\n");
			continue;
		}

		if (network_recvfrom(client_sock, &in, sizeof (in), 0, NULL, NULL, pkt_drop) <= 0) {
			debug_printf("packet drop\n");
			continue;
		}

		if (ntohl(in.type) != PKT_COMMITABORTACK) {
			debug_printf("commit wrong packet\n");
			continue;
		}

		if (ntohl(in.fd) != open_fd) {
			debug_printf("commit wrong fd %d %d\n", ntohl(in.fd), open_fd);
			continue;
		}

		ret = client_process_commitabortack(&in, ack_cnt);
		if (ret == server_cnt) {
			debug_printf("all ready to commit\n");
			break;
		}
	}

	ASSERT(ret <= server_cnt);
	server_cnt = ret;
	clear_log();
	return 0;
}

/* ------------------------------------------------------------------ */

int
Commit( int fd ) {
	ASSERT( fd >= 0 );
	if (fd != open_fd) {
		debug_printf("wrong fd\n");
		return -1;
	}

#ifdef DEBUG
	printf( "Commit: FD=%d\n", fd );
#endif

	/****************************************************/
	/* Prepare to Commit Phase			    */
	/* - Check that all writes made it to the server(s) */
	/****************************************************/
	
	if (client_check_write_log() < 0) return -1;
	/****************/
	/* Commit Phase */
	/****************/

	if (client_commit(FALSE) < 0) return -1;

	return 0;
}

static int
client_abort()
{
	int ret = 0;
	uint32_t ack_cnt[MAXSERVERS];
	memset(ack_cnt, 0, sizeof (ack_cnt));

	pkt_abort_t out;
	pkt_commitabortack_t in;

	struct timeval orig, last, now;

	fd_set fdset;

	struct timeval resend;
	resend.tv_sec = 0;
	resend.tv_usec = RESEND * 1000;

	out.type = htonl(PKT_ABORT);
	out.fd = htonl(open_fd);

	sendto(client_sock, &out, sizeof (out), 0, &client_addr, sizeof (struct sockaddr));
	gettimeofday(&last, NULL);
	gettimeofday(&orig, NULL);

	while (1) {
		gettimeofday(&now, NULL);
		if (timeout_triggered(&now, &orig, TIMEOUT)) break;

		if (timeout_triggered(&now, &last, RESEND)) {
			debug_printf("abort resend triggered\n");
			sendto(client_sock, &out, sizeof (out), 0, &client_addr, sizeof (struct sockaddr));
			gettimeofday(&last, NULL);
		}

		FD_ZERO(&fdset);
		FD_SET(client_sock, &fdset);
		if (select(NFDS, &fdset, NULL, NULL, &resend) <= 0) {
			//debug_printf("wait for socket timed out\n");
			continue;
		}

		if (network_recvfrom(client_sock, &in, sizeof (in), 0, NULL, NULL, pkt_drop) <= 0) {
			debug_printf("packet drop\n");
			continue;
		}

		if (ntohl(in.type) != PKT_COMMITABORTACK) {
			debug_printf("abort wrong packet\n");
			continue;
		}

		if (ntohl(in.fd) != open_fd) {
			debug_printf("abort wrong fd %d %d\n", ntohl(in.fd), open_fd);
			continue;
		}

		ret = client_process_commitabortack(&in, ack_cnt);
		if (ret == server_cnt) {
			debug_printf("all ready to commit\n");
			break;
		}
	}

	ASSERT(ret <= server_cnt);
	server_cnt = ret;
	clear_log();
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
	return client_abort();
}

/* ------------------------------------------------------------------ */

int
CloseFile( int fd )
{
	int ret = 0;
	ASSERT( fd >= 0 );

#ifdef DEBUG
	printf( "Close: FD=%d\n", fd );
#endif

	/*****************************/
	/* Check for Commit or Abort */
	/*****************************/
	open_fname[0] = '\0';
	if (write_log[0] == NULL) return 0;

	ret = Commit(fd);
	open_fd = 0;
	return ret;
}

/* ------------------------------------------------------------------ */
