#ifndef PACKET_H
#define PACKET_H

#include "main.h"

#define MAXNAMELEN 128
#define MAXBLOCKLEN 512

typedef struct pkt_init {
	uint32_t type;
} pkt_init_t;

typedef struct pkt_initack {
	uint32_t type;
	uint32_t server_id;
} pkt_initack_t;

typedef struct pkt_open {
	uint32_t type;
	uint32_t fd;
	char filename[MAXNAMELEN];
} pkt_open_t;

typedef struct pkt_openack {
	uint32_t type;
	uint32_t server_id;
	uint32_t fd;
} pkt_openact_t;

typedef struct pkt_write {
	uint32_t type;
	uint32_t fd;
	uint32_t commit_no;
	uint32_t write_no;
	uint32_t byte_offset;
	uint32_t blocksize;
	char data[MAXBLOCKLEN];
} pkt_write_t;

typedef struct pkt_check {
	uint32_t type;
	uint32_t fd;
	uint32_t commit_no;
	uint32_t num_writes;
} pkt_check_t;

typedef struct pkt_checkyes {
	uint32_t type;
	uint32_t server_id;
	uint32_t fd;
	uint32_t commit_no;
} pkt_checkyes_t;

typedef struct pkt_checkno {
	uint32_t type;
	uint32_t server_id;
	uint32_t fd;
	uint32_t commit_no;
	uint64_t missing;
} pkt_checkno_t;

typedef struct pkt_commit {
	uint32_t type;
	uint32_t close;
	uint32_t fd;
	uint32_t commit_no;
} pkt_commit_t;

typedef struct pkt_commitack {
	uint32_t type;
	uint32_t server_id;
	uint32_t fd;
	uint32_t commit_no;
} pkt_commitack_t;

typedef struct pkt_abort {
	uint32_t type;
	uint32_t fd;
	uint32_t commit_no;
} pkt_abort_t;

typedef struct pkt_abortack {
	uint32_t type;
	uint32_t server_id;
	uint32_t fd;
	uint32_t commit_no;
} pkt_abortack_t;

#endif
