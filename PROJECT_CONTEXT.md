# Project Context – Raspberry Pi Data Logger

## Overview

This project is a Raspberry Pi based data acquisition logger designed to record analog signals using an MCC128 DAQ HAT. Data is stored in WAV files with embedded XML metadata describing the recording hardware, configuration, and GPS location.

The system is intended for unattended field operation and automatically starts recording at boot using a systemd service.

---

# Hardware

## Main Components

* Raspberry Pi 4
* MCC128 Voltage DAQ HAT
* Adafruit Ultimate GPS Breakout (PA1616S)
* Analog sensors (up to 8 channels)

## GPS Interface

* UART serial connection
* Device: `/dev/serial0`
* Baud rate: `9600`
* Managed by `gpsd`

GPS provides:

* System time synchronization
* Latitude / longitude
* Altitude
* Fix status

---

# Software Architecture

## Main Logger

Executable:

```
dh_logger
```

Primary source files:

```
main.c
wav.c
wav.h
```

Supporting utilities:

```
dh_stop_scan
dh_check_scan
check_chunks
```

Configuration file:

```
logger.conf
```

Build system:

```
Makefile
```

---

# Logger Configuration

Example configuration:

```
[mcc128]
address = 0
channels = 0 1 2 3 4 5 6 7
range = 10V
mode = SE
sample_rate = 1000

[logger]
file_postfix = ntst
file_duration = 60
```

This configures:

* 8 analog channels
* ±10 V input range
* 1000 Hz sample rate
* 60-second WAV files

---

# Recording Behavior

At startup the logger:

1. Reads `logger.conf`
2. Initializes the MCC128 DAQ
3. Starts a scan on configured channels
4. Writes audio data to WAV files
5. Rotates files based on duration

Example filename:

```
2026-03-03T01:05:30Z-ntst.wav
```

Time is stored in **Zulu time (UTC)**.

---

# Metadata Format

Each WAV file contains an XML metadata block.

Example:

```xml
<?xml version='1.0'?>
<dataset>
  <hardware>
    <product role="daq">
      <name>MCC 128 Voltage HAT</name>
      <model version="1">HAT_ID_MCC_128</model>
      <firmware version="1.1"/>
      <item serial="DB9D63E"/>
      <address>0</address>
      <channels>
        <channel id="0">0</channel>
        <channel id="1">1</channel>
        <channel id="2">2</channel>
        <channel id="3">3</channel>
        <channel id="4">4</channel>
        <channel id="5">5</channel>
        <channel id="6">6</channel>
        <channel id="7">7</channel>
      </channels>
      <mode>SINGLE_ENDED</mode>
      <range>+/- 10V</range>
      <sample_rate>1000.000000Hz</sample_rate>
    </product>

    <product role="logger">
      <name>Raspberry Pi</name>
      <model version="4">B Rev 1.2</model>
    </product>
  </hardware>

  <time>2026-03-03T02:35:15Z</time>

  <location>
    <lat>44.6113967</lat>
    <lon>-63.6159733</lon>
    <alt_m>68.30</alt_m>
    <fix_mode>3</fix_mode>
  </location>

</dataset>
```

---

# GPS Time Synchronization

System time is synchronized using:

```
gpsd
chrony
```

Chrony configuration uses GPS as the primary reference clock.

Example chrony source:

```
#* GPS
```

Zulu time is used for:

* file naming
* metadata timestamps

---

# System Service

The logger runs automatically using systemd.

Service file:

```
~/.config/systemd/user/dh_logger.service
```

Example:

```
[Unit]
Description=DaqHat logger for NTST

[Service]
WorkingDirectory=/home/tiuser/dh-logger
ExecStart=/home/tiuser/dh-logger/dh_logger
Restart=on-failure

[Install]
WantedBy=default.target
```

---

# Current Project Status

Working features:

* MCC128 data acquisition
* 8 channel recording
* WAV file generation
* XML metadata embedding
* GPS coordinates in metadata
* GPS time synchronization
* Automatic startup via systemd

---

# Planned Improvements

Remaining project tasks:

1. Embed additional GPS information in metadata

   * satellite count
   * HDOP / accuracy

2. Differential input configuration support

3. File generation log

   * configuration
   * location
   * timestamps

4. Remote monitoring and data extraction

   * wireless access
   * remote file retrieval

---

# Development Environment

Primary development is performed on the Raspberry Pi using:

```
gcc
make
git
```

Code is version controlled using GitHub.

Repository owner:

```
IanArcadeBooth
```
