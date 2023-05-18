#!/bin/bash

if [ "$EUID" -ne 0 ]; then
  echo "Please run as root"
  exit
fi

# https://stackoverflow.com/a/7359006
USER_HOME=$(getent passwd $SUDO_USER | cut -d: -f6)
rm /usr/bin/choose -- "$USER_HOME/.choose.bash"
sed -i '/[ -f ~\/.choose.bash ] && source ~\/.choose.bash/d' -- "$USER_HOME/.bashrc"
