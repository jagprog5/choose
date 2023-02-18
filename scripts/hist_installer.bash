#!/bin/bash
# this script is run by cmake during installation

if [[ ! -f ~/.bashrc ]] ; then
    echo bashrc not found. skipping.
    exit
fi

read -p "Install hist (y/n)? " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]
then
    echo skipping
    exit
fi

USER_HOME=$(getent passwd $SUDO_USER | cut -d: -f6)
LINE="[ -f ~/.choose.bash ] && source ~/.choose.bash"
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

cp "$SCRIPT_DIR/choose.bash" "$USER_HOME/.choose.bash" || exit 1
grep -qxF "$LINE" "$USER_HOME/.bashrc" || echo "$LINE" >> "$USER_HOME/.bashrc" || exit 1
echo "Done! run source ~/.bashrc for it to take effect."
