#ifndef MAIN_H
#define MAIN_H

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <time.h>
#include <stdbool.h>

#define DEBUGPRINT 1
#define debug_printf(...) \
	do { if (DEBUGPRINT) fprintf(stderr, ##__VA_ARGS__); } while (0)
#endif
