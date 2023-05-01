#!/bin/bash
# modified from https://gist.github.com/johnlane/6bdeac9dd5a7b0c60820
# this script is installed in /usr/bin and is used by choose.bash

saved_settings=$(stty -F /dev/tty -g)
stty -F /dev/tty -echo -icanon min 1 time 0
choose_injector </dev/stdin
until read -t0; do
  sleep 0.02
done
stty -F /dev/tty "$saved_settings"
