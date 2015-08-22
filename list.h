#include <sys/queue.h>
#include <stdlib.h>
#include <stdio.h>
#include "socketmachine.h"

struct kv {
        char *key;
        char *val;
};

/* should work out to multiples of cache lines with fill and link pointers
 * considered (2 pointers, one long */
#define BLOCKITEMS  14

struct list_head {								\
	struct kv_node *tqh_first;		/* first element */		\
	struct kv_node **tqh_last;	/* addr of last next element */	\
        unsigned long length;
};

struct kv_node {
        TAILQ_ENTRY(kv_node) link;
        unsigned long fill;
        struct kv items[BLOCKITEMS];
};


inline struct list_head * list_new();

inline void list_free(struct list_head *head);


void list_append(struct list_head *head, char *key, char *val);

void list_insert(struct list_head *head, int index, char *key, char *val);

int list_get(struct list_head *head, int index, struct kv *result);

int list_remove(struct list_head *head, int index);


/* exits loop if the return value of fun is non-zero */
void list_foreach(struct list_head *head, int(*fun)(char *key, char *val));

