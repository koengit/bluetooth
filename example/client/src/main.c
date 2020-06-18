
#include "api.h"
#include <sys/printk.h>

void connected(int id) {
    printk("Just connected, it got ID: %d\n", id);
}

void disconnected(int id) {
    printk("Just disconnected, it got ID: %d\n", id);
}

void main() {
    start_bt();
    register_connected_callback(connected);
    register_disconnected_callback(disconnected);
    try_connect(0xffcc);
}

