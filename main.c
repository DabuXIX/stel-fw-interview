/*
* Enhanced the BLE central application to:
* 1. Filter connecting devices by name in the device_found function using the bt_data_parse API.
*    - Specifically filters for devices named "DXC" by checking the advertisement data type BT_DATA_NAME_COMPLETE.
* 2. Support multiple device connections.
*    - Manages up to six simultaneous connections by using an array of connection objects. 
*	 - The MAX_CONNECTIONS variable can be changed to suit the desired amount of supported devices.
*    - Iterates through the connections array to handle connection and disconnection events.
*/
/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/sys/byteorder.h>

static void start_scan(void);

// Defines the maximum number of simultaneous connections
#define MAX_CONNECTIONS 6

// Array to store multiple connection objects, initialized to NULL
static struct bt_conn *conns[MAX_CONNECTIONS] = {0};

// Helper function to check if the device advertisement data contains the desired name
static bool device_name_found(struct bt_data *data, void *user_data)
{
    const char *name = user_data;

    if (data->type == BT_DATA_NAME_COMPLETE) {
        if (strncmp(name, (char *)data->data, data->data_len) == 0) {
            return false; 
        }
    }

    return true; 
}

// Modified device_found function to support multiple connections and filter by device name
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    int err;
    const char *desired_name = "DXC";

    /* We're only interested in connectable events */
    if (type != BT_GAP_ADV_TYPE_ADV_IND && type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        return;
    }

    /* Filter by device name */
    if (bt_data_parse(ad, device_name_found, (void *)desired_name)) {
        // Device with the desired name not found
        return;
    }

    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    printk("Device found: %s (RSSI %d)\n", addr_str, rssi);

    /* Connect only to devices in close proximity */
    if (rssi < -50) {
        return;
    }

    if (bt_le_scan_stop()) {
        return;
    }

    // Iterate through the connections array to find an available slot
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!conns[i]) {
            err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &conns[i]);
            if (err) {
                printk("Create conn to %s failed (%d)\n", addr_str, err);
                start_scan();
            }
            return;
        }
    }

    printk("No available connection slots\n");
    start_scan();
}

// There are no changes to this function required
static void start_scan(void)
{
	int err;

	/* This demo doesn't require active scan */
	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return;
	}

	printk("Scanning successfully started\n");
}

// The premise of this change is to iterate through the conns array to manage multiple connections
static void connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err) {
        printk("Failed to connect to %s (err %u %s)\n", addr, err, bt_hci_err_to_str(err));
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            if (conns[i] == conn) {
                bt_conn_unref(conns[i]);
                conns[i] = NULL;
                break;
            }
        }
        start_scan();
        return;
    }

    printk("Connected: %s\n", addr);
}

// Similar to the above function where we iterate through the conns array to manage multiple conenctions
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printk("Disconnected: %s, reason 0x%02x %s\n", addr, reason, bt_hci_err_to_str(reason));

    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (conns[i] == conn) {
            bt_conn_unref(conns[i]);
            conns[i] = NULL;
            break;
        }
    }

    start_scan();
}

int main(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");

	start_scan();
	return 0;
}
