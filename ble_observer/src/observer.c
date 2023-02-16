/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

/* Set DEBUG_ENABLE to see all debug messages*/
#define DEBUG_ENABLE	0

#define NAME_LEN 30

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
}

#if defined(CONFIG_BT_EXT_ADV)
static bool data_cb(struct bt_data *data, void *user_data)
{
	char *name = user_data;
	uint8_t len;

	switch (data->type) {
	case BT_DATA_NAME_SHORTENED:
	case BT_DATA_NAME_COMPLETE:
		len = MIN(data->data_len, NAME_LEN - 1);
		(void)memcpy(name, data->data, len);
		name[len] = '\0';
		if(DEBUG_ENABLE) printk("BLE Dev Name: %s\n", name);
		return false;
	case BT_DATA_MANUFACTURER_DATA:
		len = MIN(data->data_len, NAME_LEN - 1);
		(void)memcpy(name, data->data, len);
		name[len] = '\0';
		if(DEBUG_ENABLE) {
			printk("Manufacturer Specific Data: ");
			for (int i = 0; i < len; i++)
			{
				printk(" %.2x ", name[i]);
			}
			printk("\n");
		}
	case BT_DATA_URI:
		len = MIN(data->data_len, NAME_LEN - 1);
		(void)memcpy(name, data->data, len);
		name[len] = '\0';
		if(DEBUG_ENABLE) printk("BLE URI: %s\n", name);
		return false;
	default:
		return true;
	}
}

static const char *phy2str(uint8_t phy)
{
	switch (phy) {
	case BT_GAP_LE_PHY_NONE: return "No packets";
	case BT_GAP_LE_PHY_1M: return "LE 1M";
	case BT_GAP_LE_PHY_2M: return "LE 2M";
	case BT_GAP_LE_PHY_CODED: return "LE Coded";
	default: return "Unknown";
	}
}

static void scan_recv(const struct bt_le_scan_recv_info *info,
		      struct net_buf_simple *buf)
{
	char le_addr[BT_ADDR_LE_STR_LEN];
	char name[NAME_LEN];
	uint8_t data_status;
	uint16_t data_len;
	
	// Unique device identifier
	char * sportident_dev_id0 = "SI Beacon";
	char * sportident_dev_id1 = "SI";

	(void)memset(name, 0, sizeof(name));

	data_len = buf->len;
	char scan_data[100];
	(void)memset(scan_data, 0, sizeof(scan_data));
	memcpy(scan_data, buf->data, data_len);
	
	bt_data_parse(buf, data_cb, name);

	data_status = BT_HCI_LE_ADV_EVT_TYPE_DATA_STATUS(info->adv_props);

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));

	if (info->adv_type == 2)
	{
		// Print raw data
		if(DEBUG_ENABLE) {
			printk("Scan Data: ");
			for (int i = 0; i < data_len; i++)
			{
				printk(" %.2x ", scan_data[i]);
			}
			printk("\r\n");
			printk("[TYPE 2 DEVICE]: %s, AD evt type %u, Tx Pwr: %i, RSSI %i "
					"Data status: %u, AD data len: %u Name: %s "
					"C:%u S:%u D:%u SR:%u E:%u Pri PHY: %s, Sec PHY: %s, "
					"Interval: 0x%04x (%u ms), SID: %u\r\n",
					le_addr, info->adv_type, info->tx_power, info->rssi,
					data_status, data_len, name,
					(info->adv_props & BT_GAP_ADV_PROP_CONNECTABLE) != 0,
					(info->adv_props & BT_GAP_ADV_PROP_SCANNABLE) != 0,
					(info->adv_props & BT_GAP_ADV_PROP_DIRECTED) != 0,
					(info->adv_props & BT_GAP_ADV_PROP_SCAN_RESPONSE) != 0,
					(info->adv_props & BT_GAP_ADV_PROP_EXT_ADV) != 0,
					phy2str(info->primary_phy), phy2str(info->secondary_phy),
					info->interval, info->interval * 5 / 4, info->sid);
		}
		if ((scan_data[0] == 0x02 && scan_data[1] == 0x01 && scan_data[2] == 0x04) || 
				(strstr(name, sportident_dev_id0) != NULL || strstr(name, sportident_dev_id1) != NULL))
		{
			if(DEBUG_ENABLE) {
				printk("SPORTident Device Found!!!\n");
				printk("Scanned Data: ");
				for (int i = 0; i < data_len; i++)
				{
					printk(" %.2x ", scan_data[i]);
				}
				printk("\n");
			}

			if (scan_data[6] == 0xFF) {
				/* Parse Manufacturer specific data */
				char siac_data[] = {
					scan_data[7], scan_data[8], scan_data[9], scan_data[10], 
					scan_data[11], scan_data[12], scan_data[13]
					};
				if(DEBUG_ENABLE) {
					printk("SIAC Data: ");
					for (int i = 0; i < 7; i++)
					{
						printk(" %.2x ", siac_data[i]);
					}
					printk("\n");
				}
				spi_write_test_msg(siac_data);
			}

			if(DEBUG_ENABLE) printk("[SI DEVICE]: %s, AD evt type %u, Tx Pwr: %i, RSSI %i "
				"Data status: %u, AD data len: %u Name: %s "
				"C:%u S:%u D:%u SR:%u E:%u Pri PHY: %s, Sec PHY: %s, "
				"Interval: 0x%04x (%u ms), SID: %u\r\n",
				le_addr, info->adv_type, info->tx_power, info->rssi,
				data_status, data_len, name,
				(info->adv_props & BT_GAP_ADV_PROP_CONNECTABLE) != 0,
				(info->adv_props & BT_GAP_ADV_PROP_SCANNABLE) != 0,
				(info->adv_props & BT_GAP_ADV_PROP_DIRECTED) != 0,
				(info->adv_props & BT_GAP_ADV_PROP_SCAN_RESPONSE) != 0,
				(info->adv_props & BT_GAP_ADV_PROP_EXT_ADV) != 0,
				phy2str(info->primary_phy), phy2str(info->secondary_phy),
				info->interval, info->interval * 5 / 4, info->sid);
		}
	}
}

static struct bt_le_scan_cb scan_callbacks = {
	.recv = scan_recv,
};
#endif /* CONFIG_BT_EXT_ADV */

int observer_start(void)
{
	struct bt_le_scan_param scan_param = {
		.type       = BT_LE_SCAN_TYPE_PASSIVE,
		.options    = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
		.interval   = BT_GAP_SCAN_FAST_INTERVAL,
		.window     = BT_GAP_SCAN_FAST_WINDOW,
	};
	int err;

#if defined(CONFIG_BT_EXT_ADV)
	bt_le_scan_cb_register(&scan_callbacks);
	if(DEBUG_ENABLE) printk("Registered scan callbacks\n");
#endif /* CONFIG_BT_EXT_ADV */

	err = bt_le_scan_start(&scan_param, device_found);
	if (err) {
		if(DEBUG_ENABLE) printk("Start scanning failed (err %d)\n", err);
		return err;
	}
	if(DEBUG_ENABLE) printk("Started scanning...\n");

	return 0;
}
