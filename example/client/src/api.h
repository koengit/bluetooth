#ifndef API_BLE
#define API_BLE
#endif

/* misc */
void start_bt();

/* Connection management */
struct conn {
    int key;
};

typedef void(*conn_cb)(struct conn* con);

void register_connected_callback(conn_cb cb);
void unregister_connected_callback();

void register_disconnected_callback(conn_cb cb);
void unregister_disconnected_callback();

void try_connect(int uuid_in_hex);

/* Characteristic management */
struct value {
    int service_uuid;
    int characteristic_uuid;
    int characteristic_handle;
    void* conn;
    void* subscribe_params;
};

typedef void(*scan_cb)(struct value* val);

void scan_for_characteristic(struct conn* conn, int service_in_hex, int characteristic_in_hex, scan_cb cb);

typedef void(subscribed_cb)(const void* buf, int len); // should probably (definitely) be u16_t
int subscribe_characteristic(struct value* val, subscribed_cb cb);
int unsubscribe_characteristic(struct value* val);
