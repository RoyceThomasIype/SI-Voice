/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>

#include <zephyr/settings/settings.h>

#include <dk_buttons_and_leds.h>

#include <drivers/gpio.h>

#define NON_CONNECTABLE_ADV_IDX 0
#define CONNECTABLE_ADV_IDX     1

#define RUN_STATUS_LED          DK_LED1
#define CON_STATUS_LED          DK_LED2
#define RUN_LED_BLINK_INTERVAL  1000

#define NON_CONNECTABLE_DEVICE_NAME "SI Beacon"

#define BLE_ADV_TIMEOUT		(50)	//  N * 10ms for advertiser timeout
#define BLE_ADV_EVENTS		(5)

#define BUTTON0_NODE	DT_NODELABEL(button0)
#define BUTTON1_NODE	DT_NODELABEL(button1)
#define BUTTON2_NODE	DT_NODELABEL(button2)
#define BUTTON3_NODE	DT_NODELABEL(button3)

#define APP_IDLE		0
#define APP_BLE_ADV		1

static const struct gpio_dt_spec button0_spec = GPIO_DT_SPEC_GET(BUTTON0_NODE, gpios);
static const struct gpio_dt_spec button1_spec = GPIO_DT_SPEC_GET(BUTTON1_NODE, gpios);
static const struct gpio_dt_spec button2_spec = GPIO_DT_SPEC_GET(BUTTON2_NODE, gpios);
static const struct gpio_dt_spec button3_spec = GPIO_DT_SPEC_GET(BUTTON3_NODE, gpios);

static struct gpio_callback button0_cb;
static struct gpio_callback button1_cb;
static struct gpio_callback button2_cb;
static struct gpio_callback button3_cb;

static void advertising_work_handle(struct k_work *work);

static K_WORK_DEFINE(advertising_work, advertising_work_handle);

static struct bt_le_ext_adv *ext_adv[CONFIG_BT_EXT_ADV_MAX_ADV_SET];
static const struct bt_le_adv_param *non_connectable_adv_param =
	BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_NAME,
			// 0x140, /* 200 ms */
			0xA0,  /* 100 ms */
			// 0x190, /* 250 ms */
			0xB0,  /* 110 ms */
			NULL);

static struct bt_data non_connectable_data[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA, /* Manufacturer specific data */
		      0xFF, 0xFF, /* Manufacturer identifier for SPORTident */
		      0x07, 0x00 /*control no:*/, 0x01 /*hours*/, 0x0C /*minutes*/, 0x00, 0x00, 0x00,  /* Data from Station inc. timestamp */
			  0x00, 0x00, 0x00, 0x01	/* SIAC ID */)
};

static struct bt_data non_connectable_data0[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA, /* Manufacturer specific data */
			0xFF, 0xFF, /* Manufacturer identifier for SPORTident */
			0x07, 0x01, 0x00, 0x0C, 0x00, 0x00, 0x00,  /* Data from Station inc. timestamp */
			0x00, 0x00, 0x00, 0x01	/* SIAC ID */)
	};

static struct bt_data non_connectable_data1[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA, /* Manufacturer specific data */
			0xFF, 0xFF, /* Manufacturer identifier for SPORTident */
			0x07, 0x02, 0x00, 0x20, 0x00, 0x00, 0x00,  /* Data from Station inc. timestamp */
			0x00, 0x00, 0x00, 0x02	/* SIAC ID */)
	};

static struct bt_data non_connectable_data2[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA, /* Manufacturer specific data */
			0xFF, 0xFF, /* Manufacturer identifier for SPORTident */
			0x07, 0x03, 0x01, 0x03, 0x00, 0x00, 0x00,  /* Data from Station inc. timestamp */
			0x00, 0x00, 0x00, 0x03	/* SIAC ID */)
	};

static struct bt_data non_connectable_data3[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA, /* Manufacturer specific data */
			0xFF, 0xFF, /* Manufacturer identifier for SPORTident */
			0x07, 0x04, 0x01, 0x16, 0x00, 0x00, 0x00,  /* Data from Station inc. timestamp */
			0x00, 0x00, 0x00, 0x04	/* SIAC ID */)
	};

static void adv_connected_cb(struct bt_le_ext_adv *adv,
			     struct bt_le_ext_adv_connected_info *info)
{
	printk("Advertiser[%d] %p connected conn %p\n", bt_le_ext_adv_get_index(adv),
		adv, info->conn);
}

static const struct bt_le_ext_adv_cb adv_cb = {
	.connected = adv_connected_cb
};

static void connectable_adv_start(void)
{
	int err;

	err = bt_le_ext_adv_start(ext_adv[CONNECTABLE_ADV_IDX], BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		printk("Failed to start connectable advertising (err %d)\n", err);
	}
}

static void advertising_work_handle(struct k_work *work)
{
	connectable_adv_start();
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (err) {
		printk("Connection failed (err %u)\n", err);
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	dk_set_led_on(CON_STATUS_LED);

	printk("Connected %s\n", addr);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	dk_set_led_off(CON_STATUS_LED);

	printk("Disconnected: %s (reason %u)\n", addr, reason);

	/* Process the disconnect logic in the workqueue so that
	 * the BLE stack is finished with the connection bookkeeping
	 * logic and advertising is possible.
	 */
	k_work_submit(&advertising_work);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static int advertising_set_create(struct bt_le_ext_adv **adv,
				  const struct bt_le_adv_param *param,
				  const struct bt_data *ad, size_t ad_len)
{
	int err;
	struct bt_le_ext_adv *adv_set;

	err = bt_le_ext_adv_create(param, &adv_cb,
				   adv);
	if (err) {
		return err;
	}

	adv_set = *adv;

	printk("Created adv: %p\n", adv_set);

	err = bt_le_ext_adv_set_data(adv_set, ad, ad_len,
				     NULL, 0);
	if (err) {
		printk("Failed to set advertising data (err %d)\n", err);
		return err;
	}

	return bt_le_ext_adv_start(adv_set, BT_LE_EXT_ADV_START_PARAM(BLE_ADV_TIMEOUT, BLE_ADV_EVENTS));
}

static int non_connectable_adv_create(int mockStationNumber)
{
	int err;

	err = bt_set_name(NON_CONNECTABLE_DEVICE_NAME);
	if (err) {
		printk("Failed to set device name (err %d)\n", err);
		return err;
	}
	
	if (mockStationNumber == 0)
	{
		err = advertising_set_create(&ext_adv[NON_CONNECTABLE_ADV_IDX], non_connectable_adv_param,
				     non_connectable_data0, ARRAY_SIZE(non_connectable_data0));
		if (err) {
			printk("Failed to create a non-connectable advertising set (err %d)\n", err);
		}
	} else if (mockStationNumber == 1)
	{
		err = advertising_set_create(&ext_adv[NON_CONNECTABLE_ADV_IDX], non_connectable_adv_param,
				     non_connectable_data1, ARRAY_SIZE(non_connectable_data1));
		if (err) {
			printk("Failed to create a non-connectable advertising set (err %d)\n", err);
		}
	} else if (mockStationNumber == 2)
	{
		err = advertising_set_create(&ext_adv[NON_CONNECTABLE_ADV_IDX], non_connectable_adv_param,
				     non_connectable_data2, ARRAY_SIZE(non_connectable_data2));
		if (err) {
			printk("Failed to create a non-connectable advertising set (err %d)\n", err);
		}
	} else if (mockStationNumber == 3)
	{
		err = advertising_set_create(&ext_adv[NON_CONNECTABLE_ADV_IDX], non_connectable_adv_param,
				     non_connectable_data3, ARRAY_SIZE(non_connectable_data3));
		if (err) {
			printk("Failed to create a non-connectable advertising set (err %d)\n", err);
		}
	}  else
	{
		err = advertising_set_create(&ext_adv[NON_CONNECTABLE_ADV_IDX], non_connectable_adv_param,
				     non_connectable_data, ARRAY_SIZE(non_connectable_data));
		if (err) {
			printk("Failed to create a non-connectable advertising set (err %d)\n", err);
		}
	}

	return err;
}

int app_state = APP_IDLE;
int mock_adv_station = 0;

// Callback function when button 0 is pressed
void button0_pressed_callback(const struct device *gpiob, struct gpio_callback *cb, gpio_port_pins_t pins) {
	printk("Button 0 pressed!!\n");
	mock_adv_station = 0;
	app_state = APP_BLE_ADV;
}

// Callback function when button 1 is pressed
void button1_pressed_callback(const struct device *gpiob, struct gpio_callback *cb, gpio_port_pins_t pins) {
	printk("Button 1 pressed!!\n");
	mock_adv_station = 1;
	app_state = APP_BLE_ADV;
}

// Callback function when button 2 is pressed
void button2_pressed_callback(const struct device *gpiob, struct gpio_callback *cb, gpio_port_pins_t pins) {
	printk("Button 2 pressed!!\n");
	mock_adv_station = 2;
	app_state = APP_BLE_ADV;
}
// Callback function when button 3 is pressed
void button3_pressed_callback(const struct device *gpiob, struct gpio_callback *cb, gpio_port_pins_t pins) {
	printk("Button 3 pressed!!\n");
	mock_adv_station = 3;
	app_state = APP_BLE_ADV;
}

void main(void)
{
	int err;

	printk("Starting Bluetooth multiple advertising sets example\n");

	err = dk_leds_init();
	if (err) {
		printk("LEDs init failed (err %d)\n", err);
		return;
	}

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	// Button 0 config
	gpio_pin_configure_dt(&button0_spec, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&button0_spec, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&button0_cb, button0_pressed_callback, BIT(button0_spec.pin));
	gpio_add_callback(button0_spec.port, &button0_cb);

	// Button 1 config
	gpio_pin_configure_dt(&button1_spec, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&button1_spec, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&button1_cb, button1_pressed_callback, BIT(button1_spec.pin));
	gpio_add_callback(button1_spec.port, &button1_cb);

	// Button 2 config
	gpio_pin_configure_dt(&button2_spec, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&button2_spec, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&button2_cb, button2_pressed_callback, BIT(button2_spec.pin));
	gpio_add_callback(button2_spec.port, &button2_cb);

	// Button 3 config
	gpio_pin_configure_dt(&button3_spec, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&button3_spec, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&button3_cb, button3_pressed_callback, BIT(button3_spec.pin));
	gpio_add_callback(button3_spec.port, &button3_cb);

	while (1)
	{
		switch (app_state)
		{
		case APP_IDLE:
		k_msleep(1000);
			break;
		
		case APP_BLE_ADV:
			printk("Entered case APP_BLE_ADV\n");
			if (mock_adv_station == 0)
			{
				err = non_connectable_adv_create(0);
			} else if (mock_adv_station == 1)
			{
				err = non_connectable_adv_create(1);
			} else if (mock_adv_station == 2)
			{
				err = non_connectable_adv_create(2);
			} else if (mock_adv_station == 3)
			{
				err = non_connectable_adv_create(3);
			}
			printk("Non-connectable advertising started\n");
			app_state = APP_IDLE;
			break;
		
		default:
			break;
		}
	}
}