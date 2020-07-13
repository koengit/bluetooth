
#include <zephyr.h>
#include "list.h"

struct list* newList() {
    struct list* new = k_malloc(sizeof(struct list));
    return new;
}

/*int size(struct list* pt) {
    int size = 0;
    pt = pt->next;
    while(pt) {
        size++;
	pt = pt->next;
    }
    return size;
}

int isEmpty(struct list* pt) {
    return pt->next == NULL;
}*/

void insert(struct list* pt, void* data) {
    while(pt->next) {
        pt = pt->next;
    }
    struct list* new = k_malloc(sizeof(struct list));
    new->buf = data;
    pt->next = new;
}

void remove(struct list* pt, compare comp, void* data) {
    while(pt->next) {
        if(comp(pt->next->buf, data)/*pt->next->buf == data*/) {
            struct list* old = pt->next;
	    struct list* new_next = pt->next->next;
	    pt->next = new_next;
	    k_free(old->buf);
	    k_free(old);
	}
    }
}
