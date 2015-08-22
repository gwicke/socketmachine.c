#ifndef _SOCKETMACHINE_H
#define _SOCKETMACHINE_H

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS   64 /* use 64-bit size_t */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h> /*variable-length funcs etc */
#include <math.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <alloca.h>     /* alloca- allocate on stack */
#include <sys/queue.h>  /* TAILQ_* and so on */
#include <sys/signal.h> /* signal handler */

#include <netinet/in.h> /* for htonl */
#include <netdb.h>      /* addrinfo */
#include <sys/types.h>  /* for socket */
#include <sys/socket.h> /* for socket */

#include <arpa/inet.h>  /* for inet_ntop */

#include <strings.h>    /* for bzero */
#include <string.h>     /* for strerror */

#include <errno.h>

#include <unistd.h>     /* for write */
#include <fcntl.h>
#define min(a,b)   (((a) < (b)) ? (a) : (b))
#define max(a,b)   (((a) > (b)) ? (a) : (b))

/* the libevent library
http://www.monkey.org/~provos/libevent/
*/
#include <event.h>

//#include <evaio.c>

/* from unp.h */
/* Miscellaneous constants */
#define MAXLINE         4096    /* max text line length */
#define BUFFSIZE        4096    /* buffer size for reads and writes */
#define SERV_PORT       9877    /* TCP and UDP client-servers */

/* Following could be derived from SOMAXCONN in <sys/socket.h>, but many
   kernels still #define it as 5, while actually supporting many more */
#define LISTENQ         1024    /* 2nd argument to listen() */

/* POSIX requires that an #include of <poll.h> DefinE INFTIM, but many
   systems still DefinE it in <sys/stropts.h>.  We don't want to include
   all the STREAMS stuff if it's not needed, so we just DefinE INFTIM here.
   This is the standard value, but there's no guarantee it is -1. */
#ifndef INFTIM
#define INFTIM          (-1)    /* infinite poll timeout */
#endif


#define LISTENQ         1024    /* 2nd argument to listen() */
#define PORT            9999

/* branch prediction macros stolen from the kernel */
#define likely(x)       __builtin_expect((int)(x),1)
#define unlikely(x)     __builtin_expect((int)(x),0)

/* basic data for the connection */
struct connection {
	int fd;
	struct sockaddr remote_addr;
	struct sockaddr local_addr;
};


struct boxval {
	TAILQ_ENTRY(boxval) link;
	int ret;
	struct thread_state *ts;
	void *data;
	void *assoc;
};

/* state global to the 'thread' (in the thread of execution meaning, governed
 * by one event */
struct thread_state {
	struct event ev;

	/* only used in exactly one continuation context at a time, so can be
	 * lifted to thread-global on heap to keep conti in one cache line */
	int in_ret; /* return value */
	struct thread_state *in_ts; /* caller function or the callee calling back after blocking */
	void *in_data; /* data passed in from caller or callee */

	struct connection *conn;
	void *aux;
	void *cbfree_aux;
};


/* tracking of threads of control started from a continuation */
TAILQ_HEAD (inboxq, boxval);
TAILQ_HEAD (outboxq, boxval);
struct child_threads {
	unsigned int awaits; /* how many scheduled requests need to be received before await returns */
	unsigned int started; /* how many child threads are outstanding */
	struct inboxq inbox; /* results received so far before the await condition is true */
	struct outboxq outbox; /* refs to thread_state of scheduled child threads */
};


/* flags for struct conti */
#define SM_HEAP             0x01 /* conti and its state allocated on heap, free self on SM_RETURN */
#define SM_NOFREE           0x02 /* do not free self on SM_RETURN */
#define SM_CONNECTION_ROOT  0x04 /* conti is the root of the connection, free ts->conn on SM_RETURN */
#define SM_THREAD_ROOT      0x08 /* conti is root of thread, free ts on SM_RETURN */

/* The basic continuation passed to each func so it knows where and with which
 * state to continue. Size is optimized to fit in one cache line on most current
 * processors (8 words, pentium m, amd opteron etc) */
struct conti {
	void *state; /* for current op state, allocated/deallocated in func */

	struct thread_state *ts; /* data related to the thread of execution/event */

	unsigned int flags;
	struct conti *cbconti; /* parent continuation, for callbacks etc */
	void (*cb)(int /*fd*/, short/*event*/, void* /*cbconti*/); /* callback on success */
	void (*eb)(int /*fd*/, short/*event*/, void* /*cbconti*/); /* errback on error */
	void *label; /* the re-entry point into the parent function */

	struct child_threads *ct; /* queues for scheduled threads calling back to this conti */
};


#define RET_UNWIND EAGAIN
#define RET_THREAD 3000 /* a thread returns to its parent */
#define RET_AWAIT_RETURN 3001 /* a child asks a parent to continue only after waiting for event */

#define SM_STATE_s(conti_pt, fun, stuff) \
	struct conti *_co = conti_pt; \
void(*_cbf) = fun; \
struct _s { \
	stuff \
}; \
struct _s *s; \
if(likely(co->label)) { \
	s = co->state; \
	goto *co->label; \
} \
/* if the continuation is on the heap, our local state will be too. */ \
if(!_co->flags & SM_HEAP) \
s = _co->state = alloca(sizeof(struct _s)); \
else \
s = _co->state = malloc(sizeof(struct _s))

/* Convenience macro for demo */
#define SM_SETUP_this(args) SM_STATE_s(args)


/* check against func too, enqueue if not return from this func */
#define SM_CALL_CHECK(co) ({ \
		/*__label__ __cont;*/ \
		if(unlikely(_co->ts->in_ret == -RET_UNWIND)) { \
		_SM_UNWIND_PREPARE(_co); \
		return _co->ts->in_ret; \
		} /*else if (unlikely(_co->ts->in_ret == -RET_AWAIT_RETURN)) { \
		    unsigned long _timeout = (unsigned long)_co->ts->in_data; \
		    _co->ts->in_data = NULL; \
		    event_add(&_co->ts->ev, _timeout); \
		    _SM_UNWIND_PREPARE(_co); \
		    _co->label = &&__cont; \
		    return -RET_UNWIND; \
		    } \
__cont:*/ \
		})


#define SM_CALL(fun, ...)   ({ \
		__label__ _cont; \
		_co->label = &&_cont; \
		/* memset(childco, 0, sizeof(struct conti)); */ \
		struct conti *childco = alloca(sizeof(struct conti)); \
		childco->cb = _cbf; \
		childco->flags = 0; \
		childco->eb = NULL; \
		childco->ct = NULL; \
		childco->label = NULL; \
		childco->ts = _co->ts; \
		childco->cbconti = _co; \
		_co->ts->in_ret = fun(__VA_ARGS__); \
		SM_CALL_CHECK(); \
		_cont:  if(_co->ts->in_ret == -RET_THREAD) {\
		struct boxval *_boxval = sm_outbox_remove(_co, _co->ts->in_ts); \
		SM_INBOX_PUSH(_co->ts->in_ret, _co->ts->in_ts, _co->ts->in_data, _boxval); \
		return 0; \
		} \
		_co->ts->in_ret;  \
		}) 


/* call a potentially blocking function and check its return value
 * if the function would block, unwind the stack and wait for callback and
 * jump to label */
#define SM_INBOX_POP() (TAILQ_REMOVE(inboxq, TAILQ_LAST(&_co->inbox, inboxq), link))
#define SM_INBOX_PUSH(inret, ints, indata, boxval) ({ \
		sm_inbox_push(_co, inret, ints, indata, boxval); \
		}) 
static inline void
sm_inbox_push(
		struct conti *co, 
		int inret, 
		struct thread_state *ints,
		void *indata,
		struct boxval *_boxval )
{       
	_boxval->ret = inret;
	_boxval->ts = ints;
	_boxval->data = indata;
	TAILQ_INSERT_TAIL(&co->ct->inbox, _boxval, link);
}

static inline void *
sm_inbox_pop(struct conti *co) {
	struct boxval *_boxval = TAILQ_FIRST(&co->ct->inbox);
	void *assoc;
	TAILQ_REMOVE(&co->ct->inbox, _boxval, link);
	co->ts->in_ret = _boxval->ret;
	co->ts->in_ts = _boxval->ts;
	co->ts->in_data = _boxval->data;
	assoc = _boxval->assoc;
	free(_boxval);
	return assoc;
}

static inline struct boxval *
sm_outbox_find(struct conti *co, struct thread_state *ts) {
	struct boxval *_boxval;
	TAILQ_FOREACH(_boxval, &co->ct->outbox, link) {
		if(_boxval->ts == ts)
			return _boxval;
	}
	return NULL;
}

static inline void *
sm_outbox_remove(struct conti *co, struct thread_state *ts) {
	struct boxval *_boxval = sm_outbox_find(co, ts);
	void *data;
	if(_boxval) {
		TAILQ_REMOVE(&co->ct->outbox, _boxval, link);
		data = _boxval->data;
		free(_boxval);  
		return data;
	} else
		return NULL;
}

	static void 
sm_outbox_push(struct conti *co, struct thread_state *ts) 
{
	struct boxval *outboxval = calloc(1,sizeof(struct boxval));
	outboxval->ts = ts;
	TAILQ_INSERT_HEAD(&co->ct->outbox, outboxval, link);
}

#define _SM_UNWIND_PREPARE(co) ({ \
		if(!(co->flags & SM_HEAP)) { \
		struct conti *newco = malloc(sizeof(struct conti)); \
		*newco = *co; \
		newco->flags |= SM_HEAP; \
		/* patch cbconti up for child, if any */ \
		if (newco->ts->in_data) \
		((struct conti *)newco->ts->in_data)->cbconti = newco; \
		/* pass on new conti to cbconti */ \
		if(newco->cbconti) { \
		newco->cbconti->ts->in_data = newco; \
		} \
		/*printf("unwinding stack %p, new co is %p\n", co, newco); */ \
		co = newco; \
		if( co->state ) { \
		typeof(*s) *newstate = malloc(sizeof(*s)); \
		*newstate = *(typeof(*s) *)co->state; \
		co->state = newstate; \
		} \
		} \
		}) 

#define SM_BLOCK(fd, what, fun, co, timeout) ({ \
		_SM_UNWIND_PREPARE(co); \
		/* printf("block %d\n", fd); */ \
		evaio_set_event(&co->ts->ev, fd, what, \
			timeout, /* in seconds */ \
			(void(*))fun, co); \
		return -RET_UNWIND; \
		})


#define SM_RETURN(status, result) ({ \
		__label__ __SM_RET_free; \
		if (co->cbconti) {  \
		co->cbconti->ts->in_data = result; \
		co->cbconti->ts->in_ts = co->ts; \
		if(co->flags & SM_HEAP) { \
		/* have to call callback explicitly */ \
		co->ts->in_ret = status; \
		if(status < 0 && co->eb) \
		(*co->eb)(fd, 0, co->cbconti); \
		else if(co->cb) \
		(*co->cb)(fd, 0, co->cbconti); \
		goto __SM_RET_free; \
		}  \
		} else if ((co->flags & (SM_HEAP|SM_NOFREE)) == SM_HEAP) { \
		/* SM_HEAP and not SM_NOFREE */ \
		__SM_RET_free:     \
		if(co->state) free(co->state); \
		if(co->ct) free(co->ct); \
		if(unlikely(co->flags & (SM_CONNECTION_ROOT | SM_THREAD_ROOT))) { \
		if(co->flags & SM_CONNECTION_ROOT) { \
			close(co->ts->conn->fd); \
			free(co->ts->conn); \
		} \
			if(co->flags & SM_THREAD_ROOT) free(co->ts); \
		} \
			free(co); \
		} \
	return status; \
})

#define SM_BLOCK_RETURN

/* Schedule a new job, with callback set to self
 * Needs to be allocated on heap.
 * Returns pointer to child continuation for identification/cancellation purposes
 * */

static inline struct thread_state *
sm_schedule(
		struct conti *co,
		void (*cbfun),
		void (*fun),
		short what,
		int *in_ret,
		void *in_data )
{
	struct conti *childco = calloc(1, sizeof(struct conti));
	childco->cb = cbfun;
	childco->cbconti = co;
	childco->flags = SM_HEAP|SM_THREAD_ROOT;
	childco->ts = calloc(1, sizeof(struct thread_state));
	childco->ts->conn = co->ts->conn;
	if(!co->ct) co->ct = calloc(1,sizeof(struct child_threads));
	/* add to outbox */
	sm_outbox_push(co, childco->ts);
	event_set(&childco->ts->ev, 0, 0, fun, childco);
	event_active(&childco->ts->ev, 0, 1);
	return childco->ts;
}


/* For symmetry with SM_AWAIT and use of automatic _co and _cbf */
#define SM_SCHEDULE(fun, what, in_ret, in_data)   ({ \
		sm_schedule(_co, _cbf, fun, what, in_ret, in_data); \
		})

#define SM_AWAIT(count)   ({ \
		__label__ _cont; \
		_co->label = &&_cont; \
		_co->ct->awaiting += count; \
		_SM_UNWIND_PREPARE(_co); \
		return -RET_UNWIND; \
	_cont:  _co->ct->awaits--; \
		/* push in any case to keep things regular */ \
		struct boxval *_boxval = sm_outbox_remove(_co, _co->ts->in_ts); \
		SM_INBOX_PUSH(_co->ts->in_ret, _co->ts->in_ts, _co->ts->in_data, _boxval); \
		if(_co->ct->awaits != 0) { \
		return 0; \
		} \
		_co->ts->in_ret; \
		})


#define SM_AWAIT_EVENT(fd, what, timeout)   ({ \
		__label__ _cont; \
		_co->label = &&_cont; \
		_SM_UNWIND_PREPARE(_co); \
		evaio_set_event(&_co->ts->ev, fd, what, \
			timeout, /* in seconds */ \
			(void(*))_cbf, _co); \
		return -RET_UNWIND; \
		_cont:  if(_co->ts->in_ret == -RET_THREAD) { \
		_co->ct->awaits--; \
		/* push in any case to keep things regular */ \
		struct boxval *_boxval = sm_outbox_remove(_co, _co->ts->in_ts); \
		SM_INBOX_PUSH(_co->ts->in_ret, _co->ts->in_ts, _co->ts->in_data, _boxval); \
		return 0; \
		} \
		})

/*PyObject *pyresult = PyEval_CallObject(params->pycbobj,pyarglist);*/
/*struct python_ref {
  PyObject *obj;
  PyObject *args;
  PyObject *kwargs;
  };*/
/*
   PyObject *pyresult = PyEval_CallObject(params->pycbobj,pyarglist);
   switch pyresult->type
generator: replace frame->obj with object->next, args/kwargs null
call gen_handler with modified co
calls next(), dispatches

func: replace if down (simply call)
generator: call if down with cbconti set
cb if up
eb if exception != generatorexit
dealloc gen, co if generatorexit


int frontend
void mainfunc
*/



#include "evaio.h"


// Convenience macros for 'clean' demo purposes
#define SM_READ(_fd, _buffer, _len) SM_CALL(evaio_read, _fd, _buffer, _len, childco)

#define SM_WRITE(_buffer, _len, _fd) SM_CALL(evaio_write, _buffer, _len, _fd, childco)

#endif // _SOCKETMACHINE_H
