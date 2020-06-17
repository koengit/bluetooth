#ifndef API_BLE
#define API_BLE
#endif

/* misc */
void start_bt();

/* Connection management */
typedef void(*conn_cb)(int);

void register_connected_callback(conn_cb cb);
void unregister_connected_callback();

void register_disconnected_callback(conn_cb cb);
void unregister_disconnected_callback();

void try_connect(int uuid_in_hex);
