#!/bin/bash

# monitor_logger.sh
# Simple health/status monitor for the dh-logger project

set -u

PROJECT_DIR="/home/tiuser/dh-logger"
SERVICE_NAME="dh_logger.service"
CSV_FILE="$PROJECT_DIR/recordings_log.csv"

cd "$PROJECT_DIR" || {
    echo "ERROR: Could not access $PROJECT_DIR"
    exit 1
}

echo "========================================"
echo "        dh-logger monitor status"
echo "========================================"
echo

# Service status
SERVICE_STATUS=$(systemctl --user is-active "$SERVICE_NAME" 2>/dev/null || echo "unknown")
echo "Logger service status : $SERVICE_STATUS"

# Latest WAV file
LATEST_WAV=$(ls -t *.wav 2>/dev/null | head -n 1)
if [ -n "${LATEST_WAV:-}" ]; then
    echo "Latest WAV file       : $LATEST_WAV"
else
    echo "Latest WAV file       : none found"
fi

# Latest CSV entry
if [ -f "$CSV_FILE" ]; then
    echo "Latest CSV entry      :"
    tail -n 1 "$CSV_FILE"
else
    echo "Latest CSV entry      : recordings_log.csv not found"
fi

# Disk space
# Disk space check
echo
FREE_GB=$(df -BG "$PROJECT_DIR" | tail -n 1 | awk '{print $4}' | tr -d 'G')

if [ -n "${FREE_GB:-}" ] && [ "$FREE_GB" -lt 20 ]; then
    echo "Disk space remaining  : $FREE_GB GB"
    echo "WARNING: disk space low"
    WARNED=1
else
    echo "Disk space remaining  : $FREE_GB GB"
fi

# System time
echo
echo "System UTC time       : $(date -u '+%Y-%m-%dT%H:%M:%SZ')"

# RTC time
RTC_INVALID=0

if command -v hwclock >/dev/null 2>&1; then
    RTC_TIME=$(sudo hwclock -r 2>/dev/null)
    if [ -n "${RTC_TIME:-}" ]; then
        echo "RTC time              : $RTC_TIME"

        RTC_YEAR=$(date -d "$RTC_TIME" +%Y 2>/dev/null)
        if [ -z "${RTC_YEAR:-}" ] || [ "$RTC_YEAR" -lt 2024 ]; then
            RTC_INVALID=1
        fi
    else
        echo "RTC time              : unavailable"
        RTC_INVALID=1
    fi
else
    echo "RTC time              : hwclock not installed"
    RTC_INVALID=1
fi

# GPS fix mode
if command -v gpspipe >/dev/null 2>&1; then
    GPS_MODE=$(gpspipe -w -n 5 2>/dev/null | grep -m1 '"mode"' | sed 's/.*"mode":\([0-9]*\).*/\1/')
    if [ -n "${GPS_MODE:-}" ]; then
        case "$GPS_MODE" in
            0|1) GPS_LABEL="No fix" ;;
            2)   GPS_LABEL="2D fix" ;;
            3)   GPS_LABEL="3D fix" ;;
            *)   GPS_LABEL="Unknown" ;;
        esac
        echo "GPS fix mode          : $GPS_MODE ($GPS_LABEL)"
    else
        echo "GPS fix mode          : unavailable"
    fi
else
    echo "GPS fix mode          : gpspipe not installed"
fi

# Warning checks
echo
echo "Warnings:"
WARNED=0

if [ "$SERVICE_STATUS" != "active" ]; then
    echo "- Logger service is not active."
    WARNED=1
fi

if [ -z "${LATEST_WAV:-}" ]; then
    echo "- No WAV files found in $PROJECT_DIR."
    WARNED=1
fi

FREE_GB=$(df -BG "$PROJECT_DIR" | tail -n 1 | awk '{print $4}' | tr -d 'G')
if [ -n "${FREE_GB:-}" ] && [ "$FREE_GB" -lt 5 ]; then
    echo "- Low disk space: less than 5 GB free."
    WARNED=1
fi

if [ "${GPS_MODE:-}" = "0" ] || [ "${GPS_MODE:-}" = "1" ]; then
    echo "- GPS has no valid fix."
    WARNED=1
fi

if [ "${RTC_INVALID:-0}" -eq 1 ]; then
    echo "- RTC time is unavailable or invalid."
    WARNED=1
fi

if [ "$WARNED" -eq 0 ]; then
    echo "No warnings."
fi

echo
echo "========================================"
