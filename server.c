#include "network.h"

#define MAXWRITES 64
#define MAXBLOCKLENGTH 512
#define REPLFSPORT 44055

static struct sockaddr server_addr;
static int server_sock;
static uint32_t server_id;
static char *server_mountpoint;
static int pkt_drop;

typedef struct write_request {
	uint32_t fd;
	uint32_t commit_no;
	uint32_t write_no;
	uint32_t byte_offset;
	uint32_t blocksize;
	char buffer[MAXBLOCKLENGTH];
} write_request_t;

static write_request_t* write_log[MAXWRITES];

static int
server_init(unsigned short port, int drop_ratio, char *mountpoint) {
	srand(time(NULL));
	server_id = rand();
	pkt_drop = drop_ratio;
	server_mountpoint = strdup(mountpoint);

	int i = 0;
	for (i = 0; i < MAXWRITES; i++)
		write_log[i] = NULL;

	return network_init(port, &server_addr, &server_sock);
}

int
main(int argc, char *argv[]) {
	unsigned short port;
	int drop;
	
	if (argc != 7) {
		printf("Usage: replFsServer -port <portnum> -mount <mount_path> -drop <drop_ratio>\n");
		exit(-1);
	}

	port = atoi(argv[2]);
	drop = atoi(argv[6]);

	if (server_init(port, drop, argv[4]) < 0)
		exit (-1);

	while (1) {

	}
}
