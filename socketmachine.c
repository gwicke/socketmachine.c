/* SocketMachine - demo socket ops and asynch IO */
/* This file implements a simple HTTP server */

#include "socketmachine.h"

#define perror(args) perror(args)
#define event_debug(args)
#define event_warn(args)
#define event_warnx(arg1, arg2)

/* Open a new TCP socket and set non-blocking and other options */
int new_nonblock_socket(void){
	int flags;
	int tmp_fd = socket(AF_INET,SOCK_STREAM,0);

	setsockopt (tmp_fd, SOL_SOCKET, SO_REUSEADDR ,&tmp_fd, sizeof(tmp_fd));
	setsockopt (tmp_fd, SOL_SOCKET, SO_KEEPALIVE ,&tmp_fd, sizeof(tmp_fd));

	flags = fcntl(tmp_fd, F_GETFL, 0);
	fcntl (tmp_fd, F_SETFL, flags | O_NONBLOCK);

	return tmp_fd;
}

/* handle some signals explicitly, especially SIGPIPE which is sent on
 * connection close on a queued connection (receive queue)
 * Could later be logged */
void 
signal_cb(int fd, short event, void *arg)
{
	/* Nothing done here currently, but see samples below */
	/* struct event *signal = arg;
	printf("got signal %d\n", EVENT_SIGNAL(signal)); */
	
	/* event_del(signal);
	exit(1); */
}

/* Listen and set up callback to passed-in continuation callback */
int
tcp_listen(int t_port, char *t_addr, struct conti *co)
{
	int fd;
	struct event *ev;
	fd = new_nonblock_socket();
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(t_port);
	if(t_addr == NULL)
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	else if (inet_aton(t_addr, (struct in_addr *)&addr.sin_addr.s_addr)==0) {
		perror(__func__);
		return (-1);
	}

	if (bind(fd,(struct sockaddr *)&addr, sizeof(addr)) == -1) {
		printf("bind failed.\n");
		return (-1);
	}

	if (listen(fd,LISTENQ) == -1) {
		printf("%s: listen", __func__);
		close(fd);
		return (-1);
	}

	if (fd != -1)
		printf ("Bound to port %d - Awaiting connections ... ", PORT);
	printf("end listen, fd=%d.\n", fd);

	ev = calloc(1, sizeof(struct event));
	event_set(ev, fd, EV_READ| EV_PERSIST, co->cb, co->cbconti);
	/* Add it to the active events, without a timeout */
	event_add(ev, NULL);

	if (fd == -1) {
		printf("%s: listen failed, fd=%d\n", __func__, fd);
		return (-1);
	} else
		return fd;
}

/* Open a TCP connection */
int 
tcp_connect(int fd, short what, struct conti *co, ... /*int port, char *address */) {
	va_list argp;
	SM_STATE_s(co, &tcp_connect,
			int fd;
			char *address;
			struct addrinfo *ai;
			unsigned int port;
		  );
	va_start(argp, co);
	s->address = va_arg(argp, char *);
	s->port = va_arg(argp, unsigned int);

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(s->port);
	if(s->address == NULL)
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	else if (inet_aton(s->address, (struct in_addr *)&addr.sin_addr.s_addr)==0) {
		perror(__func__);
		return (-1);
	}

	//s->ai = make_addrinfo(s->address, s->port);
	s->fd = socket(AF_INET,SOCK_STREAM,0);
	setsockopt (s->fd, SOL_SOCKET, SO_KEEPALIVE ,&s->fd, sizeof(s->fd));
	fcntl (s->fd, F_SETFL, O_NONBLOCK);

	/*if (unlikely(s->ai == NULL)) {
	  event_debug(("%s: make_addrinfo: \"%s:%d\"",
	  __func__, s->address, s->port));
	  return (-1);
	  }*/

	if (connect(s->fd, &addr, sizeof(addr)) == -1) {
#ifdef WIN32
		int tmp_error = WSAGetLastError();
		if (tmp_error != WSAEWOULDBLOCK && tmp_error != WSAEINVAL &&
				tmp_error != WSAEINPROGRESS) {
			goto error;
		}
#else
		if (errno != EINPROGRESS) {
			perror(__func__);
			goto error;
		}
#endif
	}

	/* TODO: add ai,fd to connection */
	//co->ts->conn->fd = s->fd;
	//SM_AWAIT_EVENT(s->fd, EV_WRITE|EV_TIMEOUT, 4);

	/* Maybe: Check success as per man connect:
	 * After  select(2)
	 indicates  writability,  use  getsockopt(2)  to  read  the SO_ERROR option at level SOL_SOCKET to
	 determine whether connect() completed successfully (SO_ERROR is zero) or unsuccessfully (SO_ERROR
	 is one of the usual error codes listed here, explaining the reason for the failure).
	 * Alternative: try to use the socket and fail on use */

	//freeaddrinfo(s->ai);
	SM_RETURN(s->fd, NULL);
error:
	//freeaddrinfo(s->ai);
	SM_RETURN(-1, NULL);
}

#define ASIZE 1000
int
dorequest(int fd, short what, struct conti *co) 
{
	// declare all state local to this function
	SM_STATE_s (co, &dorequest,
			char *buf;
			char *tmpbuf;
			int headlen;
			int childfd;
		   );
	s->buf = malloc(ASIZE+1);
	if(!memset(s->buf, '.', ASIZE)) {
		// allocation failed, fail request
		SM_RETURN(-1, NULL);
	}
	s->buf[ASIZE] = 0;
	// peek at headers (without consuming them)
	do {
		if(unlikely(SM_CALL(evaio_recv_peek, fd, s->buf+100, 1000-103, childco) < 0))
			// error!
			goto error;
		/* TODO: use real parser instead */
		s->headlen = (char *)memmem(s->buf+100, ASIZE-100, "\r\n\r\n", 4) - s->buf - 100+4;
		//printf("len: %d\n", s->headlen );
	} while (unlikely(0 > s->headlen || s->headlen > 1000));
	/* this will not block as we peeked at the data already
	 * so we can allocate the buffer on the stack, at least if it is not
	 * too big */
	s->tmpbuf = alloca(s->headlen);
	SM_READ(fd, s->tmpbuf, s->headlen);

	// make up some reply headers
	sprintf(s->buf, "HTTP/1.0 200 OK\r\nContent-type: text/html\r\nConnection: close\r\n\r\n<pre>");
	// and write them.
	if(!SM_WRITE(s->buf, ASIZE, fd))
		perror(__func__);

	// send reply as request to localhost:80..
	//s->childfd = SM_CALL(tcp_connect, 0, 0, childco, "127.0.0.1", 80);
	//SM_CALL(evaio_write, s->buf, ASIZE, s->childfd, childco);
	//close(s->childfd);

	free(s->buf);
	SM_RETURN(0, NULL);

error:  free(s->buf);
	SM_RETURN(-1, NULL);

}


void tcp_accept(int fd, short what, struct conti *co) {
	int sfd;
	socklen_t addrlen;
	struct connection *conn = malloc(sizeof(struct connection));
	addrlen = sizeof(struct sockaddr);

	if ((sfd = accept(fd, &conn->remote_addr, &addrlen)) >= 0) {
		if(evaio_set_nonblocking(sfd) < 0) {
			close(sfd);
			goto error;
		}
	} else if (errno != EAGAIN && errno != EWOULDBLOCK) {
		perror("accept");
		goto error;
	}

	conn->fd = sfd;

	struct thread_state *ts = calloc(1,sizeof(struct thread_state));
	ts->conn = conn;
	/* freed by handler */
	struct conti *childco = calloc(1,sizeof(struct conti));
	childco->ts = ts;
	/* dorequest will be the root of the connection and the thread state
	 * and responsible for resource deallocation */
	childco->flags = SM_HEAP | SM_CONNECTION_ROOT | SM_THREAD_ROOT;
	/* register for reading. After the accept the socket buffer is just
	 * allocated and empty. TCP slow-start follows, so a read at this
	 * stage would block. Difference in a benchmark was 48req/second (3428
	 * vs. 3475 req/sec) */
	evaio_set_event(&ts->ev, sfd, EV_READ|EV_TIMEOUT, 
			400, // timeout in seconds
			(void(*))dorequest, childco);
	return;
error:
	free(conn);
	return;
}




int main(){
	/* Initalize the event library */
	event_init();

	/* Set handler for SIGPIPE to signal_cb */
	struct event signal_int;
	event_set(&signal_int, SIGPIPE, EV_SIGNAL|EV_PERSIST, signal_cb, &signal_int);
	event_add(&signal_int, NULL);

	/* Allocate root continuation, set callback to tcp_accept */
	struct conti *co = calloc(1,sizeof(struct conti));
	co->cb = (void(*))&tcp_accept;
	tcp_listen(PORT, NULL, co);

	/* Finally, start main event loop */
	return event_dispatch();
}

