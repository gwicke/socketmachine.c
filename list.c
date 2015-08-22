#include "list.h"

inline void 
list_append(struct list_head *head, char *key, char *val) 
{
        struct kv_node *lastnode = TAILQ_LAST(head, list_head);
        if (lastnode->fill < BLOCKITEMS) {
                lastnode->items[lastnode->fill].key = key;
                lastnode->items[lastnode->fill].val = val;
                lastnode->fill++;
        } else {
                struct kv_node *newnode = malloc(sizeof(struct kv_node));
                newnode->items[0].key = key;
                newnode->items[0].val = val;
                newnode->fill = 1;
                TAILQ_INSERT_TAIL(head, newnode, link);
        }
        head->length++;
}

void list_prepend(struct list_head *head, char *key, char *val) 
{
        /* The first node in the queue is filled downwards */
        struct kv_node *node = TAILQ_FIRST(head);
        if (node->fill < BLOCKITEMS) {
                node->items[BLOCKITEMS - 1 - node->fill].key = key;
                node->items[BLOCKITEMS - 1 - node->fill].val = val;
                node->fill++;
        } else {
                struct kv_node *newnode = malloc(sizeof(struct kv_node));
                newnode->items[BLOCKITEMS - 1].key = key;
                newnode->items[BLOCKITEMS - 1].val = val;
                newnode->fill = 1;
                TAILQ_INSERT_HEAD(head, newnode, link);
        }
        head->length++;
}

static inline void
node_for_index(struct list_head *head, int index, struct kv_node **node_ptr, int *node_index) {
        long i = 0;
        struct kv_node *node = TAILQ_FIRST(head);
        struct kv_node *end = TAILQ_LAST(head, list_head);
        
        if (node->fill > index) {
                *node_ptr = node;
                *node_index = index;
                return;
        } else {
                i += node->fill;
        }

        while(i < index && node != end) {
                node = TAILQ_NEXT(node, link);
                i += node->fill;
        }

        if(i < index) {
                *node_ptr = NULL;
                *node_index = 0;
        } else {
                *node_ptr = node;
                *node_index = index - (i - node->fill);
        }
}


void
list_insert(struct list_head *head, int index, char *key, char *val) {
        int i = 0;
        int node_index;
        struct kv_node *node, *newnode;
        /*if (index == 0)
                return list_prepend(head, key, val);
        else */if (index == head->length)
                return list_append(head, key, val);
        node_for_index(head, index, &node, &node_index);
        if(!node)
                return;
        if (node->fill < BLOCKITEMS) {
                for (i=node->fill; i>node_index; i--)
                        node->items[i] = node->items[i-1];
                node->items[node_index].key = key;
                node->items[node_index].val = val;
                node->fill++;
        } else if(unlikely(TAILQ_NEXT(node, link) && TAILQ_NEXT(node, link)->fill < BLOCKITEMS)) {
                /* move item to next */
                newnode = TAILQ_NEXT(node, link);
                for (i=newnode->fill; i>0; i--)
                        newnode->items[i] = newnode->items[i-1];
                newnode->items[0] = node->items[node->fill - 1];
                newnode->fill++;
                for (i=node->fill-1; i > node_index; i--)
                        node->items[i] = node->items[i-1];
                node->items[node_index].key = key;
                node->items[node_index].val = val;
        } else if (unlikely(TAILQ_PREV(node, list_head, link) 
                        && TAILQ_PREV(node, list_head, link)->fill < BLOCKITEMS)) {
                newnode = TAILQ_PREV(node, list_head, link);
                newnode->items[newnode->fill] = node->items[0];
                newnode->fill++;
                for(i=0; i<node_index; i++)
                        node->items[i] = node->items[i+1];
                node->items[node_index].key = key;
                node->items[node_index].val = val;
        } else {
                newnode = malloc(sizeof(struct kv_node));
                newnode->items[0] = node->items[node->fill - 1];
                newnode->fill = 1;
                TAILQ_INSERT_AFTER(head, node, newnode, link);
                for (i=node->fill-1; i > node_index; i--)
                        node->items[i] = node->items[i-1];
                node->items[node_index].key = key;
                node->items[node_index].val = val;
        }
                
        head->length++;
}

int
list_remove(struct list_head *head, int index) {
        int i;
        int node_index;
        struct kv_node *node;
        node_for_index(head, index, &node, &node_index);
        if(!node)
                return -1;
        for(i=node->fill-1; i>node_index; i--)
                node->items[i-1] = node->items[i];
        node->fill--;
        head->length--;
        if(node->fill == 0 && node != TAILQ_FIRST(head)) {
                TAILQ_REMOVE(head, node, link);
                free(node);
        }
        return 0;
}


int
list_get(struct list_head *head, int index, struct kv *result) 
{
        int node_index;
        struct kv_node *node;
        node_for_index(head, index, &node, &node_index);
        if(!node) {
                return -1;
        } else {
                result->key = node->items[node_index].key;
                result->val = node->items[node_index].val;
                return 0;
        }
}

void
list_foreach(struct list_head *head, int(*fun)()) {
        struct kv_node *node;
        int i;
        TAILQ_FOREACH(node, head, link) {
                for(i=0; i<node->fill; i++) {
                        if((*fun)(node->items[i].key, node->items[i].val))
                                goto out;
                }
        }
out:
        return;
}
/*
#define _LIST_FOREACH(head, var) \
##ifndef _have_list_foreach_macro_vars \
##define  _have_list_foreach_macro_vars \
                struct kv_node *_n; \
                int _i = 0; \
##endif \
                for(_n = TAILQ_FIRST(head), var = &_n->items[0]; \
                        _n && i < _n->fill; \
                        ({if (i == _n->fill - 1) {  \
                            _n = TAILQ_NEXT(_n, link); \
                            _i = 0; \
                        } else { \
                            _i++; \
                        } \
                        var = &_n->items[_i]; \
                        }) \
                )
*/
int
printkv(char *key, char *val) {
        printf("key: %s, val: %s\n", key, val);
        return 0;
}
        

inline struct list_head *
list_new()
{
        /* optimization: use a single malloc for head and first node */
        struct list_head *head = malloc(sizeof(struct list_head) + sizeof(struct kv_node));
        struct kv_node *node = (struct kv_node *)((char *)head + sizeof(struct list_head));
        TAILQ_INIT(head);
        node->fill = 0;
        TAILQ_INSERT_HEAD(head, node, link);
        return head;
}

inline void
list_free(struct list_head *head) {
        struct kv_node *node, *oldnode;
        node = TAILQ_NEXT(TAILQ_FIRST(head), link);
        while(node) {
                oldnode = node;
                node = TAILQ_NEXT(node, link);
                free(oldnode);
        }
        free(head);
}


int main() {
        int i;
        struct kv *kv;
        char *key = calloc(1,32);
        char *val = calloc(1,32);
        sprintf(key, "Some key");
        sprintf(val, "Some value");
        for(i=0;i<10000000;i++) {
                struct list_head *head = list_new();
                int j;
                for(j=0; j<10; j+=2) {
                    list_append(head, key, val);
                    list_append(head, key, val);
                }
                list_insert(head, 2, key, val);
                list_insert(head, 0, key, val);
                list_insert(head, 9, key, val);
                list_insert(head, 4, key, val);
                list_remove(head, 0);
                /*list_foreach(head, printkv);
                
                struct kv *var;
                
                _LIST_FOREACH(head, var)
                {
                        printf("k: %s, v: %s\n", var->key, var->val);
                }
                */
                list_free(head);
        }
        free(key);
        free(val);
        return 0;
}

