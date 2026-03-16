#!/bin/bash

BUTTON=5
GREEN=6
RED=13

HEALTH_SCRIPT="/home/tiuser/dh-logger/health_check.sh"

raspi-gpio set $BUTTON ip pu
raspi-gpio set $GREEN op dl
raspi-gpio set $RED op dl

green_on()  { raspi-gpio set $GREEN dh; }
green_off() { raspi-gpio set $GREEN dl; }
red_on()    { raspi-gpio set $RED dh; }
red_off()   { raspi-gpio set $RED dl; }

all_off() {
    green_off
    red_off
}

show_green_ok() {
    all_off
    green_on
    sleep 3
    green_off
}

blink_red_code() {
    code=$1
    all_off

    for cycle in 1 2 3
    do
        i=1
        while [ $i -le $code ]
        do
            red_on
            sleep 0.3
            red_off
            sleep 0.3
            i=$((i + 1))
        done
        sleep 1
    done
}

button_pressed() {
    [ "$(gpioget gpiochip0 $BUTTON)" = "0" ]
}

while true
do
    if button_pressed; then
        held=0

        while button_pressed; do
            sleep 0.1
            held=$((held + 1))

            if [ $held -ge 15 ]; then
                "$HEALTH_SCRIPT"
                status=$?

                if [ $status -eq 0 ]; then
                    show_green_ok
                else
                    blink_red_code "$status"
                fi

		sleep 1

                while button_pressed; do
                    sleep 0.1
                done
                break
            fi
        done
    fi

    sleep 0.05
done
