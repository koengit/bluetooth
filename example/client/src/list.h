#ifndef STACK_BLE
#define STACK_BLE
#endif

struct list {
    void* buf;
    struct list* next;
};

typedef int(*compare)(void* op1, void* op2);

struct list* newList();
//int size(struct list* pt);
//int isEmpty(struct list* pt);
void insert(struct list* pt, void* dat);
void remove(struct list* pt, compare comp,  void* data);
