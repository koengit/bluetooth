#ifndef STACK_BLE
#define STACK_BLE
#endif

struct stack
{
	int maxsize;	// define max capacity of stack
	int top;		
	int *items;
};

struct stack* newStack(int capacity);
int size(struct stack *pt);
int isEmpty(struct stack *pt);
int isFull(struct stack *pt);
void push(struct stack *pt, int x);
int peek(struct stack *pt);
int pop(struct stack *pt);
