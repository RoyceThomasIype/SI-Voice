/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/spi.h>
#include "isc_msgs.h"
#include <hal/nrf_gpio.h>

/* Set DEBUG_ENABLE to see all debug messages*/
#define DEBUG_ENABLE	0

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   10000

// GPIO Control Pins for the EPSON speech IC
#define H_RESET_PIN		NRF_GPIO_PIN_MAP(0, 14)
#define H_MUTE_PIN		NRF_GPIO_PIN_MAP(0, 15)
#define H_STBEXT_PIN	NRF_GPIO_PIN_MAP(0, 16)

#define MY_SPI_MASTER DT_NODELABEL(my_spi_master)

// SPI master functionality
const struct device *spi_dev;
static struct k_poll_signal spi_done_sig = K_POLL_SIGNAL_INITIALIZER(spi_done_sig);

struct spi_cs_control spim_cs = {
	.gpio = SPI_CS_GPIOS_DT_SPEC_GET(DT_NODELABEL(reg_my_spi_master)),
	.delay = 0,
};

uint8_t tx_buffer[70];		/* Note: Transmit buffer size should be large enough to send the entire SPI message. SPI message length increases with the number of phrases to be played. Each new phrase will approximately add 8 bytes to the total message length.*/
uint8_t rx_buffer[16];

const struct spi_buf tx_buf = {
	.buf = tx_buffer,
	.len = sizeof(tx_buffer)
};
const struct spi_buf_set tx = {
	.buffers = &tx_buf,
	.count = 1
};

struct spi_buf rx_buf = {
	.buf = rx_buffer,
	.len = sizeof(rx_buffer),
};
const struct spi_buf_set rx = {
	.buffers = &rx_buf,
	.count = 1
};

void printBuffer(uint8_t buffer[], int len)
{
  for(int i=0; i < len; i++) {
      printk("0x%.2x ", buffer[i]);
  }
  printf("\r\n");
}

///////////////////////////////////////////////////////////////////////
//  function: GPIO_ControlStandby
//
//  description:
//    STAND-BY control for Device STBY(Stand-by) High/Low control
//
//  argument:
//    iValue    Signal value High:1  Low:0
///////////////////////////////////////////////////////////////////////
void GPIO_ControlStandby(int iValue)
{
  if (iValue==1)
  {
    // Write 1 to P0.16 - H_STBEXIT pin
	nrf_gpio_pin_set(H_STBEXT_PIN);
  }
  else
  {
    // Write 0 to P0.16 - H_STBEXIT pin
	nrf_gpio_pin_clear(H_STBEXT_PIN);
  }
}

///////////////////////////////////////////////////////////////////////
//  function: GPIO_ControlMute
//
//  description:
//    MUTE control for Device MUTE control
//
//  argument:
//    iValue    Signal value Mute  enable:1  disable:0
///////////////////////////////////////////////////////////////////////
void GPIO_ControlMute(int iValue)
{
  if (iValue)
  {
    // Write 1 to P0.15 - H_MUTE pin
	nrf_gpio_pin_set(H_MUTE_PIN);
  }
  else
  {
    // Write 0 to P0.15 - H_MUTE pin
	nrf_gpio_pin_clear(H_MUTE_PIN);
  }
}

///////////////////////////////////////////////////////////////////////
//  function: GPIO_S1V3G340_Reset
//
//  description:
//    RESET control for Device reset control
//
//  argument:
//    iValue    Signal value High:1  Low:0
///////////////////////////////////////////////////////////////////////
void GPIO_S1V3G340_Reset(int iValue)
{
  if (iValue)
  {
    // Write 1 to P0.14 - H_RESET pin
	nrf_gpio_pin_set(H_RESET_PIN);
  }
  else
  {
    // Write 0 to P0.14 - H_RESET pin
	nrf_gpio_pin_clear(H_RESET_PIN);
  }
}

///////////////////////////////////////////////////////////////////////
//  function: updateTxBuffer
//
//  description:
//    Loads the transmit buffer with the message to be sent via SPI
//
//  argument:
//    msgBuf: SPI message to be sent to the speech IC
//	  len: length of the message to be transmitted 
///////////////////////////////////////////////////////////////////////
static void updateTxBuffer(unsigned char msgBuf[], int len) 
{
	size_t i = 0;
	for (i = 0; i < len; i++)
	{
		tx_buffer[i] = msgBuf[i];
	}
	//clearing rest of the buffer
	for (size_t j = i; j < sizeof(tx_buffer); j++)
	{
		tx_buffer[j] = 0;
	}
}

static void spi_init(void)
{
	spi_dev = DEVICE_DT_GET(MY_SPI_MASTER);
	if(!device_is_ready(spi_dev)) {
		if(DEBUG_ENABLE) printk("SPI master device not ready!\n");
	}
	if(!device_is_ready(spim_cs.gpio.port)){
		if(DEBUG_ENABLE) printk("SPI master chip select device not ready!\n");
	}
}

static const struct spi_config spi_cfg = {
	.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB |
				 SPI_MODE_CPOL | SPI_MODE_CPHA,
	.frequency = 1000000,
	.slave = 0,
	.cs = &spim_cs,
};

/*
Note: Phrase numbers stored on the speech IC to play the audio "Reached control 1 in 1 hour 15 minutes":
	PS_0203 - (0x00CB - 1) = 0x00CA (Reached control)
	PS_0143 - (0x008F - 1) = 0x008E (1)
	PS_0204 - (0x00CC - 1) = 0x00CB (in)
	PS_0001 - (0x0001 - 1) = 0x0000 (1 hour)
	PS_0039 - (0x0027 - 1) = 0x0026 (15 minutes)
*/
/*iscSequencerConfigReq acts as a format placeholder to play phrases in sequence. To dynamically play specific audio phrases, the "file event" structure should be modified.*/
unsigned char iscSequencerConfigReq[] = {
		0x00, 0xAA, 0x30, 0x00, 0xC4, 0x00, 0x01, 0x00, 0x05, 0x00,
		// file event PS_0203 - "Reached control"
		0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0xCA, 0x00,
		// file event - station number
		0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x8E, 0x00,
		// file event PS_0204 - "in"
		0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0xCB, 0x00,
		// file event - hours
		0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00,
		// file event - minutes
		0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x26, 0x00,
		// padding data to skip ISC_SEQUENCER_CONFIG_RESP.
		//0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

bool msgHasHours = false;

/*iscSequencerConfigReqWithoutHours acts as a format placeholder to play announcements without hours. For eg, "Reached control 1 in 30 minutes."*/
unsigned char iscSequencerConfigReqWithoutHours[] = {
		0x00, 0xAA, 0x28, 0x00, 0xC4, 0x00, 0x01, 0x00, 0x04, 0x00,
		// file event PS_0203 - "Reached control"
		0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0xCA, 0x00,
		// file event - station number
		0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x8E, 0x00,
		// file event PS_0204 - "in"
		0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0xCB, 0x00,
		// file event - minutes
		0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x26, 0x00,
	};

///////////////////////////////////////////////////////////////////////
//  function: createIscSequencerConfigReq
//
//  description:
//    Function to parse the incoming BLE data from SIAC and get the 
// 	  control number, hours and minutes data from it and update the 
// 	  iscSequencerConfigReq or iscSequencerConfigReqWithoutHours
//
//  argument:
//    siac_data: Data recieved from SIAC via BLE. This function is 
//    called from the observer.c file
///////////////////////////////////////////////////////////////////////
void createIscSequencerConfigReq(char siac_data[]) {
	
	uint8_t controlNumber = 0, hours = 0, minutes = 0;
	for (int i = 0; i < strlen(siac_data); i++)
	{
		if (siac_data[i] == 0x07)
		{
			/* Parse SIAC data */
			controlNumber = siac_data[i+1];
			hours = siac_data[i+2];
			minutes = siac_data[i+3];
		}
	}
	if(DEBUG_ENABLE) printk("control no: %d, hours: %d, minutes: %d\n", controlNumber, hours, minutes);

	uint8_t hoursMsgCode = hours - 1;
	uint8_t minutesMsgcode = (minutes + 24) - 1;
	uint8_t controlMsgCode = (controlNumber + 142) - 1;
	if(DEBUG_ENABLE) printk("control code: 0x%.2x, hours code: 0x%.2x, minutes code: 0x%.2x\n", controlMsgCode, hoursMsgCode, minutesMsgcode);

	if (hours == 0)
	{
		msgHasHours = false;
		iscSequencerConfigReqWithoutHours[24] = controlMsgCode;
		iscSequencerConfigReqWithoutHours[40] = minutesMsgcode;
	} else {
		msgHasHours = true;
		iscSequencerConfigReq[24] = controlMsgCode;
		iscSequencerConfigReq[40] = hoursMsgCode;
		iscSequencerConfigReq[48] = minutesMsgcode;
	}
}

int S1V3G340_Initialize_Audio_Config(void) {

	/***************************Reset speech IC***************************/
	// send ISC_RESET_REQ
	updateTxBuffer(aucIscResetReq, iIscResetReqLen);
	if(DEBUG_ENABLE) printBuffer(tx_buffer, iIscResetReqLen);
	// Reset signal
	k_poll_signal_reset(&spi_done_sig);
	// Start transaction
	int error = spi_transceive_async(spi_dev, &spi_cfg, &tx, &rx, &spi_done_sig);
	if(error != 0){
		if(DEBUG_ENABLE) printk("SPI transceive error: %i\n", error);
		return error;
	}
	// Wait for the done signal to be raised and log the rx buffer
	int spi_signaled, spi_result;
	do{
		k_poll_signal_check(&spi_done_sig, &spi_signaled, &spi_result);
	} while(spi_signaled == 0);
	if(DEBUG_ENABLE) printBuffer(rx_buffer, LEN_ISC_RESET_RESP);

	/***************************Registry key-code***************************/
	// send ISC_TEST_REQ
	updateTxBuffer(aucIscTestReq, iIscTestReqLen);
	if(DEBUG_ENABLE) printBuffer(tx_buffer, iIscTestReqLen);

	error = spi_transceive_async(spi_dev, &spi_cfg, &tx, &rx, &spi_done_sig);
	if(error != 0){
		if(DEBUG_ENABLE) printk("SPI transceive error: %i\n", error);
		return error;
	}
	do{
		k_poll_signal_check(&spi_done_sig, &spi_signaled, &spi_result);
	} while(spi_signaled == 0);
	if(DEBUG_ENABLE) printBuffer(rx_buffer, LEN_ISC_TEST_RESP);

	/***************************Get version info.***************************/
	// send ISC_VERSION_REQ	
	updateTxBuffer(aucIscVersionReq, iIscVersionReqLen);
	if(DEBUG_ENABLE) printBuffer(tx_buffer, iIscVersionReqLen);

	error = spi_transceive_async(spi_dev, &spi_cfg, &tx, &rx, &spi_done_sig);
	if(error != 0){
		if(DEBUG_ENABLE) printk("SPI transceive error: %i\n", error);
		return error;
	}
	do{
		k_poll_signal_check(&spi_done_sig, &spi_signaled, &spi_result);
	} while(spi_signaled == 0);
	if(DEBUG_ENABLE) printBuffer(rx_buffer, LEN_ISC_VERSION_RESP);

	/***********************Set volume & sampling freq.***********************/
	// send ISC_AUDIO_CONFIG_REQ
	updateTxBuffer(aucIscAudioConfigReq, iIscAudioConfigReqLen);
	if(DEBUG_ENABLE) printBuffer(tx_buffer, iIscAudioConfigReqLen);

	error = spi_transceive_async(spi_dev, &spi_cfg, &tx, &rx, &spi_done_sig);
	if(error != 0){
		if(DEBUG_ENABLE) printk("SPI transceive error: %i\n", error);
		return error;
	}
	do{
		k_poll_signal_check(&spi_done_sig, &spi_signaled, &spi_result);
	} while(spi_signaled == 0);
	if(DEBUG_ENABLE) printBuffer(rx_buffer, LEN_ISC_AUDIO_CONFIG_RESP);

	if(DEBUG_ENABLE) printk("Initialization complete!!!\n");

	return 0;
}

int S1V3G340_Play_Specific_Audio(char siac_data[]) {

	if(DEBUG_ENABLE) printk("Playing audio!!!\n");
	
	/***************************Sequencer configuration***************************/
	// send ISC_SEQUENCER_CONFIG_REQ
	createIscSequencerConfigReq(siac_data);
	if (msgHasHours)
	{
		updateTxBuffer(iscSequencerConfigReq, sizeof(iscSequencerConfigReq));
		if(DEBUG_ENABLE) printBuffer(tx_buffer, sizeof(iscSequencerConfigReq));
	} else {
		updateTxBuffer(iscSequencerConfigReqWithoutHours, sizeof(iscSequencerConfigReqWithoutHours));
		if(DEBUG_ENABLE) printBuffer(tx_buffer, sizeof(iscSequencerConfigReqWithoutHours));
	}

	// Start SPI transaction
	int error = spi_transceive_async(spi_dev, &spi_cfg, &tx, &rx, &spi_done_sig);
	if(error != 0){
		if(DEBUG_ENABLE) printk("SPI transceive error: %i\n", error);
		return error;
	}
	int spi_signaled, spi_result;
	do{
		k_poll_signal_check(&spi_done_sig, &spi_signaled, &spi_result);
	} while(spi_signaled == 0);
	if(DEBUG_ENABLE) printBuffer(rx_buffer, LEN_ISC_SEQUENCER_CONFIG_RESP);

	/***************************Start sequencer playback***************************/
	aucIscSequencerStartReq[6] = 0;		// set notify status ind
	// send ISC_SEQUENCER_START_REQ
  	updateTxBuffer(aucIscSequencerStartReq, iIscSequencerStartReqLen);
	if(DEBUG_ENABLE) printBuffer(tx_buffer, iIscSequencerStartReqLen);

	error = spi_transceive_async(spi_dev, &spi_cfg, &tx, &rx, &spi_done_sig);
	if(error != 0){
		if(DEBUG_ENABLE) printk("SPI transceive error: %i\n", error);
		return error;
	}
	do{
		k_poll_signal_check(&spi_done_sig, &spi_signaled, &spi_result);
	} while(spi_signaled == 0);
	if(DEBUG_ENABLE) printBuffer(rx_buffer, LEN_ISC_SEQUENCER_START_RESP);

	return 0;
}

int spi_write_test_msg(char siac_data[])
{
	S1V3G340_Initialize_Audio_Config();
	S1V3G340_Play_Specific_Audio(siac_data);
	return 0;
}

int observer_start(void);

void main(void)
{
	int err;
	printk("Starting SI Voice Audio device\n");

	//EPSON S1V3G340 Control pins config
	nrf_gpio_cfg_output(H_RESET_PIN);
	nrf_gpio_cfg_output(H_MUTE_PIN);
	nrf_gpio_cfg_output(H_STBEXT_PIN);

	GPIO_S1V3G340_Reset(0);
	GPIO_ControlStandby(0);		// Set stanby signal(STBYEXIT) to Low(deassert)
	GPIO_ControlMute(0);        // Set mute signal(MUTE) to Low(enable)
	GPIO_S1V3G340_Reset(1);
	GPIO_ControlMute(1);        // Set mute signal(MUTE) to High(disable)
	k_msleep(120);    			// To ensure wait for "t1" as 120msec.

	spi_init();

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		if(DEBUG_ENABLE) printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	(void)observer_start();

	if(DEBUG_ENABLE) printk("Exiting %s thread.\n", __func__);

	while (1) {
		k_msleep(SLEEP_TIME_MS);
	}
}