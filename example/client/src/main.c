#include "api.h"
#include <sys/printk.h>
#include <zephyr.h>

#define DEVICE                             0xffcc

#define TEMPERATURE_SENSOR_SERVICE         0xff11
#define TEMPERATURE_SENSOR_CHARACTERISTIC  0xff12

#define OCTAVIUS_SERVICE                   0xff21
#define OCTAVIUS_CHARACTERISTIC            0xff22

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

void subscribe_temperature(const void* buf, int len) {
    int* input = (int*) buf;
    func(*input, 0);
}

void subscribe_octavius(const void* buf, int len) {
    int* input = (int*) buf;
    func(-273, (*input) + 1);
}

void scanned_temperature_callback(struct value* val) {
    subscribe_characteristic(val, subscribe_temperature);
}

void scanned_octavius_callback(struct value* val) {
    subscribe_characteristic(val, subscribe_octavius);
}

void connected(struct conn* id) {
    scan_for_characteristic(id,
		            OCTAVIUS_SERVICE, 
			    OCTAVIUS_CHARACTERISTIC, 
			    scanned_octavius_callback);
    scan_for_characteristic(id,
		            TEMPERATURE_SENSOR_SERVICE, 
			    TEMPERATURE_SENSOR_CHARACTERISTIC, 
			    scanned_temperature_callback);
}

void disconnected(struct conn* id) {
    try_connect(DEVICE);
}

void main() {
    main_mem = (struct blexa_mem*) k_malloc (sizeof(struct blexa_mem));
    blexa_reset(main_mem);

    start_bt();
    register_connected_callback(connected);
    register_disconnected_callback(disconnected);
    try_connect(DEVICE);
}

