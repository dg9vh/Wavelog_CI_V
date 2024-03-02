# Wavelog_CI_V

## Introduction
This is a CI-V Connector for Wavelog that can be running on an ESP32 developer board.

## Needed hardware
* ESP 32 board
* 3,5 mm jack plug
* some wire
* diode
* 1x 4k7 resistor
* 1x 10k resistor

## Wiring schedule
[Schematics.pdf](/Documents/Schematics.pdf)

## Features
This connects via CI-V to your Icom Transceiver (e.g. IC-7300) on the 3,5 mm jack plug port.
From there it reads following measurements:
* Working frequency
* Working mode
* Working rf-power-level

This values are transported to the radio hardware API of Wavelog (Cloudlog also works) and
there this data can be used for logging purpose.

## Prerequisites
To have this working properly, you need to have your transceiver in "transceive = on"-mode for 
CI-V. Also it should be configured for 19200 baud on the serial output (not on USB-port!).

## Starting from Scratch
First time you start this code (or every time until your local WiFi-network is configured, the
ESP32 starts up with an ad-hoc-wlan with the name "Wavelog_CI_V_AP" and the pre-shared key
12345678 to connect to the ESP32 in order to configure the production WiFi-network it should
connect to.

This is done by connecting to this WiFi-network with a client (phone, tablet, notebook, pc) and 
open the configuration site http://192.168.4.1 to chose your network to use. Enter your PSK and 
then the ESP32 connects to your WiFi. It should optain an ip-address in your LAN via DHCP and 
would listen on the new ip-address with a configuration dialog.

On this dialog you configure the URL of your wavelog installation (for example http://wavelog.dg9vh.de)
and the API-endpoint (for example /index.php/api/radio). Also the Wavelog API Key is needed beneath
the CI-V address of your transceiver (for example 0x94 for an IC-7300).

Save this and be ready to rumble :-)

## How it's working?
From now on every change of one of the parameters mentioned above this would be transported to
your wavelog instance via the API to be available for logging.

## Credits
Some parts of this code (for example fundamentals of the CI-V-communication) where taken from 
Patric (DF7ZZ) and his "Antennenumschalter ESP32"-code. So thank you Patric for your inspirations!
