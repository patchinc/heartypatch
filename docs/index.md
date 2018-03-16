---
layout: page
homepage: true
---

# Welcome to HeartyPatch
![The HeartyPatch](images/heartypatch2.jpg)

HeartyPatch is a completely open source, single-lead, ECG-HR wearable patch with HRV (Heart Rate Variability) analysis. It is based on the popular ESP32 system-on-a-chip.

If you don't already have one, you can buy one on [Crowd Supply](https://www.crowdsupply.com/protocentral/heartypatch) campaign page. We have started shipping them out to backers.

If you face any issue in any part of this guide or with the HeartyPatch in general, please make sure to check our [Frequently Asked Questions](faq.md). If it does not address your question, please email us at support@protocentral.com.

# Getting Started with HeartyPatch

If you just want to use the HeartyPatch with the pre-loaded firmware, it comes ready to go. You do not have to do any programming to get it working. The pre-loaded firmware send HRV values over BLE.

**HeartyPatch can be used with any Android app that supports the heart-rate profile.**

## The HeartyPatch App for Android - BETA

An android app for HeartyPatch is now available that receives and displays the real-time heart rate as well as HRV trends and variables. The app scans for any HeartyPatch devices in the vicinity and shows a list of devices accessible through Bluetooth Low Energy (BLE). Please note that you would need an Android device with BLE support to use this app.

You can download and install the app from the [Google Play Store](https://play.google.com/store/apps/details?id=com.protocentral.heartypatch) in the following link:

[![HeartyPatch App for Android](images/google-play-badge.png)](https://play.google.com/store/apps/details?id=com.protocentral.heartypatch)

*Note: The android app for HeartyPatch is still in BETA and there might be some instability as a result.*

# Modes of operation

### Bluetooth LE Mode
With the preloaded firmware, the HeartyPatch will do heart-rate and R-R interval measurement and send it over BLE through a standard [Heart-rate BLE service](https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.heart_rate.xml), as well as a separate custom HRV service that will provide parameters used for HRV analysis such as SD, Mean RR, PNN50 and SDNN.

### Continuous ECG Streaming mode over WiFi/TCP

The HeartyPatch can also do live ECG streaming from the chest. This works similar to a heart-rate monitor. Below is a screen capture of the device working in ECG mode. The data is sent over a TCP socket over WiFi.

![HeartyPatch Streaming ECG](images/streaming-tcp.gif)

**To get the WiFi mode turned on, you will have to flash the firmware onto the device. The following link provides detailed step-by-step instructions about how to reprogram the HeartyPatch.**

### [Programming HeartyPatch for ECG streaming over WiFi/TCP](streaming-ecg-tcp-mode.md)

### Parts of the HeartyPatch

![HeartyPatch Parts](images/heartypatch-parts.png)

<iframe src="https://player.vimeo.com/video/249182147" width="640" height="360" frameborder="0" webkitallowfullscreen mozallowfullscreen allowfullscreen></iframe>

# Updating the firmware

It is important to always keep your firmware up-to-date for best results. We keep adding changes, bug fixes and enhancements as and when we do updates. HeartyPatch will continue to be developed and new features added.

Be on the lookout for new firmware releases on our Github releases page at https://github.com/Protocentral/protocentral_heartypatch

The firmware for the HeartyPatch's on-board Espressif ESP32 chip uses the [esp-idf framework](https://github.com/espressif/esp-idf)
 for development. You will need to install and configure the following components to setup a development environment:

**The Xtensa ESP32 toolchain**
(*Please make sure download the toolchain from the links at: http://esp-idf.readthedocs.io/en/v2.1/get-started/index.html#standard-setup-of-toolchain*)

This version of the esp-idf compiles well only with the toolchain version 1.22.0-61-gab8375a-5.2.0.

**The v2.1 release of esp-idf**

*The current version of the HeartyPatch code compiles well and performs well only with v2.1 of the ESP-IDF and version 1.22.0-61-gab8375a-5.2.0 of the Xtensa toolchain.*

[Download the ESP-IDF version 2.1 here](https://github.com/espressif/esp-idf/releases/download/v2.1/esp-idf-v2.1.zip)

`Please make sure all of the above tools are properly installed before proceeding.`

Setup guides for these components are available on the [ESP-IDF documentation site](https://esp-idf.readthedocs.io/en/v2.1/get-started/index.html).

You can then clone our [Github repository](https://github.com/Protocentral/protocentral_heartypatch) of code for the ESP32:

`git clone https://github.com/Protocentral/protocentral_heartypatch.git`

[or download a zip file of the latest master branch](https://github.com/Protocentral/protocentral_heartypatch/archive/master.zip).

Download this zip file, rename it to whatever you want to. Change to this directory and then start building.

The folder "heartypatch-ble" in the "Firmware" contains the  code of preloaded firmware on the heartypatch board, which calculates RR interval, heart rate and time domain parameters for hrv analysis and sends them through BLE.

Now that you have the code and ready to build, you will need to configure the build options for heartypatch through menuconfig utility:

* Open the terminal and navigate to the folder which contains the heartypatch code 	
* Start the project configuration utility **menuconfig**     		
`make menuconfig`

![menuconfig](images/makemenuconfig.png)

* Configure your serial port under *Serial flasher config -> Default serial port*
![port](images/serialport.png)

* Use the *Heartypatch configuration* to enable ble mode.
![heartypatch-config](images/heartypatch-config-ble.png)

BLE mode at this time does not support ECG stream since max30003 sensor is configured for rtor detectoin. By enabling the wifi, you may get hr and rr values through TCP in the heartypatch GUI. For ECG strem you may use the heartypatch-stream-tcp code [protocentral-heartypatch/firmware/heatypatch-stream-tcp]

* Navigate to component config -> esp32-specific ->main XTAL frequency and select 26 Mhz as the board crystal
![xtal](images/main-xtal-frequency.png)

* save the configuration by selecting `<save>` and close menuconfig

Run the makefile (assuming previous steps are done correctly):

`make` or `make -j5`

To flash the firmware on to the board, just use:

`make flash`

If the flashing is successful, you should see something similar to the below screen:

![ESP Success](images/esp-flash-success.png)

**ECG STREAMING**

The folder `heartypatch-stream-tcp` contains the code for streaming ECG.

Before flashing this code, enable wifi, TCP and mdns through `makemenuconfig` similar to the process described in the previous section.

* Start makemenuconfig and navigate to *Heartypatch configuration*, set your wifi ssid and password, enable mdns and TCP:
![Heartypatch config](images/heartypatch-config-tcp.png)

* Configure the serial port under Serial flasher config -> Default serial port
* Navigate to *Component config -> esp32-specific ->main XTAL frequency* and select 26 MHz as the board crystal

Run the makefile (assuming previous steps are done correctly):

`make` or `make -j5`

To flash the firmware on to the board, just use:

`make flash`

If the flashing is successful, you should see something similar to the below screen:

![ESP Success](images/esp-flash-success.png)

You can open the IDF monitor to see the debug messages by using the command:

`make monitor`

![idf-monitor](images/idf-monitor-tcp-connection.png)

Once the heartypatch is connected with the wifi, open the gui from the project folder and you should be able see the ECG stream.  

More information about this process and the ESP32 framework is available in the [ESP32 Get Started Guide](http://esp-idf.readthedocs.io/en/latest/get-started/).

# Frequently Asked Questions

**Why is the heart-rate not stable? There's a lot of variation**

Do not worry, that's what is [Heart-rate variability](https://en.wikipedia.org/wiki/Heart_rate_variability) and HeartyPatch displays real-time beat-to-beat heart rate. Heart-rate is supposed to vary from beat-to-beat.

**How do I get the full ECG signal from the device?**

Please read our [Guide to program the device the TCP Streaming mode](streaming-ecg-tcp-mode.md)

**I have problems getting the code to compile. What do I do?**

Most of the problems in compilation arise from having the wrong versions of the ESP-IDF. Please make sure the versions of the esp-idf as well as the Xtensa toochain match the ones given in out [Guide to firmware upgrades](firmware-upgrades.md)

**Help, there is not LED flashing on the device.**

By default, the LED would not flash. This is done to conserve battery power. This can be enabled in the software though.

*We will continue to add to this list based on user input, so please check this list if something goes wrong.*

# License Information

This product is open source! Both, our hardware and software are open source and licensed under the following licenses:

## Hardware

All hardware is released under [Creative Commons Share-alike 4.0 International](http://creativecommons.org/licenses/by-sa/4.0/).

![CC-BY-SA-4.0](https://i.creativecommons.org/l/by-sa/4.0/88x31.png)

## Software

All software is released under the MIT License(http://opensource.org/licenses/MIT).

Please check [*LICENSE.md*](LICENSE.md) for detailed license descriptions.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
