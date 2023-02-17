.. _multiple_adv_sets:

Multiple advertising SI stations
####################################

.. contents::
   :local:
   :depth: 2

The Multiple advertising SI stations sample mocks the functionality of four different SI stations with each button press on the nRF52840 DK.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

.. include:: /includes/tfm.txt

Overview
********
When you start the sample, it will wait for a button click on the nrf52840 DK. Button click emulates a punch on the SI station.
Each button emulates a SI control point.
To test the sample use your scanner device after each button click to observe the advertiser ``SI Beacon``.

Building and running
********************
.. |sample path| replace:: :file:`samples/bluetooth/multiple_adv_sets`

.. include:: /includes/build_and_run_ns.txt

.. _multiple_adv_sets_testing:

Testing
=======

After programming the sample to your dongle or development kit, test it by performing the following steps:

1. |connect_terminal_specific|
#. Reset the kit.
#. Start the `nRF Connect for Mobile`_ application on your smartphone or tablet.
#. Click the first button on the DK.
#. Start scanning on the `nRF Connect for Mobile`_ application.
   
   The device is advertising as ``SI Beacon``.

#. Click second button on the DK.
#. Repeat step 5.
#. Repeat the same with the other two buttons on the DK.

Dependencies
************

This sample uses the following |NCS| libraries:

* :ref:`dk_buttons_and_leds_readme`

In addition, it uses the following Zephyr libraries:

* ``include/kernel.h``

* :ref:`zephyr:bluetooth_api`:

  * ``include/bluetooth/bluetooth.h``
  * ``include/bluetooth/conn.h``
  * ``include/bluetooth/uuid.h``
  * ``include/bluetooth/services/dis.h``

The sample also uses the following secure firmware component:

* :ref:`Trusted Firmware-M <ug_tfm>`
