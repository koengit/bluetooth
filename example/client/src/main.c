
#include "api.h"
#include <sys/printk.h>
#include <zephyr.h>

struct conn* conn;
void scanned_cb2(struct value* val) {
    printk("Scan callback 2 was invoked!\n");
}

void scanned_cb(struct value* val) {
    printk("Scan callback was invoked!\n");
    scan_for_characteristic(conn, 0xff11, 0xff12, scanned_cb2);
}

void connected(struct conn* id) {
    conn = id;
    printk("Just connected, it got ID: %d\n", id->key);
    scan_for_characteristic(id, 0xff21, 0xff22, scanned_cb);
}

void disconnected(struct conn* id) {
    printk("Just disconnected, it had ID: %d\n", id->key);
    try_connect(0xffcc);
}

void main() {
    start_bt();
    register_connected_callback(connected);
    register_disconnected_callback(disconnected);
    try_connect(0xffcc);
}

