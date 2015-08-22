#include <obstack.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free


/* tracking of threads of control started from a continuation */
TAILQ_HEAD (inboxq, boxval);
TAILQ_HEAD (outboxq, boxval);
struct child_threads {
	unsigned int awaits; /* how many scheduled requests need to be received before await returns */
	unsigned int started; /* how many child threads are outstanding */
	struct inboxq inbox; /* results received so far befor the await condition is true */
	struct outboxq outbox; /* refs to thread_state of scheduled child threads */
};


/* all possible default watcher types */
union ev_thread_watcher
{
	struct ev_watcher w;
	struct ev_watcher_list wl;

	struct ev_io io;
	struct ev_timer timer;
	struct ev_periodic periodic;
	struct ev_child child;
#if EV_IDLE_ENABLE
	struct ev_idle idle;
#endif
	struct ev_prepare prepare;
	struct ev_check check;
#if EV_FORK_ENABLE
	struct ev_fork fork;
#endif
#if EV_ASYNC_ENABLE
	struct ev_async async;
#endif
};

/* all state associated with a thread, including two watchers */
struct thread {
	union ev_thread_watcher watch; /* ->data is conti */
	ev_timer timer;
	/* thread-global variables */
	struct obstack stack; /* init with obstack_init (struct obstack *obstack-ptr) */
	/* the currently-active continuation */
	struct conti *co;
	/* callback for the thread, invoked when the uppermost conti finds a
	 * null link */
	struct conti *link;
	unsigned int inboxlen; /* how many scheduled requests need to be received before await returns */
	unsigned int outboxlen; /* how many child threads are outstanding */
	struct inboxq inbox; /* results received so far befor the await condition is true */
	struct outboxq outbox; /* refs to thread_state of scheduled child threads */
}

/* the basic continuation, first part of each local state struct */
struct conti {
	unsigned int flags; /* connection etc */

	/* the local function */
	void (*fun)(EV_P_ ev_watcher * /*w*/, int /*revents*/);
	void *label; /* the re-entry point into the function (computed goto) */

	struct conti *link; /* parent continuation */
	struct thread *thread; /* the associated thrad */
};

#define setup_locals(thread, _fun) do { \
		struct _fun *_l = obstack_alloc(&thread->stack, sizeof(struct _fun)); \
		_l->_co->link = (ev_watcher *)w->data; /* current data */ \
		_l->_co->fun = _fun; /* keep fun pointer for call and callback */ \
		_l->_co->label = NULL; \
		_l; } while(0)

/* locals can be cast to conti */
#define ev_get_locals(watcher, threadvar, localsvar) \
		/* autocast from void to local state struct * */ \
		localsvar = watcher->data;\
		/* create a consistent continuation alias for other macros to use */ \
		struct conti *__co = watcher->data; \
		threadvar = __co->thread; \
		if (__co->label != NULL) \
			goto *(__co->label;

#define spawn(fun, args)	do { \
	struct thread *__nt = obstack_alloc(&__thread->stack, sizeof(struct thread)); \
	((ev_watcher *)__nt)->data = __nt->co = \
		(struct conti *)obstack_alloc(&__nt->stack, sizeof(struct _fun)); \
	__nt->link = __co; \
	_setup_fun((struct _fun *)__nt->co, args); \
	/* run the thread immediately */ \
	ev_init((ev_watcher *)__nt, fun); \
	ev_feed_event (EV_A_ (ev_watcher *)__nt, int revents); \
	__nt; \
	} while(0)



/*
 ********************
 * op evread 
 ********************
 */
static void _evread (EV_P_ ev_io *w, int revents);

/* continuation and local state, in one struct allocated on call */
struct _evio_read {
	struct conti _co;
	int x;
	/* some more */
};


/* setup function usable for scheduling and direct calls */
static void
_setup_evio_read(struct _evio_read *r, int x) {    
	r->x = _x;
	return r;
}

#define evio_read(args)  CALL(_evio_read, _setup_evio_read(args))


int
_evio_read (EV_P_ struct ev_watcher *w, int revents) {
	ev_get_locals(w, struct thread * t; struct _evio_read *l);

	evio_read(2);
	// do something
	// w->data->_co->link is the linked conti
}

/* watcher -> data -> thread */
