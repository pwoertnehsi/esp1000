# ESP1000

This project is targeting **PSP-E1000 (Street)** which lacks native Wi-Fi support. It uses an **ESP32-S3** as a USB modem with userspace driver and software implementation that integrates into Sony's native WLAN infrastructure.

Current state of this codebase is highly experimental and I hold no resposibility for you damaging/bricking your console. It is made using an **LLM-based Autonomous Agent** and the documentation therefore might be of poor quality/outdated. I do not discourage you opening issues as a user or a developer, however, these are much welcomed.

The plugin could be enabled at any time and would not interfere with USB file transfer feature or XMB camera app. Plugin was only tested with **ARK-5 CFW** of firmware version 6.61.

Modern SSL and WPA2-PSK are handled by the ESP32-S3 - there's no need to install any additional compatibility plugins.

## Implementation status - Roadmap

|Feature           |State|
|------------------|-----|
|Ad Hoc multiplayer|Only tested in Gran Turismo (UCES01245), Car transfer and Multiplayer races (PSP to PPSSPP) work.|
|Native Browser    |Pretty stable performance, may crash on an exit while the page is still loading.|
|Native RSS        |**Not implemented:** The feed itself can be added to the list, but console crashes upon trying to import its contents (tested with [opennet RSS feed](https://www.opennet.ru/opennews/opennews_all.rss)).|
|Network settings  |Fully implemented Ad Hoc Mode, Infrastructure Mode and RSS Channel Settings|
|Game Sharing      |**Not implemented** and not tested|
|Remote Play       |**Not implemented** and not tested|
|Internet Radio    |**Not implemented:** opens .prs radio pages but does not start playing|

## ESP32-S3 and hardware setup

You'd need a Mini-USB B breakout cable with following connections:
|Pin|Assignment|
|-|-|
|VBUS|ESP32-S3 **3V3** series with a 1K resistor
|D-|ESP32-S3 **GPIO19**
|D+|ESP32-S3 **GPIO20**
|GND|ESP32-S3 **GND**

You also need to make some kind of connection with side pads that actually carry power to the PSP accessories to power the ESP32-S3. The left one carries 3V and the right one is GND if looked at from the PSP's front face. I recommend using pogo pins or solid core wire to make those connections.

The setup was tested with an ESP32-S3-N32R16 but any ESP32-S3 would do as this project does not make use of PSRAM.

# Quick start guide

Make sure you have **psp-sdk**, **psp-binutils** and **ESP-IDF v6.1** installed.

## Building

Clone the repository:
```
git clone --recurse-submodules https://git.patchouliknowledge.com/tenshi/esp1000
# OR
git clone --recurse-submodules https://github.com/pwoertnehsi/esp1000
```

Your toolchains must be on PATH:
```
export PSPDEV=/opt/pspdev
export PATH="$PSPDEV/bin:$PATH"
source /opt/esp-idf/export.sh
```

Then build:
```
make -j$(nproc) production esp
```

## Installation

See [Build guide](./docs/BUILD_INSTALL.md) for more info on how to build and install.
