
#include "stack.h"

#include <zephyr.h>

// stack for positive integers

struct stack* newStack(int capacity) {
    struct stack *pt = (struct stack*)k_malloc(sizeof(struct stack));

    pt->maxsize = capacity;
    pt->top = -1;
    pt->items = (int*)k_malloc(sizeof(int) * capacity);

    return pt;
}

int size(struct stack *pt){
    return pt->top + 1;
}

int isEmpty(struct stack *pt){
    return pt->top == -1;
}

int isFull(struct stack *pt){
    return pt->top == pt->maxsize - 1;
}

void push(struct stack *pt, int x){
    if (isFull(pt)) {
        return;
    }
    pt->items[++pt->top] = x;
}

int peek(struct stack *pt){
    return !isEmpty(pt) ? pt->items[pt->top] : -1;
}

int pop(struct stack *pt){
    if (isEmpty(pt)){
	return -1;
    }
    return pt->items[pt->top--];
}
