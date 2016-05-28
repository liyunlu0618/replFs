#ifndef NETWORK_H
#define NETWORK_H

#include "main.h"

#define REPLFSGROUP 0xe0010101

int network_init(unsigned short, struct sockaddr *, int *);
ssize_t network_recvfrom(int, void *, size_t, int, struct sockaddr *, socklen_t *, int);

#endif
