#ifndef _EVAIO_H
#define _EVAIO_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h> // calloc
#include <unistd.h>
#include <fcntl.h> // for splice and fcntl

#include <event.h> // libevent

#include <linux/tcp.h> // setsockopt for TCP_CORK on Linux
//#define DEBUG

// splice-related constants not defined in fcntl.h
#ifndef SPLICE_F_MOVE

#define SPLICE_F_MOVE	(0x01)	/* move pages instead of copying */
#define SPLICE_F_NONBLOCK (0x02) /* don't block on the pipe splicing (but */
				 /* we may still block on the fd we splice */
				 /* from/to, of course */
#define SPLICE_F_MORE	(0x04)	/* expect more data */
#define SPLICE_F_GIFT   (0x08)  /* pages passed in are a gift */
#endif /* SPLICE_F_MOVE defined */

/*
 * SPLICE_F_UNMAP was introduced later, so check for that seperately
 */
#ifndef SPLICE_F_UNMAP
#define SPLICE_F_UNMAP	(0x10)	/* undo vmsplice map */
#endif


// Linux uses 64k pipe buffers
#define EVAIO_BS	(size_t)65536

/* move to private header */
/* pipe on latest linux, small buffer in old linux and win */
/* Todo: cache buffers, pipe creation/destruction takes about 8usec per cycle, 40 times
 * as long as malloc/free */
struct evaio_buffer {
        int pipe_in;
        int pipe_out;
        ssize_t fill;
        ssize_t off;
        char *bytes[EVAIO_BS];
};

/* fd->fd */
struct evaio_fd_fd_io {
        int fd_in;
        loff_t off_in;
        int fd_out;
        loff_t off_out;
};

// string -> fd or fd -> buffer of size len
// call int _PyString_Resize( PyObject **string, Py_ssize_t newsize) to grow buffer
struct evaio_fd_buf_io {
        void *buf;
        loff_t off_buf; // incremented on write
        int fd;
        loff_t off_fd;  // incremented if supported
};

/* end private header stuff */

// the default timeslice for looping async ops if they don't block
#define DEFAULT_TIMESLICE 0.1

#define EVAIO_READSOME  1

#define __evaio_job_common \
    size_t size; \
    size_t size_done; \
    struct evaio_buffer *pipe_buf; \
    unsigned int runs; /* the number of runs to write in one go if not blocked */ \
    short flags

struct evaio_job {
    __evaio_job_common;
};

struct evaio_fd_buf_job {
    __evaio_job_common;
    void *buf;
    loff_t buf_offset; // incremented on write
    int fd;
    int fd_flags;
    loff_t off_fd;  // incremented if supported
};

struct evaio_fd_fd_job {
    __evaio_job_common;
    int fd_in;
    loff_t fd_in_offset;
    int fd_in_flags;
    int fd_out;
    loff_t fd_out_offset;
    int fd_out_flags;
};



struct evaio_job *evaio_job_new( void );

size_t evaio_sendfile(int fd_in, off_t off_in, int fd_out, size_t size, struct conti *co);
// read/write first, use real splice later
size_t evaio_recvfile(int sock_in, int fd_out, off_t off_out, size_t size, struct conti *co);


size_t evaio_write(void *buf, size_t size, int fd, struct conti *co);
int evaio_read(int fd_in, void *buf, size_t size, struct conti *co);
int evaio_recv(int fd_in, void *buf, size_t size, struct conti *co);
int evaio_recv_peek(int fd_in, void *buf, size_t size, struct conti *co);

size_t evaio_fwrite( const void *ptr, size_t size, size_t nmemb,
                     FILE *stream);
size_t evaio_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);

size_t evaio_copyfile(int fd_in, off_t off_in, int fd_out, size_t size);
// read/write first, use real splice later

// saves setting up another pipe
size_t evaio_sendpipe(int pipe_in, int fd_out, size_t size);
size_t evaio_recvpipe(int sock_in, int pipe_out, size_t size);


/* 
 * use writev if available, else buffer + write
 * just write sequentially on non-supporting systems
 */
#include <sys/uio.h>
ssize_t evaio_readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t evaio_writev(int fd, const struct iovec *iov, int iovcnt);


/* Each transfer allocates a custom struct that holds all info needed for the
 * transfer plus an event pointer and calls a _doxx function with this struct
 * If IO blocks an event is registered to monitor the file that blocked,
 * calling the _doxx when ready.
 * When all is transferred, call callback.
 * If error occured, call errback.
 */

void evaio_set_event(struct event *ev_ptr, 
        int fd, short flags, 
        int timeout, 
        void (*func)(int, short, void *), 
        void *args);

static inline int
evaio_set_nonblocking(int fd) {
    int flags;
    if (unlikely((flags = fcntl(fd, F_GETFL))) < 0) {
        perror("getting O_NONBLOCK mode");
        return flags;
    }
    if (unlikely(!(flags & O_NONBLOCK) && fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)) {
        perror("setting O_NONBLOCK mode");
        return -1;
    }
    return 0;
}

#endif // _EVAIO_H
