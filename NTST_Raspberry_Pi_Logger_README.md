# NTST Raspberry Pi Data Logger

## Overview

This project implements a **field‑deployable data acquisition system**
using a Raspberry Pi and MCC128 DAQ HAT.\
The system continuously records analog sensor data to WAV files while
embedding metadata and maintaining an indexed CSV event log.

The logger is designed for **long‑term unattended deployments** and
includes:

-   GPS disciplined time synchronization
-   Embedded metadata inside recordings
-   Automatic service startup using systemd
-   Hardware configuration detection using GPIO switches
-   Physical status monitoring via LED indicators and push button
-   System health monitoring scripts

------------------------------------------------------------------------

## System Architecture

Sensors → MCC128 DAQ → Raspberry Pi → WAV Recordings → CSV Event Log →
Monitoring Tools

Key subsystems:

  Component      Purpose
  -------------- -------------------------------------
  MCC128         Analog data acquisition
  dh_logger      Main acquisition program
  WAV files      Sensor recordings
  XML metadata   Recording configuration information
  CSV log        Searchable recording index
  GPS            Accurate UTC time source
  RTC            Boot‑time fallback
  LED monitor    Physical system health indicator

------------------------------------------------------------------------

## Hardware

### Raspberry Pi System

The system runs on a Raspberry Pi equipped with:

-   MCC128 DAQ HAT
-   GPS receiver (Adafruit PA1616S)
-   optional RTC module
-   status LEDs
-   push‑button monitor

------------------------------------------------------------------------

### GPIO Configuration

#### Input Configuration Switches

Hardware switches determine channel pairing mode.

  Pair   Channels   GPIO
  ------ ---------- --------
  1      1 & 5      GPIO17
  2      2 & 6      GPIO27
  3      3 & 7      GPIO22
  4      4 & 8      GPIO23

Switch states are embedded into recording metadata.

------------------------------------------------------------------------

### Status Monitor Hardware

  Device      GPIO     Pin
  ----------- -------- --------
  Button      GPIO5    Pin 29
  Green LED   GPIO6    Pin 31
  Red LED     GPIO13   Pin 33

The push button triggers a system health check.

------------------------------------------------------------------------

## Software Components

### Main Logger

Program:

    dh_logger

Responsibilities:

-   configure MCC128
-   read analog channels
-   record data to WAV files
-   embed metadata
-   update CSV event log

Key source files:

    main.c
    wav.c
    wav.h

------------------------------------------------------------------------

## WAV Recording System

The logger records:

-   multi‑channel analog data
-   32‑bit floating point samples
-   metadata embedded in WAV files

Metadata includes:

-   GPS coordinates
-   timestamp
-   channel configuration
-   input pairing mode
-   system configuration

------------------------------------------------------------------------

## CSV Event Log

Each recording event is added to:

    recordings_log.csv

Example entry:

    timestamp,filename,rms,lat,lon,mode1,mode2,mode3,mode4

This allows recordings to be indexed without opening WAV files.

------------------------------------------------------------------------

## Time Synchronization

Time synchronization follows this pipeline:

    GPS → gpsd → chrony → system clock → dh_logger

Characteristics:

-   System runs in UTC
-   GPS is the primary time source
-   RTC provides boot‑time fallback
-   No internet dependency

------------------------------------------------------------------------

## System Services

Two custom systemd services control the system.

### Logger Service

    dh_logger.service

Runs the main acquisition program.

### Status Monitor Service

    status_button_monitor.service

Runs the push‑button LED monitoring interface.

Both services automatically start on boot.

------------------------------------------------------------------------

## Monitoring Tools

### monitor_logger.sh

Human‑readable system status report.

Displays:

-   service status
-   latest recording
-   disk space
-   GPS fix
-   RTC time

Run with:

    ./monitor_logger.sh

------------------------------------------------------------------------

### health_check.sh

Machine‑readable health check used by the LED monitor.

Return codes:

  Code   Meaning
  ------ ------------------------
  0      System healthy
  1      Logger service stopped
  2      No recordings detected
  3      GPS has no fix
  4      Disk space low

------------------------------------------------------------------------

## Hardware Status Interface

Hold the button for **\~2 seconds** to run a health check.

### LED Status Codes

  LED Pattern     Meaning
  --------------- ----------------
  Green (solid)   System healthy
  1 red blink     Logger stopped
  2 red blinks    No recordings
  3 red blinks    GPS no fix
  4 red blinks    Disk space low

Each pattern repeats three times for visibility in field conditions.

------------------------------------------------------------------------

## Project Structure

    dh-logger/
    │
    ├── main.c
    ├── wav.c
    ├── wav.h
    │
    ├── monitor_logger.sh
    ├── health_check.sh
    ├── status_button_monitor.sh
    │
    ├── systemd/
    │   ├── dh_logger.service
    │   └── status_button_monitor.service
    │
    ├── docs/
    │   ├── RTC_SETUP.txt
    │   ├── GPS_SETUP.txt
    │   └── NTST_LED_Status_Monitor_Technical_Brief.txt
    │
    └── README.md

------------------------------------------------------------------------

## Useful Commands

### Logger control

Start logger

    systemctl --user start dh_logger.service

Stop logger

    systemctl --user stop dh_logger.service

Restart logger

    systemctl --user restart dh_logger.service

Check status

    systemctl --user status dh_logger.service

------------------------------------------------------------------------

### Monitor service

Check monitor

    systemctl --user status status_button_monitor.service

Restart monitor

    systemctl --user restart status_button_monitor.service

------------------------------------------------------------------------

### System diagnostics

View logs

    journalctl --user -u dh_logger.service -f

Check GPS

    cgps

Check disk space

    df -h

List recordings

    ls -lt *.wav

------------------------------------------------------------------------

## Design Goals

This system was designed to provide:

-   reliable long‑term recording
-   precise GPS timestamping
-   minimal maintenance
-   easy field diagnostics
-   reproducible deployment
-   clear data traceability

------------------------------------------------------------------------

## License

Intended for research and monitoring applications.
