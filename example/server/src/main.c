/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/printk.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#define BT_UUID_TEMP                 BT_UUID_DECLARE_16(0xffcc)
#define BT_UUID_TEMP_SERVICE         BT_UUID_DECLARE_16(0xffaa)
#define BT_UUID_TEMP_CHARACTERISTIC  BT_UUID_DECLARE_16(0xffbb)

/*********************************/
struct payload {
  u8_t temperature;
  u8_t octavius;
};

static struct payload data = {.temperature = 30, .octavius = 1};
static ssize_t read_data(struct bt_conn* conn, const struct bt_gatt_attr *attr, void *buf, u16_t len, u16_t offset) {
	struct payload outp = data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &outp,
				 sizeof(outp));
}

static void tempoct_ccc_cfg_changed(const struct bt_gatt_attr *attr,
				       u16_t value)
{
	ARG_UNUSED(attr);

	bool notif_enabled = (value == BT_GATT_CCC_NOTIFY);

	printk("BAS Notifications %s\n", notif_enabled ? "enabled" : "disabled");
}

BT_GATT_SERVICE_DEFINE(bas,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_TEMP_SERVICE),
	BT_GATT_CHARACTERISTIC(BT_UUID_TEMP_CHARACTERISTIC,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, // it can be read from and subscribed to
			       BT_GATT_PERM_READ, read_data, NULL,
			       &data),
	BT_GATT_CCC(tempoct_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

struct payload bt_gatt_get_temp_oct_data(void)
{
	return data;
}
int bt_gatt_set_temp_oct_data(struct payload input)
{
	data = input;

	int rc = bt_gatt_notify(NULL, &bas.attrs[1], &input, sizeof(input));

	return rc == -ENOTCONN ? 0 : rc;
}

/***************************************************************************************/
struct bt_conn *default_conn;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xcc, 0xff, 0xaa, 0xff, 0x0a, 0x18),
};

static void connected(struct bt_conn *conn, u8_t err)
{
	if (err) {
		printk("Connection failed (err 0x%02x)\n", err);
	} else {
		default_conn = bt_conn_ref(conn);
		printk("Connected\n");
	}
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	printk("Disconnected (reason 0x%02x)\n", reason);

	if (default_conn) {
		bt_conn_unref(default_conn);
		default_conn = NULL;
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_ready(void)
{
	int err;

	printk("Bluetooth initialized\n");

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.cancel = auth_cancel,
};

static void bas_notify(void)
{
	struct payload current = bt_gatt_get_temp_oct_data();
	current.temperature--;

	if (!current.temperature) {
		current.temperature = 100U;
	}

	bt_gatt_set_temp_oct_data(current);
}

void main(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	bt_ready();

	bt_conn_cb_register(&conn_callbacks);
	bt_conn_auth_cb_register(&auth_cb_display);

	/* Implement notification. At the moment there is no suitable way
	 * of starting delayed work so we do it here
	 */
	while (1) {
		k_sleep(K_SECONDS(1));

		/* Battery level simulation */
		bas_notify();
	}
}
