/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr.h>
#include <sys/printk.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <sys/byteorder.h>

#include "api.h"
#include "stack.h"
#include "list.h"

#define MAX_CONNECTIONS 5

#define BT_UUID_DEVICE                             BT_UUID_DECLARE_16(0xffcc)

#define BT_UUID_TEMPERATURE_SENSOR_SERVICE         BT_UUID_DECLARE_16(0xff11)
#define BT_UUID_TEMPERATURE_SENSOR_CHARACTERISTIC  BT_UUID_DECLARE_16(0xff12)

#define BT_UUID_OCTAVIUS_SERVICE                   BT_UUID_DECLARE_16(0xff21)
#define BT_UUID_OCTAVIUS_CHARACTERISTIC            BT_UUID_DECLARE_16(0xff22)

/*********** Connection management ***********/
struct stack* stack;
struct bt_conn* conns[MAX_CONNECTIONS];
struct conn* apiconns[MAX_CONNECTIONS];

int set_conn(struct bt_conn* conn) {
    int index = pop(stack);
    if(index != -1) {
        conns[index] = conn;
	return index;
    } else {
        return -1;
    }
}

int get_slot() {
    return pop(stack);
}

struct bt_conn* get_conn(int key) {
    struct bt_conn* res = NULL;
    if(0 <= key && key < MAX_CONNECTIONS) {
        res = conns[key];
    }
    return res;
}

int get_key(struct bt_conn* conn) {
    int res = 0;

    for(int i = 0; i < MAX_CONNECTIONS; i++) {
        if(conn == conns[i]) {
            res = i;
	    break;
	}
    }

    return res;
}

void recycle_key(int key) {
    conns[key] = NULL;
    push(stack, key);
}
/*********************************************/

static int service_uuid;
static int characteristic_uuid;
static scan_cb scancb;
static struct bt_uuid_16 uuid = BT_UUID_INIT_16(0);
static struct bt_gatt_discover_params* discover_parameters;
static struct bt_gatt_subscribe_params* subscribe_parameters;

static u8_t characteristic_found(struct bt_conn* conn,
		                 const struct bt_gatt_attr* attr,
				 struct bt_gatt_discover_params* params) { 
    if(!attr) {
        printk("Discovery reached attr == NULL\n");
	return BT_GATT_ITER_STOP;
    }

    printk("[ATTRIBUTE] handle %u\n", attr->handle);

    if(!bt_uuid_cmp(params->uuid, BT_UUID_DECLARE_16(service_uuid))) {
	struct bt_gatt_service_val* serv = attr->user_data;
        memcpy(&uuid, BT_UUID_DECLARE_16(characteristic_uuid), sizeof(uuid));
	discover_parameters->uuid = &uuid.uuid;
	discover_parameters->start_handle = attr->handle + 1;
	discover_parameters->end_handle = serv->end_handle;
	discover_parameters->type = BT_GATT_DISCOVER_CHARACTERISTIC;

	bt_gatt_discover(conn, discover_parameters);
    } else if(!bt_uuid_cmp(params->uuid, BT_UUID_DECLARE_16(characteristic_uuid))) {
        memcpy(&uuid, BT_UUID_GATT_CCC, sizeof(uuid));
	discover_parameters->uuid = &uuid.uuid;
	discover_parameters->start_handle = attr->handle + 2;
	discover_parameters->type = BT_GATT_DISCOVER_DESCRIPTOR;
	subscribe_parameters->value_handle = bt_gatt_attr_value_handle(attr);

	bt_gatt_discover(conn, discover_parameters);
    } else {
	subscribe_parameters->value = BT_GATT_CCC_NOTIFY;
	subscribe_parameters->ccc_handle = attr->handle;

        printk("Discovery complete\n");
	struct value* val = k_malloc(sizeof(struct value));

	val->service_uuid          = service_uuid;
	val->characteristic_uuid   = characteristic_uuid;
	val->characteristic_handle = subscribe_parameters->value_handle;
	val->discover_params       = (void*)discover_parameters;
	val->subscribe_params      = (void*)subscribe_parameters;
	if(scancb) {
            scancb(val);
	}

	return BT_GATT_ITER_STOP;
    }
    return BT_GATT_ITER_STOP;
}

void scan_for_characteristic(struct conn* conn, int service_in_hex, int characteristic_in_hex, scan_cb cb) {
    service_uuid = service_in_hex;
    characteristic_uuid = characteristic_in_hex;
    scancb = cb;

    struct bt_gatt_discover_params* params = k_malloc(sizeof(struct bt_gatt_discover_params));
    memcpy(&uuid, BT_UUID_DECLARE_16(service_uuid), sizeof(uuid));
    params->uuid = &uuid.uuid;
    params->func = characteristic_found;
    params->start_handle = 0x0001;
    params->end_handle = 0xffff;
    params->type = BT_GATT_DISCOVER_PRIMARY;
    discover_parameters = params;

    struct bt_gatt_subscribe_params* subscribe_params = k_malloc(sizeof(struct bt_gatt_subscribe_params));
    subscribe_parameters = subscribe_params;

    bt_gatt_discover(get_conn(conn->key), params);
}

/*********************************************/
/********** Subscription management **********/
struct callback {
    subscribed_cb* cb;
    struct bt_conn* conn;
    struct value* value;
};

struct node {
    void* data;
    struct node* next;
};

struct node* callbacks;

subscribed_cb* find_callback(struct bt_conn* conn, struct bt_gatt_subscribe_params* params) {
    subscribed_cb* res = NULL;

    struct node* cbs = callbacks;
    while(cbs) {
        struct callback* cb = cbs->data;
        if(conn == cb->conn && params->value_handle == cb->value->characteristic_handle) {
            res = cb->cb;
	    break;
	}
        cbs = cbs->next;
    }
    return res;
}

void delete_callback(struct conn* connection, struct value* val) {
    struct bt_conn* conn = get_conn(connection->key);

    struct node* cbs = callbacks;
    if(cbs) {
        struct callback* cb = cbs->data;
	if(cb->conn == conn && cb->value->characteristic_handle == val->characteristic_handle) {
            struct node* n = cbs;
	    cbs = cbs->next;
	    k_free(cb);
	    k_free(n);
	    callbacks = cbs;
	}
    } else if(!cbs) {
        // No callback registered, nothing to unsubscribe
	return;
    } else {
        while(cbs) {
	    struct node* next_n = cbs->next;
            struct callback* next_cb = next_n->data;
            if(next_n && next_cb->conn == conn && next_cb->value->characteristic_handle == val->characteristic_handle) {
                cbs->next = cbs->next->next;
		k_free(next_cb);
		k_free(next_n);
		return;
	    }
	    cbs = next_n;
        }
    }
}

void insert_callback(struct callback* cb) {
    struct node* new = (struct node*) k_malloc(sizeof(struct node));
    new->data = cb;

    struct node* cbs = callbacks;
    if(!cbs) {
        cbs = new;
    } else {
        while(cbs->next) {
            cbs = cbs->next;
	}
	cbs->next = new;
    }
    callbacks = cbs;
}

static u8_t global_callback(struct bt_conn* conn, struct bt_gatt_subscribe_params* params, const void* data, u16_t length) {
    subscribed_cb* cb = find_callback(conn, params);
    if(cb && data) {
        (*cb)(data, length);
    } else {
        printk("An error ocurred - received notification without a registered callback function or data is NULL\n");
    }
    return BT_GATT_ITER_CONTINUE;
}

int subscribe_characteristic(struct conn* connection, struct value* val, subscribed_cb cb) {
    struct bt_conn* conn = get_conn(connection->key);

    if(conn) {
        struct callback* callback = (struct callback*) k_malloc(sizeof(struct callback));
        callback->cb = cb;
        callback->conn = conn;
        callback->value = val;

	struct bt_gatt_subscribe_params* params = val->subscribe_params;
	params->notify = global_callback;

	int err = bt_gatt_subscribe(conn, params);
	if(err && err != -EALREADY) {
            printk("Subscribe failed\n");
	    k_free(callback);
	    return 1;
	} else {
            printk("Subscribe succeeded\n");
	    insert_callback(callback);
	    return 0;
	}
    } else {
        printk("The connection does not exist\n");
	return 1;
    }
}

int unsubscribe_characteristic(struct conn* connection, struct value* val) {
    struct bt_conn* conn = get_conn(connection->key);
    struct bt_gatt_subscribe_params* params = val->subscribe_params;

    delete_callback(connection, val);

    int err = bt_gatt_unsubscribe(conn, params);

    if(err) {
        printk("Ubsubscribe failed\n");
        return 1;
    } 
    return 0;
}

/*********************************************/

static int target;
static int target_key;
struct bt_uuid* t;

static bool eir_found(struct bt_data *data, void *user_data)
{
	bt_addr_le_t *addr = user_data;
	int i;

	printk("[AD]: %u data_len %u\n", data->type, data->data_len);

	switch (data->type) {
	case BT_DATA_UUID16_SOME:
	case BT_DATA_UUID16_ALL:
		if (data->data_len % sizeof(u16_t) != 0U) {
			printk("AD malformed\n");
			return true;
		}

		for (i = 0; i < data->data_len; i += sizeof(u16_t)) {
			struct bt_le_conn_param *param;
			struct bt_uuid *uuid;
			u16_t u16;
			int err;

			memcpy(&u16, &data->data[i], sizeof(u16));
			uuid = BT_UUID_DECLARE_16(sys_le16_to_cpu(u16));
			if (bt_uuid_cmp(uuid, BT_UUID_DECLARE_16(target))) {
			//if (bt_uuid_cmp(uuid, t)) {
				continue;
			}

			err = bt_le_scan_stop();
			if (err) {
				printk("Stop LE scan failed (err %d)\n", err);
				continue;
			}

			param = BT_LE_CONN_PARAM_DEFAULT;
			err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
						param, &(conns[target_key]));
			if (err) {
				printk("Create conn failed (err %d)\n", err);
			}

			return false;
		}
	}

	return true;
}

static void device_found(const bt_addr_le_t *addr, s8_t rssi, u8_t type,
		struct net_buf_simple *ad) {
	char dev[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, dev, sizeof(dev));
	printk("[DEVICE]: %s, AD evt type %u, AD data len %u, RSSI %i\n",
	       dev, type, ad->len, rssi);

	/* We're only interested in connectable events */
	if (type == BT_GAP_ADV_TYPE_ADV_IND ||
	    type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
		bt_data_parse(ad, eir_found, (void *)addr);
	}
}

void try_connect(int uuid_in_hex) {
    int slot = get_slot();
    if(slot != -1) {
        int err; 
        target = uuid_in_hex;
        target_key = slot;

        struct bt_le_scan_param scan_param = {
	    .type       = BT_LE_SCAN_TYPE_ACTIVE,
	    .options    = BT_LE_SCAN_OPT_NONE,
	    .interval   = BT_GAP_SCAN_FAST_INTERVAL,
	    .window     = BT_GAP_SCAN_FAST_WINDOW,
        };

        err = bt_le_scan_start(&scan_param, device_found);
        if(err) {
            printk("Scanning failed to start (err %d)\n", err);
    	    target = -1;
    	    return;
        }
        printk("Scanning successfully started\n");
    } else {
        printk("Maximum number of concurrent connections reached: %d\n",
			MAX_CONNECTIONS);
    }
}

/* Connection callback */
conn_cb connect;
void register_connected_callback(conn_cb cb) {connect = cb;};
void unregister_connected_callback() { connect = NULL; };

static void connected(struct bt_conn *conn, u8_t conn_err) {
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	int key = get_key(conn);
	struct conn* connection = (struct conn*)k_malloc(sizeof(struct conn));
	connection->key = key;
	apiconns[key] = connection;
	if (conn_err) {
		printk("Failed to connect to %s (%u)\n", addr, conn_err);

		bt_conn_unref(conn);
		recycle_key(key);
		return;
	}

	printk("Connected: %s\n", addr);

	/* If a conn_cb is registered, apply it */
	if(connect) {
            connect(connection);
	}

	target = -1;
	target_key = -1;
	t = NULL;
}

conn_cb disconnect;
void register_disconnected_callback(conn_cb cb) {disconnect = cb;};
void unregister_disconnected_callback() { disconnect = NULL; };

static void disconnected(struct bt_conn *conn, u8_t reason) {
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

	bt_conn_unref(conn);
	if(disconnect) {
            int key = get_key(conn);
	    struct conn* apiconn = apiconns[key];
	    disconnect(apiconn);
	    recycle_key(key);
	    k_free(apiconn);
	    apiconns[key] = NULL;
	}

}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

void start_bt(void)
{
	int err;

	stack = newStack(MAX_CONNECTIONS);
        for(int i = 0; i < MAX_CONNECTIONS; i++) {
            push(stack, i);
	}

	t = (struct bt_uuid*) k_malloc(sizeof(struct bt_uuid));

	err = bt_enable(NULL);

	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	bt_conn_cb_register(&conn_callbacks);
}
