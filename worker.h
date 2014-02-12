#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/timeb.h>
#include <math.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>

#ifndef _WORKER_H
#define _WORKER_H

typedef struct {
    void *se_ptr;
    void *timeout_ptr;

    int fd;
    time_t stime;

    struct in_addr client_addr;

    char *out_buf;
    int out_buf_len;
    int out_buf_sended;
} epdata_t;

void worker_main(int _worker_n);

#endif
