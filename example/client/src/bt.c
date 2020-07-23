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

// concurrent connections
#define MAX_CONNECTIONS 5

struct node {
    void* data;
    struct node* next;
};

/*********** Connection management ***********/
/* Ideally when we establish a connection we'd just allocate the resources
 * required, when we need them, and put them away in a list or something.
 * However, Zephyr requires that the connection object struct bt_conn be
 * allocated at compile time. This is the reason for keeping an array of
 * conveniently sized elements around, and a stack (pile) of 'free' array
 * indexes, indicating that the connection objects associated with those indexes
 * are free to use, meaning they are not bound to a remote device.
 *
 * When you want to establish a connection:
 *   * Get a free slot
 *   * Get the connection object in that slot
 *   * Use it as you see fit
 *
 * When a connection is broken:
 *   * Get the key associated with the connection object (the disconnection
 *   callback will have been given a connection object).
 *   * Recycle the connection object by invoking 'recycle_key', which will
 *   assign NULL to the now invalid connection object and put the index back
 *   in the stack of free indexes.
 *
 *   NOTE: There are two types; struct bt_conn and struct conn. bt_conn is
 *   a Zephyr type while struct conn is a type exposed through our new API.
 *   It does not reveal any zephyr implementation details.
 */

struct stack* stack;
struct bt_conn* conns[MAX_CONNECTIONS];
struct conn* apiconns[MAX_CONNECTIONS];

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
/*
 * When you scan for a characteristic the intention is that you get a value back
 * which can be used to initiate communication and/or subscribe events.
 * While the search is taking place, a struct target object is created to
 * keep track of what we are looking for. As we find more and more information
 * about the remote device we update the discovery & subscription parameters.
 *
 * To scan for more than 1 characteristic simultaneously we keep a list of
 * targets. The discovery callback knows which discover parameters were used
 * to scan, which we can use to uniquely identify the right target in the list.
 *
 * When we found every piece of information required we deallocate the
 * struct target, but place the subscribe parameters and some other information
 * in a struct value* object that is passed back through to the caller via the
 * scan callback, found in the target struct.
 *
 * Scanning for a characteristic only probes the remote device. It does not
 * send or read the characteristic in question. These things can be done through
 * the API by using the struct value object.
 */

struct target {
    int service_uuid;
    int characteristic_uuid;
    scan_cb scancb;
    struct bt_uuid_16 uuid;
    struct bt_gatt_discover_params* discover_parameters;
    struct bt_gatt_subscribe_params* subscribe_parameters;
};

int eqParams(struct target* t, struct bt_gatt_discover_params* params) {
    return t->discover_parameters == params;
}

K_MUTEX_DEFINE(targets_lock);
struct node* targets;

void insert_into_targets(struct target* t) {
    k_mutex_lock(&targets_lock, K_FOREVER);
    struct node* new = k_malloc(sizeof(struct target));
    new->data = t;

    struct node* list = targets;
    
    if(!list) {
        targets = new;
    } else {
        while(list->next) {
            list = list->next;
        }
	list->next = new;
    }
    k_mutex_unlock(&targets_lock);
}

void remove_from_targets(struct bt_gatt_discover_params* params) {
    k_mutex_lock(&targets_lock, K_FOREVER);
    struct node* list = targets;

    if(targets && eqParams(targets->data, params)) {
        struct node* old = targets;
        struct target* t = targets->data;

	targets = targets->next;
	k_free(old);
	k_free(t);
    } else if(targets) {
        while(list->next) {
            if(eqParams(list->next->data, params)) {
                struct node* old = list->next;
		struct target* t = list->next->data;

		list->next = list->next->next;
		k_free(old);
		k_free(t);
		break;
	    }
	}
	list = list->next;
    }
    k_mutex_unlock(&targets_lock);
}

struct target* find_target(struct bt_gatt_discover_params* params) {
    k_mutex_lock(&targets_lock, K_FOREVER);
    struct target* res = NULL;

    struct node* tgs = targets;
    while(tgs) {
        struct target* t = tgs->data;
        if(t->discover_parameters == params) {
            res = t;
	    break;
	}
	tgs = tgs->next;
    }
    k_mutex_unlock(&targets_lock);
    return res;
}

static u8_t characteristic_found(struct bt_conn* conn,
		                 const struct bt_gatt_attr* attr,
				 struct bt_gatt_discover_params* params) { 
    if(!attr) {
        printk("Discovery reached attr == NULL\n");
	return BT_GATT_ITER_STOP;
    }

    printk("[ATTRIBUTE] handle %u\n", attr->handle);

    struct target* target = find_target(params);

    if(!bt_uuid_cmp(params->uuid, BT_UUID_DECLARE_16(target->service_uuid))) {
	struct bt_gatt_service_val* serv = attr->user_data;
        memcpy(&target->uuid, BT_UUID_DECLARE_16(target->characteristic_uuid), sizeof(target->uuid));
	target->discover_parameters->uuid = &target->uuid.uuid;
	target->discover_parameters->start_handle = attr->handle + 1;
	target->discover_parameters->end_handle = serv->end_handle;
	target->discover_parameters->type = BT_GATT_DISCOVER_CHARACTERISTIC;

	bt_gatt_discover(conn, target->discover_parameters);
    } else if(!bt_uuid_cmp(params->uuid, BT_UUID_DECLARE_16(target->characteristic_uuid))) {
        memcpy(&target->uuid, BT_UUID_GATT_CCC, sizeof(target->uuid));
	target->discover_parameters->uuid = &target->uuid.uuid;
	target->discover_parameters->start_handle = attr->handle + 2;
	target->discover_parameters->type = BT_GATT_DISCOVER_DESCRIPTOR;
	target->subscribe_parameters->value_handle = bt_gatt_attr_value_handle(attr);

	bt_gatt_discover(conn, target->discover_parameters);
    } else {
	target->subscribe_parameters->value = BT_GATT_CCC_NOTIFY;
	target->subscribe_parameters->ccc_handle = attr->handle;

        printk("Discovery complete\n");
	struct value* val = k_malloc(sizeof(struct value));

	val->service_uuid          = target->service_uuid;
	val->characteristic_uuid   = target->characteristic_uuid;
	val->characteristic_handle = target->subscribe_parameters->value_handle;
	val->conn                  = conn;
	val->subscribe_params      = (void*)target->subscribe_parameters;
	if(target->scancb) {
            (target->scancb)(val);
	}
	remove_from_targets(params);

	return BT_GATT_ITER_STOP;
    }
    return BT_GATT_ITER_STOP;
}

void scan_for_characteristic(struct conn* conn, int service_in_hex, int characteristic_in_hex, scan_cb cb) {
    struct target* t = k_malloc(sizeof(struct target));
    t->service_uuid = service_in_hex;
    t->characteristic_uuid = characteristic_in_hex;
    t->scancb = cb;

    t->uuid.uuid.type = 0;
    t->uuid.val = 0;
	
    struct bt_gatt_discover_params* params = k_malloc(sizeof(struct bt_gatt_discover_params));
    memcpy(&(t->uuid), BT_UUID_DECLARE_16(t->service_uuid), sizeof(t->uuid));
    params->uuid = &(t->uuid.uuid);
    params->func = characteristic_found;
    params->start_handle = 0x0001;
    params->end_handle = 0xffff;
    params->type = BT_GATT_DISCOVER_PRIMARY;
    t->discover_parameters = params;

    struct bt_gatt_subscribe_params* subscribe_params = k_malloc(sizeof(struct bt_gatt_subscribe_params));
    t->subscribe_parameters = subscribe_params;

    insert_into_targets(t);
    bt_gatt_discover(get_conn(conn->key), t->discover_parameters);
}    
/*********************************************/
/********** Subscription management **********/
/*
 * Currently there is a global callback function that is registered as the
 * callback for every characteristic you subscribe to. When this callback is
 * invoked the callback will use the connection & subscription parameters to
 * uniquely identify and fetch the application callback code to run.
 *
 * TODO The _single_ callback registered to a characteristic should probably
 * be a list of such callback functions instead. We should not be limited in
 * the number of callbacks we can register.
 */

struct callback {
    subscribed_cb* cb;
    struct bt_conn* conn;
    struct value* value;
};

K_MUTEX_DEFINE(callbacks_lock);
struct node* callbacks;

subscribed_cb* find_callback(struct bt_conn* conn, struct bt_gatt_subscribe_params* params) {
    k_mutex_lock(&callbacks_lock, K_FOREVER);
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
    k_mutex_unlock(&callbacks_lock);
    return res;
}

void delete_callback(struct value* val) {
    k_mutex_lock(&callbacks_lock, K_FOREVER);
    struct bt_conn* conn = val->conn;

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
    } else if(cbs) {
        while(cbs) {
	    struct node* next_n = cbs->next;
            struct callback* next_cb = next_n->data;
            if(next_n && next_cb->conn == conn && next_cb->value->characteristic_handle == val->characteristic_handle) {
                cbs->next = cbs->next->next;
		k_free(next_cb);
		k_free(next_n);
                k_mutex_unlock(&callbacks_lock);
		return;
	    }
	    cbs = next_n;
        }
    }
    k_mutex_unlock(&callbacks_lock);
}

void insert_callback(struct callback* cb) {
    k_mutex_lock(&callbacks_lock, K_FOREVER);
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
    k_mutex_unlock(&callbacks_lock);
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

int subscribe_characteristic(struct value* val, subscribed_cb cb) {
    struct bt_conn* conn = val->conn;

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

int unsubscribe_characteristic(struct value* val) {
    struct bt_conn* conn = val->conn;
    struct bt_gatt_subscribe_params* params = val->subscribe_params;

    delete_callback(val);

    int err = bt_gatt_unsubscribe(conn, params);

    if(err) {
        printk("Ubsubscribe failed\n");
        return 1;
    } 
    return 0;
}

/*********************************************/
// TODO When I have another board so that I can test it, I'd like to rewrite this
/* using the same trick as with characteristic scanning, keeping current scans
 * in a list, thus enabling the user to issue connection events to more than one
 * device at a time.
 */

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
