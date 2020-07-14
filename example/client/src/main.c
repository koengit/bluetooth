
#include "api.h"
#include <sys/printk.h>
#include <zephyr.h>

/********************/
struct blexa_mem
{ int l;
};

struct blexa_mem *main_mem;

void (blexa_reset(struct blexa_mem (* self))) {
  ((*(self)).l) = (0);
}

int (blexa_step(struct blexa_mem (* self), int a, int b)) {
  int j;
  int d;
  int m;
  int k;
  int g;
  int f;
  int e;
  int c;
  int o;
  int n;
  int h;
  switch (b) {
    case (2): (j) = (2); break;
    case (1): (j) = (1); break;
    case (0): (j) = (0); break;
  };
  (f) = ((*(self)).l);
  (d) = (j);
  switch (d) {
    case (2): (k) = (1); break;
    case (1): (k) = (0); break;
    case (0): (k) = (f); break;
  };
  (e) = (k);
  switch (b) {
    case (2): (m) = (1); break;
    case (1): (m) = (0); break;
    case (0): (m) = (e); break;
  };
  (g) = (m);
  ((*(self)).l) = (e);
  (c) = ((a) > (30));
  switch (c) {
    case (1): (n) = (1); break;
    case (0): (n) = (0); break;
  };
  switch (g) {
    case (1): (o) = (n); break;
    case (0): (o) = (2); break;
  };
  (h) = (o);
  return h;
}

int func(int temp, int door) {
    int x = blexa_step(main_mem, temp, door);
    printk("temperature: %d octavius %d windowCommand %d\n",temp, door, x);
    return 0;
}
/********************/

struct conn* conn;
struct value* vall;
int i = 0;

void subscribe_temperature(const void* buf, int len) {
    int* input = (int*) buf;
    //int x = blexa_step(main_mem, *input, 0);
    func(*input, 0);
}

void scanned_cb2(struct value* val) {
    printk("Scan callback 2 was invoked!\n");
    subscribe_characteristic(conn, val, subscribe_temperature);
}

void subscribe_octavius(const void* buf, int len) {
    int* input = (int*) buf;
    func(-273, (*input) + 1);
    if(i > 10) {
        unsubscribe_characteristic(conn, vall);
        unsubscribe_characteristic(conn, vall);
    } else {
        printk("i = %d\n", i);
        i++;
    }
    //int x = blexa_step(main_mem, -273, (*input) + 1);
}

void scanned_cb(struct value* val) {
    printk("Scan callback was invoked!\n");
    vall = val;
    subscribe_characteristic(conn, val, subscribe_octavius);
}

void connected(struct conn* id) {
    conn = id;
    printk("Just connected, it got ID: %d\n", id->key);
    scan_for_characteristic(id, 0xff21, 0xff22, scanned_cb);
    scan_for_characteristic(conn, 0xff11, 0xff12, scanned_cb2);
}

void disconnected(struct conn* id) {
    printk("Just disconnected, it had ID: %d\n", id->key);
    try_connect(0xffcc);
}

void main() {
    main_mem = (struct blexa_mem*) k_malloc (sizeof(struct blexa_mem));
    blexa_reset(main_mem);

    start_bt();
    register_connected_callback(connected);
    register_disconnected_callback(disconnected);
    try_connect(0xffcc);
}

