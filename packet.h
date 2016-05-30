#ifndef PACKET_H
#define PACKET_H

#include "main.h"

#define MAXNAMELEN 128
#define MAXBLOCKLEN 512

#define PKT_OPEN		0
#define PKT_OPENACK		1
#define PKT_WRITE		2
#define PKT_CHECK		3
#define PKT_CHECKRES		4
#define PKT_COMMIT		5
#define PKT_ABORT		6
#define PKT_COMMITABORTACK	7

typedef struct pkt_header {
	uint32_t type;
} pkt_header_t;

typedef struct pkt_open {
	uint32_t type;
	uint32_t fd;
	char filename[MAXNAMELEN];
} pkt_open_t;

typedef struct pkt_openack {
	uint32_t type;
	uint32_t server_id;
	uint32_t fd;
} pkt_openack_t;

typedef struct pkt_write {
	uint32_t type;
	uint32_t fd;
	uint32_t write_no;
	uint32_t byte_offset;
	uint32_t blocksize;
	char data[MAXBLOCKLEN];
} pkt_write_t;

typedef struct pkt_check {
	uint32_t type;
	uint32_t fd;
	uint32_t num_writes;
} pkt_check_t;

typedef struct pkt_checkres {
	uint32_t type;
	uint32_t server_id;
	uint32_t fd;
	uint32_t missing[2];
} pkt_checkres_t;

typedef struct pkt_commit {
	uint32_t type;
	uint32_t fd;
} pkt_commit_t;

typedef struct pkt_abort {
	uint32_t type;
	uint32_t fd;
} pkt_abort_t;

typedef struct pkt_commitabortack {
	uint32_t type;
	uint32_t server_id;
	uint32_t fd;
} pkt_commitabortack_t;

#endif
