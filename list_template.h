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


struct kv_node {
        TAILQ_ENTRY(kv_node) link;
        unsigned long fill;
        struct kv items[BLOCKITEMS];
};

#define LIST_COMMON_HEAD \
	struct kv_node *tqh_first;		/* first element */		\
	struct kv_node **tqh_last;	/* addr of last next element */	\
        unsigned long length;	\
	unsigned short blockitems; \
	unsigned short itemsize

struct list_head {
	LIST_COMMON_HEAD;
};


#define init_list(headptr) do {\
        TAILQ_INIT(headptr); \
        headptr->node.fill = 0; \
	headptr->itemsize = sizeof(headptr->node.items[0]); \
        TAILQ_INSERT_HEAD(headptr, &head->node, link); \
	} while(0)



struct list_head * list_new();

void list_free(struct list_head *head);


void list_append(struct list_head *head, void *item);

void list_insert(struct list_head *head, int index, void *item);

int list_get(struct list_head *head, int index, void *result);

int list_remove(struct list_head *head, int index);


/* exits loop if the return value of fun is non-zero */
//void list_foreach(struct list_head *head, int(*fun)(char *key, char *val));

#define declare_list(name, list_head_type, blockitems) \
	struct name_kv_node { \
		TAILQ_ENTRY(name_kv_node) link; \
		unsigned long fill; \
		list_head_type items[blockitems]; \
	}; \
	struct name { \
		LIST_COMMON_HEAD; \
		struct name_kv_node node; \
	}; \
\
void  \
name_list_append(list_head_type *head, void *item)  \
{ \
        struct name_kv_node *lastnode = TAILQ_LAST(head, list_head_type); \
        if (lastnode->fill < BLOCKITEMS) { \
                lastnode->items[lastnode->fill] = *(typeof(&lastnode->items[0]))item; \
                lastnode->fill++; \
        } else { \
                struct name_kv_node *newnode = malloc(sizeof(struct name_kv_node)); \
                newnode->items[0] = *(typeof(head->node.items[0])*)item; \
                newnode->fill = 1; \
                TAILQ_INSERT_TAIL(head, newnode, link); \
        } \
        head->length++; \
} \
 \
void name_list_prepend(list_head_type *head, void *item)  \
{ \
        /* The first node in the queue is filled downwards */ \
        struct name_kv_node *node = TAILQ_FIRST(head); \
        if (node->fill < BLOCKITEMS) { \
                node->items[BLOCKITEMS - 1 - node->fill] = *(typeof(head->node.items[0])*)item; \
                node->fill++; \
        } else { \
                struct name_kv_node *newnode = malloc(sizeof(struct name_kv_node)); \
void  \
name_list_append(list_head_type *head, void *item)  \
{ \
        struct name_kv_node *lastnode = TAILQ_LAST(head, list_head_type); \
        if (lastnode->fill < BLOCKITEMS) { \
                lastnode->items[lastnode->fill] = *(typeof(lastnode->items[0])*)item; \
                lastnode->fill++; \
        } else { \
                struct name_kv_node *newnode = malloc(sizeof(struct name_kv_node)); \
                newnode->items[0] = *(typeof(head->node.items[0])*)item; \
                newnode->fill = 1; \
                TAILQ_INSERT_TAIL(head, newnode, link); \
        } \
        head->length++; \
} \
 \
void name_list_prepend(list_head_type *head, void *item)  \
{ \
        /* The first node in the queue is filled downwards */ \
        struct name_kv_node *node = TAILQ_FIRST(head); \
        if (node->fill < BLOCKITEMS) { \
                node->items[BLOCKITEMS - 1 - node->fill] = *(typeof(head->node.items[0])*)item; \
                node->fill++; \
        } else { \
                struct name_kv_node *newnode = malloc(sizeof(struct name_kv_node)); \
                newnode->items[BLOCKITEMS - 1].key = *(typeof(head->node.items[0])*)item; \
                newnode->fill = 1; \
                TAILQ_INSERT_HEAD(head, newnode, link); \
        } \
        head->length++; \
} \
 \
void \
name_node_for_index(list_head_type *head, int index, struct name_kv_node **node_ptr, int *node_index) { \
        long i = 0; \
        struct name_kv_node *node = TAILQ_FIRST(head); \
        struct name_kv_node *end = TAILQ_LAST(head, list_head_type); \
         \
        if (node->fill > index) { \
                *node_ptr = node; \
                *node_index = index; \
                return; \
        } else { \
                i += node->fill; \
        } \
 \
        while(i < index && node != end) { \
                node = TAILQ_NEXT(node, link); \
                i += node->fill; \
        } \
 \
        if(i < index) { \
                *node_ptr = NULL; \
                *node_index = 0; \
        } else { \
                *node_ptr = node; \
                *node_index = index - (i - node->fill); \
        } \
} \
 \
 \
void \
name_list_insert(list_head_type *head, int index, void *item) { \
        int i = 0; \
        int node_index; \
        struct name_kv_node *node, *newnode; \
        /*if (index == 0) \
                return list_prepend(head, key, val); \
        else */if (index == head->length) \
                return name_list_append(head, key, val); \
        node_for_index(head, index, &node, &node_index); \
        if(!node) \
                return; \
        if (node->fill < BLOCKITEMS) { \
                for (i=node->fill; i>node_index; i--) \
                        node->items[i] = node->items[i-1]; \
                node->items[node_index] = *(typeof(head->node.items[0])*)item; \
                node->fill++; \
        } else if(unlikely(TAILQ_NEXT(node, link) && TAILQ_NEXT(node, link)->fill < BLOCKITEMS)) { \
                /* move item to next */ \
                newnode = TAILQ_NEXT(node, link); \
                for (i=newnode->fill; i>0; i--) \
                        newnode->items[i] = newnode->items[i-1]; \
                newnode->items[0] = node->items[node->fill - 1]; \
                newnode->fill++; \
                for (i=node->fill-1; i > node_index; i--) \
                        node->items[i] = node->items[i-1]; \
                node->items[node_index] = *(typeof(head->node.items[0])*)item; \
        } else if (unlikely(TAILQ_PREV(node, list_head_type, link)  \
                        && TAILQ_PREV(node, list_head_type, link)->fill < BLOCKITEMS)) { \
                newnode = TAILQ_PREV(node, list_head_type, link); \
                newnode->items[newnode->fill] = node->items[0]; \
                newnode->fill++; \
                for(i=0; i<node_index; i++) \
                        node->items[i] = node->items[i+1]; \
                node->items[node_index] = *(typeof(head->node.items[0])*)item; \
        } else { \
                newnode = malloc(sizeof(struct name_kv_node)); \
                newnode->items[0] = node->items[node->fill - 1]; \
                newnode->fill = 1; \
                TAILQ_INSERT_AFTER(head, node, newnode, link); \
                for (i=node->fill-1; i > node_index; i--) \
                        node->items[i] = node->items[i-1]; \
                node->items[node_index] = *(typeof(head->node.items[0])*)item; \
        } \
                 \
        head->length++; \
} \
 \
int \
name_list_remove(list_head_type *head, int index) { \
        int i; \
        int node_index; \
        struct name_kv_node *node; \
        node_for_index(head, index, &node, &node_index); \
        if(!node) \
                return -1; \
        for(i=node->fill-1; i>node_index; i--) \
                node->items[i-1] = node->items[i]; \
        node->fill--; \
        head->length--; \
        if(node->fill == 0 && node != TAILQ_FIRST(head)) { \
                TAILQ_REMOVE(head, node, link); \
                free(node); \
        } \
        return 0; \
} \
 \
 \
int \
name_list_get(list_head_type *head, int index, void *result)  \
{ \
        int node_index; \
        struct name_kv_node *node; \
        node_for_index(head, index, &node, &node_index); \
        if(!node) { \
                return -1; \
        } else { \
		*(typeof(head->node.items[0])*)item = node->items[node_index]; \
                return 0; \
        } \
} \
                newnode->items[BLOCKITEMS - 1].key = *(typeof(head->node.items[0])*)item; \
                newnode->fill = 1; \
                TAILQ_INSERT_HEAD(head, newnode, link); \
        } \
        head->length++; \
} \
 \
void \
name_node_for_index(list_head_type *head, int index, struct name_kv_node **node_ptr, int *node_index) { \
        long i = 0; \
        struct name_kv_node *node = TAILQ_FIRST(head); \
        struct name_kv_node *end = TAILQ_LAST(head, list_head_type); \
         \
        if (node->fill > index) { \
                *node_ptr = node; \
                *node_index = index; \
                return; \
        } else { \
                i += node->fill; \
        } \
 \
        while(i < index && node != end) { \
                node = TAILQ_NEXT(node, link); \
                i += node->fill; \
        } \
 \
        if(i < index) { \
                *node_ptr = NULL; \
                *node_index = 0; \
        } else { \
                *node_ptr = node; \
                *node_index = index - (i - node->fill); \
        } \
} \
 \
 \
void \
name_list_insert(list_head_type *head, int index, void *item) { \
        int i = 0; \
        int node_index; \
        struct name_kv_node *node, *newnode; \
        /*if (index == 0) \
                return list_prepend(head, key, val); \
        else */if (index == head->length) \
                return name_list_append(head, key, val); \
        node_for_index(head, index, &node, &node_index); \
        if(!node) \
                return; \
        if (node->fill < BLOCKITEMS) { \
                for (i=node->fill; i>node_index; i--) \
                        node->items[i] = node->items[i-1]; \
                node->items[node_index] = *(typeof(head->node.items[0])*)item; \
                node->fill++; \
        } else if(unlikely(TAILQ_NEXT(node, link) && TAILQ_NEXT(node, link)->fill < BLOCKITEMS)) { \
                /* move item to next */ \
                newnode = TAILQ_NEXT(node, link); \
                for (i=newnode->fill; i>0; i--) \
                        newnode->items[i] = newnode->items[i-1]; \
                newnode->items[0] = node->items[node->fill - 1]; \
                newnode->fill++; \
                for (i=node->fill-1; i > node_index; i--) \
                        node->items[i] = node->items[i-1]; \
                node->items[node_index] = *(typeof(head->node.items[0])*)item; \
        } else if (unlikely(TAILQ_PREV(node, list_head_type, link)  \
                        && TAILQ_PREV(node, list_head_type, link)->fill < BLOCKITEMS)) { \
                newnode = TAILQ_PREV(node, list_head_type, link); \
                newnode->items[newnode->fill] = node->items[0]; \
                newnode->fill++; \
                for(i=0; i<node_index; i++) \
                        node->items[i] = node->items[i+1]; \
                node->items[node_index] = *(typeof(head->node.items[0])*)item; \
        } else { \
                newnode = malloc(sizeof(struct name_kv_node)); \
                newnode->items[0] = node->items[node->fill - 1]; \
                newnode->fill = 1; \
                TAILQ_INSERT_AFTER(head, node, newnode, link); \
                for (i=node->fill-1; i > node_index; i--) \
                        node->items[i] = node->items[i-1]; \
                node->items[node_index] = *(typeof(head->node.items[0])*)item; \
        } \
                 \
        head->length++; \
} \
 \
int \
name_list_remove(list_head_type *head, int index) { \
        int i; \
        int node_index; \
        struct name_kv_node *node; \
        node_for_index(head, index, &node, &node_index); \
        if(!node) \
                return -1; \
        for(i=node->fill-1; i>node_index; i--) \
                node->items[i-1] = node->items[i]; \
        node->fill--; \
        head->length--; \
        if(node->fill == 0 && node != TAILQ_FIRST(head)) { \
                TAILQ_REMOVE(head, node, link); \
                free(node); \
        } \
        return 0; \
} \
 \
 \
int \
name_list_get(list_head_type *head, int index, void *result)  \
{ \
        int node_index; \
        struct name_kv_node *node; \
        node_for_index(head, index, &node, &node_index); \
        if(!node) { \
                return -1; \
        } else { \
		*(typeof(head->node.items[0])*)item = node->items[node_index]; \
                return 0; \
        } \
}
