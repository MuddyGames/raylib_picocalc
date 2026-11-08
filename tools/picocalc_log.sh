#!/usr/bin/env bash
#
# picocalc_log.sh : Shows the game's TraceLog() lines as it runs on the PicoCalc.
#
#     tools/picocalc_log.sh                 (finds the Pico by itself)
#     tools/picocalc_log.sh /dev/ttyACM1    (or say which)
#
# Plug the Pico's micro-USB into the computer (the USB-C is just power). It
# waits for the Pico to turn up, prints what it says, and when the Pico goes
# away (you flash it again, press its reset, unplug it) waits for it to come
# back. Ctrl+C to stop.

port="$1"

find_port() {
	if [ -n "$1" ]; then
		[ -e "$1" ] && echo "$1"
	else
		ls /dev/ttyACM* 2>/dev/null | head -n 1
	fi
}

waiting=""
while true; do
	dev=$(find_port "$port")
	if [ -z "$dev" ]; then
		if [ -z "$waiting" ]; then
			echo "-- Waiting for the Pico (micro-USB plugged into this computer, game running)..."
			waiting=yes
		fi
		sleep 0.5
		continue
	fi
	waiting=""

	if [ ! -r "$dev" ]; then
		echo "-- No permission to read $dev. Once, then log out and back in:"
		echo "     Debian, Ubuntu, Pop!_OS:  sudo usermod -aG dialout \$USER"
		echo "     Arch, Manjaro, EndeavourOS: sudo usermod -aG uucp \$USER"
		echo "   (or for now:  sudo $0 $dev)"
		exit 1
	fi

	echo "-- Connected to $dev (Ctrl+C to stop)"
	stty -F "$dev" 115200 raw -echo 2>/dev/null
	cat "$dev"
	echo "-- $dev went away: waiting for it to come back"
	sleep 0.5
done
