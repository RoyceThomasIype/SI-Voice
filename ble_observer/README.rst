.. _bluetooth-observer-sample:

SI Voice firmware
###################

Overview
********

The application will scan for SI stations nearby.
If any found, gets the advertising data from the device, and parses the data to get the timestamp and control station number.
On receiving the relevant data, the application sends SPI commands to the EPSON speech IC to play the audio message via a loudspeaker.

If the used Bluetooth Low Energy Controller supports Extended Scanning, you may
enable `CONFIG_BT_EXT_ADV` in the project configuration file. Refer to the
project configuration file for further details.

Requirements
************

* nrf52840 DK
* EPSON S1V3G340 Text-To-Speech IC/Rutronik RutAdaptBoard-TextToSpeech Rev-2

Building and Running
********************

Testing
=======

After programming the sample to your development kit, test it by performing the following steps:

1. |connect_terminal_specific|
#. Reset the kit.
#. Open a serial com port mointor tool of choice to see the data when in debug mode.
#. Press any button on the other nRF DK with the "_multiple_adv_sets" firmware.
#. If the connections are correct the RutAdaptBoard-TextToSpeech board will play the corresponding audio.
