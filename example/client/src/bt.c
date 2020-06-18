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

#define MAX_CONNECTIONS 5

#define BT_UUID_DEVICE                             BT_UUID_DECLARE_16(0xffcc)

#define BT_UUID_TEMPERATURE_SENSOR_SERVICE         BT_UUID_DECLARE_16(0xff11)
#define BT_UUID_TEMPERATURE_SENSOR_CHARACTERISTIC  BT_UUID_DECLARE_16(0xff12)

#define BT_UUID_OCTAVIUS_SERVICE                   BT_UUID_DECLARE_16(0xff21)
#define BT_UUID_OCTAVIUS_CHARACTERISTIC            BT_UUID_DECLARE_16(0xff22)

static struct bt_conn *default_conn;

/*********** Connection management ***********/
// TODO Can not remove connections from here now. Add that later.

struct bt_conn* conns[MAX_CONNECTIONS];
int next_free = 0;

/* Returns -1 if it was not possible to add the connection.
 * This can only happen if the max number of connections have been reached.
 *
 * Otherwise, return x >= 0 */
int set_conn(struct bt_conn* conn) {
    if(next_free < MAX_CONNECTIONS) {
        conns[next_free] = conn;
	return next_free++;
    } else {
        return -1;
    }
}

int get_slot() {
    if(next_free < MAX_CONNECTIONS) {
       return next_free++;
    }
    return -1;
}

/* Get the connection object associated with the key. */
struct bt_conn* get_conn(int key) {
    struct bt_conn* res;
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
				continue;
			}
			printk("Val from desired device:    %d\n", (int)BT_UUID_16(t)->val);
			printk("The uuid is: %d\n", target);

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
//	k_tid_t id = k_current_get();
//	printk("%d\n", id);

	/* We're only interested in connectable events */
	if (type == BT_GAP_ADV_TYPE_ADV_IND ||
	    type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
		bt_data_parse(ad, eir_found, (void *)addr);
	}
}

void try_connect(int uuid_in_hex) {
    int err;
    target = uuid_in_hex;
    target_key = get_slot();
    t = BT_UUID_DECLARE_16(uuid_in_hex);
    printk("Just before scan starts t has uuid: %d\n", (int)BT_UUID_16(t)->val);

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
}

/* Connection callback */
conn_cb connect;
void register_connected_callback(conn_cb cb) {connect = cb;};
void unregister_connected_callback() { connect = NULL; };

static void connected(struct bt_conn *conn, u8_t conn_err) {
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		printk("Failed to connect to %s (%u)\n", addr, conn_err);

		bt_conn_unref(default_conn);
		default_conn = NULL;

		return;
	}

	printk("Connected: %s\n", addr);

	/* If a conn_cb is registered, apply it */
	if(connect) {
            connect(target_key);
	    target_key = -1;
	    target = -1;
	}
}

conn_cb disconnect;
void register_disconnected_callback(conn_cb cb) {disconnect = cb;};
void unregister_disconnected_callback() { disconnect = NULL; };

static void disconnected(struct bt_conn *conn, u8_t reason) {
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

	if (default_conn != conn) {
		return;
	}

	bt_conn_unref(default_conn);
	default_conn = NULL;

	if(disconnect) {
            int key = get_key(conn);
	    disconnect(key);
	}

}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

void start_bt(void)
{
	int err;
	err = bt_enable(NULL);

	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	bt_conn_cb_register(&conn_callbacks);
	struct stack* stack;
}
