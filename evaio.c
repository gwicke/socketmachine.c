//#include "evaio.h"

#include "socketmachine.h"

#define event_debug(args)   //printf args

// forward declaration
static size_t
evaio_fdfd_splice(int, short, struct conti *);

static struct evaio_buffer
*get_pipe_buffer(void)
{
    struct evaio_buffer *buf = malloc(sizeof(struct evaio_buffer));
    int pfds[2];
    if (pipe(pfds) < 0) {
        return NULL;
    }
    buf->pipe_in = pfds[0];
    buf->pipe_out = pfds[1];
    buf->fill = 0;
    buf->off = 0;
    return buf;
}

static void
return_pipe_buffer(struct evaio_buffer *evpipe)
{
    close(evpipe->pipe_in);
    close(evpipe->pipe_out);
    free(evpipe);
}


void
evaio_set_event(struct event *ev_ptr, 
        int fd, short flags, 
        int timeout, 
        void (*func)(int, short, void *), 
        void *args)
{
    struct timeval tv;

    event_set(ev_ptr, fd, flags, func, args);
    /*else if (event_pending(*ev_ptr, flags|EV_TIMEOUT, NULL))
        event_del(*ev_ptr);*/
    
    bzero(&tv, sizeof(struct timeval));
    tv.tv_sec = timeout >= 0 ? timeout : 0;
    
    event_add(ev_ptr, &tv);
}

size_t 
evaio_sendfile(
        int fd_in, 
        off_t off_in, 
        int fd_out, 
        size_t size, 
        struct conti *co)
{
    struct evaio_fd_fd_job *job = alloca(sizeof(struct evaio_fd_fd_job));
    job->size = size;
    job->size_done = 0;
    job->pipe_buf = get_pipe_buffer();
    job->runs = 1;
    job->flags = 0;
    job->fd_in = fd_in;
    job->fd_in_offset = off_in;
    job->fd_in_flags = SPLICE_F_NONBLOCK;
    job->fd_out = fd_out;
    job->fd_out_offset = -1;
    job->fd_out_flags = SPLICE_F_NONBLOCK;
    co->state = job;
#ifdef WIN32
    return evaio_fdfd_userspace(0, 0, co);
#else
    // should check for linux version >= 2.6.17
    return evaio_fdfd_splice(0, 0, co);
#endif
}


/* do the actual transfer from one fd to another */
static size_t
evaio_fdfd_splice(int fd, short what, struct conti *co)
{
    struct evaio_fd_fd_job *job = (struct evaio_fd_fd_job *)co->state;
    struct evaio_fd_fd_job *s = job;

    int i = 0;
    ssize_t to_read, in_bytes = 0, out_bytes;
    if (what == EV_TIMEOUT) {
        //event_debug(("evaio_buf_fd timed out!"));
        goto error;
    }
    loff_t *off_in = (job->fd_in_offset >= 0 ? (loff_t*)&job->fd_in_offset : NULL);
    loff_t *off_out = (job->fd_out_offset >= 0 ? (loff_t*)&job->fd_out_offset : NULL);
    
    do {
        if(!job->pipe_buf)
            goto error;
        to_read = min((EVAIO_BS - job->pipe_buf->fill), (job->size - job->size_done));
        event_debug(("to_read: %d\n", to_read));
        if (to_read > 4096 || (to_read > 0 && job->pipe_buf->fill == 0) ) {
            /* fill pipe */
            in_bytes = splice(job->fd_in, off_in, 
                       job->pipe_buf->pipe_out , NULL, 
                       to_read, job->fd_in_flags);
            if (in_bytes < 0) {
                if (errno == EAGAIN) {
                    event_debug(("sendfile: read suspended\n"));
                    SM_BLOCK(job->fd_in, EV_READ|EV_TIMEOUT, evaio_fdfd_splice, co, 400);
                } else {
                    /* call errback */
                    event_debug(("error in sendfile, read: %d, %s\n", errno, strerror(errno)));
                    goto error;
                }
            }

            job->pipe_buf->fill += in_bytes;
        }

        /* push stuff out of pipe */
        out_bytes = splice(job->pipe_buf->pipe_in, NULL, 
                job->fd_out, off_out, 
                job->pipe_buf->fill, job->fd_out_flags);

        if (out_bytes < 0) {
            if (errno == EAGAIN) {
                if ((to_read == 0 || in_bytes == 0) && co->ts->conn->fd == job->fd_out) {
                    /* end of read, try to disable TCP_CORK if it was set
                     * otherwise we never get the last chunk written if it
                     * won't fill a packet- splice reads packet-aligned when
                     * TCP_CORK is set. */
                    int state = 0;
                    setsockopt(job->fd_out, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));
                }
                SM_BLOCK(job->fd_out, EV_WRITE|EV_TIMEOUT, evaio_fdfd_splice, co, 400);
            } else {
                /* call errback */
                goto error;
            }
        }
        job->pipe_buf->fill -= out_bytes;
        job->size_done += out_bytes;
        i++;
    } while (out_bytes >= 0 && job->size_done < job->size && i < job->runs);
    if (job->size_done == job->size || (in_bytes == 0 && job->pipe_buf->fill == 0)) {
        return_pipe_buffer(job->pipe_buf);
        SM_RETURN(job->size_done, NULL);
    } else {
        SM_BLOCK(job->fd_out, EV_READ|EV_WRITE|EV_TIMEOUT, evaio_fdfd_splice, co, 400);
    }
error:
    return_pipe_buffer(job->pipe_buf);
    SM_RETURN(-1, NULL);
}




/* do the actual transfer from one fd to another, using a userspace buffer */
static size_t
evaio_fdfd_userspace(int fd, short what, struct conti *co)
{
    struct evaio_fd_fd_job *job = (struct evaio_fd_fd_job *)co->state;
    struct evaio_fd_fd_job *s = job;
    int i = 0;
    ssize_t to_read, in_bytes = 0, out_bytes;
    
    do {
        to_read = min(EVAIO_BS, (job->size - job->size_done));
        event_debug(("to_read: %d\n", to_read));
        if (job->pipe_buf->fill == 0) {
            /* fill buffer */
            in_bytes = read(job->fd_in, job->pipe_buf->bytes, to_read);
            if (in_bytes < 0) {
                if (errno == EAGAIN) {
                    event_debug(("sendfile: read suspended\n"));
                    SM_BLOCK(job->fd_in, EV_READ|EV_TIMEOUT, evaio_fdfd_userspace, co, 400);
                } else {
                    /* call errback */
                    event_debug(("error in sendfile, read: %d, %s\n", errno, strerror(errno)));
                    goto error;
                }
            }

            job->pipe_buf->fill += in_bytes;
        }

        /* push stuff out of buffer */
        out_bytes = write(job->fd_out, job->pipe_buf->bytes + job->pipe_buf->off, job->pipe_buf->fill - job->pipe_buf->off);
        if (out_bytes < 0) {
            if (errno == EAGAIN) {
                event_debug(("sendfile: write suspended\n"));
                if (to_read == 0 || in_bytes == 0) {
                    /* end of read, try to disable TCP_CORK if it was set
                     * otherwise we never get the last chunk written if it
                     * won't fill a packet- splice reads packet-aligned when
                     * TCP_CORK is set. */
                    int state = 0;
                    setsockopt(job->fd_out, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));
                }
                SM_BLOCK(job->fd_out, EV_WRITE|EV_TIMEOUT, evaio_fdfd_userspace, co, 400);
            } else {
                /* call errback */
                goto error;
            }
        }
        if ((out_bytes + job->pipe_buf->off) == job->pipe_buf->fill) {
            job->pipe_buf->off = 0;
            job->pipe_buf->fill = 0;
        } else {
            job->pipe_buf->off = out_bytes;
        }
        job->size_done += out_bytes;
        i++;
    } while (out_bytes >= 0 && job->size_done < job->size && i < job->runs);
    if (job->size_done == job->size || (in_bytes == 0 && job->pipe_buf->fill == 0)) {
        return_pipe_buffer(job->pipe_buf);
        SM_RETURN(job->size_done, NULL);
    } else {
        SM_BLOCK(job->fd_out, EV_READ|EV_WRITE|EV_TIMEOUT, evaio_fdfd_userspace, co, 400);
    }
error:
    return_pipe_buffer(job->pipe_buf);
    SM_RETURN(-1, NULL);
}

/* this might not work yet on linux <= 2.6.22, splice socket receive support
 * only got solid in 2.6.23
 * Should use userspace read/write on older versions instead */
size_t 
evaio_recvfile(
        int fd_in, 
        int fd_out, 
        off_t off_out, 
        size_t size, 
        struct conti *co)
{
    struct evaio_fd_fd_job *job = alloca(sizeof(struct evaio_fd_fd_job));
    job->size = size;
    job->size_done = 0;
    job->pipe_buf = get_pipe_buffer();
    job->runs = 1;
    job->flags = 0;
    job->fd_in = fd_in;
    job->fd_in_offset = -1;
    job->fd_in_flags = SPLICE_F_NONBLOCK|SPLICE_F_MOVE;
    job->fd_out = fd_out;
    job->fd_out_offset = off_out;
    job->fd_out_flags = SPLICE_F_NONBLOCK;
    co->state = job;
    return evaio_fdfd_userspace(0, 0, co);
}



/* do the actual transfer from a buffer to an fd */
static size_t
evaio_buf_fd(int fd, short what, struct conti *co)
{
    struct evaio_fd_buf_job *job = (struct evaio_fd_buf_job *)co->state;
    struct evaio_fd_buf_job *s = job;
    if (what == EV_TIMEOUT) {
        goto error;
    }
    int i = 0;
    ssize_t written = 0;
    do {
        written = write(job->fd, job->buf + job->size_done, job->size - job->size_done);
        event_debug(("written to fd %d %s: %d %d\n", job->fd, job->buf, written, errno));
        if (written < 0) {
            if (errno == EAGAIN || errno == EINTR ) {
                SM_BLOCK(job->fd, EV_WRITE|EV_TIMEOUT, evaio_buf_fd, co, 400);
            } else {
                /* call errback */
                goto error;
            }
        } else if (written == 0) {
            /* should not happen */
            goto error;
        }
        job->size_done += written;
        i++;
    } while (written >= 0 && job->size_done < job->size && i < job->runs);
    if (job->size_done == job->size) {
        SM_RETURN(job->size_done, NULL);
    } else {
        SM_BLOCK(job->fd, EV_WRITE|EV_TIMEOUT, evaio_buf_fd, co, 400);
    }

error:
    //event_debug(("gone to error\n"));
    SM_RETURN(-1, NULL);
}

/* writes a buffer to an fd */
size_t 
evaio_write(
        void *buf, 
        size_t size, 
        int fd, 
        struct conti *co)
{
    struct evaio_fd_buf_job *job = alloca(sizeof(struct evaio_fd_buf_job));
    job->flags = 0;
    job->size = size;
    job->size_done = 0;
    job->runs = 1;
    job->buf = buf;
    job->buf_offset = 0;
    job->fd = fd;
    job->fd_flags = 0;
    co->state = job;
    return evaio_buf_fd(fd, 0, co);
}

/* do the actual transfer from an fd to a buffer */
static int
evaio_fd_buf(int fd, short what, struct conti *co)
{
    struct evaio_fd_buf_job *job = (struct evaio_fd_buf_job *)co->state;
    struct evaio_fd_buf_job *s = job;
    if (what == EV_TIMEOUT) {
        //event_debug(("evaio_buf_fd timed out!"));
        goto error;
    }
    int i = 0;
    ssize_t read_bytes;
    do {
        read_bytes = read(job->fd, job->buf + job->size_done, job->size - job->size_done);
        // ssize_t recv(int s, void *buf, size_t len, int flags);
        event_debug(("read %d %s: %d %d\n", job->fd, job->buf, read_bytes, errno));
        if (read_bytes < 0) {
            if (errno == EAGAIN) {
                SM_BLOCK(job->fd, EV_READ|EV_TIMEOUT, evaio_fd_buf, co, 400);
            } else {
                /* call errback */
                //event_debug(("evaio_buf_fd: write error"));
                goto error;
            }
        } else if (read_bytes == 0) {
            /* should not happen */
            //event_debug(("evaio_buf_fd: wrote zero bytes"));
            goto error;
        }
        job->size_done += read_bytes;
        i++;
    } while (read_bytes >= 0 && job->size_done < job->size && i < job->runs && !(job->flags & EVAIO_READSOME));
    if (job->size_done == job->size) {
        SM_RETURN(job->size_done, NULL);
    } else {
        SM_BLOCK(job->fd, EV_READ|EV_TIMEOUT, evaio_fd_buf, co, 400);
    }

error:
    //event_debug(("gone to error\n"));
    SM_RETURN(-2, NULL);
}

/* read from fd to a buffer */
int 
evaio_read(
        int fd, 
        void *buf, 
        size_t size, 
        struct conti *co)
{
    struct evaio_fd_buf_job *job = alloca(sizeof(struct evaio_fd_buf_job));
    job->flags = 0;
    job->size = size;
    job->size_done = 0;
    job->runs = 1;
    job->buf = buf;
    job->buf_offset = 0;
    job->fd = fd;
    job->fd_flags = 0;
    co->state = job;
    return evaio_fd_buf(fd, 0, co);
}



/* do the actual transfer from a socket to a buffer */
static int
evaio_socket_buf(int fd, short what, struct conti *co)
{
    struct evaio_fd_buf_job *job = (struct evaio_fd_buf_job *)co->state;
    struct evaio_fd_buf_job *s = job;

    if (what == EV_TIMEOUT) {
        //event_debug(("evaio_buf_fd timed out!"));
        goto error;
    }
    int i = 0;
    ssize_t read_bytes;
    do {
        read_bytes = recv(job->fd, job->buf + job->size_done, job->size - job->size_done, job->fd_flags);
        event_debug(("recvd %d %s: %d %d\n", job->fd, job->buf, read_bytes, errno));
        if (read_bytes < 0) {
            if (errno == EAGAIN) {
                SM_BLOCK(job->fd, EV_READ|EV_TIMEOUT, evaio_socket_buf, co, 400);
            } else {
                /* call errback */
                //event_debug(("evaio_buf_fd: write error"));
                goto error;
            }
        } else if (read_bytes == 0) {
            /* should not happen */
            //event_debug(("evaio_buf_fd: wrote zero bytes"));
            goto error;
        }
        job->size_done += read_bytes;
        i++;
    } while (read_bytes >= 0 && job->size_done < job->size && i < job->runs && !(job->flags & EVAIO_READSOME));
    if (job->size_done == job->size || (job->flags & EVAIO_READSOME)) {
        SM_RETURN(job->size_done, NULL);
    } else {
        SM_BLOCK(job->fd, EV_READ|EV_TIMEOUT, evaio_socket_buf, co, 400);
    }
    SM_RETURN(job->size_done, NULL);

error:
    //event_debug(("gone to error\n"));
    SM_RETURN(-1, NULL);
}

/* read from fd to a buffer */
int 
evaio_recv(
        int fd, 
        void *buf, 
        size_t size, 
        struct conti *co)
{
    struct evaio_fd_buf_job *job = alloca(sizeof(struct evaio_fd_buf_job));
    job->flags = 0;
    job->size = size;
    job->size_done = 0;
    job->runs = 1;
    job->buf = buf;
    job->buf_offset = 0;
    job->fd = fd;
    job->fd_flags = 0;
    co->state = job;
    return evaio_socket_buf(fd, 0, co);
}

/* read from fd to a buffer */
int 
evaio_recv_peek(
        int fd, 
        void *buf, 
        size_t size, 
        struct conti *co)
{
    struct evaio_fd_buf_job *job = alloca(sizeof(struct evaio_fd_buf_job));
    job->flags = EVAIO_READSOME;
    job->size = size;
    job->size_done = 0;
    job->runs = 1;
    job->buf = buf;
    job->buf_offset = 0;
    job->fd = fd;
    job->fd_flags = MSG_PEEK;
    co->state = job;
    return evaio_socket_buf(fd, 0, co);
}
