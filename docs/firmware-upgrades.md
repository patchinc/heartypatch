# Compiling HeartyPatch Firmware

The firmware for the HeartyPatch's on-board Espressif ESP32 chip uses the [esp-idf framework](https://github.com/espressif/esp-idf)
 for development. You will need to install and configure the following components to setup a development environment:

* The Xtensa ESP32 toolchain
* The v2.1 release of esp-idf
* Supporting software such as Python and pyserial

**Please make sure all of the above tools are properly installed before proceeding.**

Setup guides for these components are available on the [ESP-IDF documentation site](https://esp-idf.readthedocs.io/en/latest/get-started/index.html).

You can then clone our [Github repository](https://github.com/Protocentral/protocentral_heartypatch) of code for the ESP32 or [download a zip file of the latest master branch](https://github.com/Protocentral/protocentral_heartypatch/archive/master.zip).

Download this zip file, rename it to whatever you want to. Change to this directory and then start building.

Use the menuconfig utility to configure the USB port to which the HeartyPatch is connected and all the other build options.

`make menuconfig`

Run the makefile (assuming the toolchain is completely setup):

`make` or `make -j5`

To flash the firmware one to the board, just use:

`make flash`

If the flashing is successful, you should see something similar to the below screen:

![ESP Success](images/esp-flash-success.png)

More information about this process and the ESP32 framework is available in the [Get Started Guide](http://esp-idf.readthedocs.io/en/latest/get-started/).
