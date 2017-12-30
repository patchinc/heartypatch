![The HeartyPatch](images/heartypatch2.jpg)

HeartyPatch is a completely open source, single-lead, ECG-HR wearable patch with HRV (Heart Rate Variability) analysis. It is based on the popular ESP32 system-on-a-chip.

If you don't already have one, you can buy one on [Crowd Supply](https://www.crowdsupply.com/protocentral/heartypatch) campaign page. We have started shipping them out to backers.

# Getting Started with HeartyPatch

If you just want to use the HeartyPatch with the pre-loaded firmware, it comes ready to go. You do not have to do any programming to get it working.

With the preloaded firmware, the HeartyPatch will do heart-rate and R-R interval measurement and send it over BLE through a standard [Heart-rate BLE service](https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.heart_rate.xml), as well as a separate custom HRV service that will provide parameters used for HRV analysis such as SD, Mean RR, PNN50 and SDNN.

**HeartyPatch can be used with any Android app that supports the heart-rate profile.**

Parts of the HeartyPatch:

![HeartyPatch Parts](images/heartypatch-parts.png)

<iframe src="https://player.vimeo.com/video/249182147" width="640" height="360" frameborder="0" webkitallowfullscreen mozallowfullscreen allowfullscreen></iframe>

## The HeartyPatch App for Android - BETA

An android app for HeartyPatch is now available that receives and displays the real-time heart rate as well as HRV trends and variables. The app scans for any HeartyPatch devices in the vicinity and shows a list of devices accessible through Bluetooth Low Energy (BLE). Please note that you would need an Android device with BLE support to use this app.

You can download and install the app from the [Google Play Store](https://play.google.com/store/apps/details?id=com.protocentral.heartypatch) in the following link:

[![HeartyPatch App for Android](images/google-play-badge.png)](https://play.google.com/store/apps/details?id=com.protocentral.heartypatch)

*Note: The android app for HeartyPatch is still in BETA and there might be some instability as a result.*

## Updating the firmware

It is important to always keep your firmware up-to-date for best results. We keep adding changes, bug fixes and enhancements as and when we do updates. HeartyPatch will continue to be developed and new features added.

Be on the lookout for new firmware releases on our Github releases page at:

https://github.com/Protocentral/protocentral_heartypatch

To install the new firmware and/or try out your own firmware, check out new [Guide to Firmware Upgrades](firmware-upgrades.md).

*Please submit an [issue on Github](https://github.com/Protocentral/protocentral-healthypi-v3/issues/new) if you face any problems with the HealthyPi.*

# License Information

This product is open source! Both, our hardware and software are open source and licensed under the following licenses:

## Hardware

All hardware is released under [Creative Commons Share-alike 4.0 International](http://creativecommons.org/licenses/by-sa/4.0/).

![CC-BY-SA-4.0](https://i.creativecommons.org/l/by-sa/4.0/88x31.png)

## Software

All software is released under the MIT License(http://opensource.org/licenses/MIT).

Please check [*LICENSE.md*](LICENSE.md) for detailed license descriptions.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
