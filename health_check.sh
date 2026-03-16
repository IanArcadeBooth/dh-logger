#!/bin/bash

PROJECT_DIR="/home/tiuser/dh-logger"
SERVICE_NAME="dh_logger.service"

cd "$PROJECT_DIR" || exit 9

# Return codes
# 0 = healthy
# 1 = logger service stopped
# 2 = no wav recordings
# 3 = GPS no fix
# 4 = disk space low

# --------------------------------
# Check logger service
# --------------------------------

SERVICE_STATUS=$(systemctl --user is-active "$SERVICE_NAME" 2>/dev/null)

if [ "$SERVICE_STATUS" != "active" ]; then
    exit 1
fi

# --------------------------------
# Check WAV recordings exist
# --------------------------------

LATEST_WAV=$(ls -t *.wav 2>/dev/null | head -n 1)

if [ -z "${LATEST_WAV:-}" ]; then
    exit 2
fi

# --------------------------------
# Check GPS fix
# --------------------------------

if command -v gpspipe >/dev/null 2>&1; then
    GPS_MODE=$(gpspipe -w -n 8 2>/dev/null | grep -m1 '"mode"' | sed 's/.*"mode":\([0-9]*\).*/\1/')

    if [ -z "${GPS_MODE:-}" ] || [ "$GPS_MODE" = "0" ] || [ "$GPS_MODE" = "1" ]; then
        exit 3
    fi
else
    exit 3
fi

# --------------------------------
# Check disk space
# --------------------------------

FREE_GB=$(df -BG "$PROJECT_DIR" | tail -n 1 | awk '{print $4}' | tr -d 'G')

if [ -n "${FREE_GB:-}" ] && [ "$FREE_GB" -lt 20 ]; then
    exit 4
fi

# --------------------------------
# System healthy
# --------------------------------

exit 0
