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

LINE="[ -f ~/.choose.bash ] && source ~/.choose.bash"

cp scripts/choose.bash ~/.choose.bash && \
    grep -qxF "$LINE" ~/.bashrc || echo "$LINE" >> ~/.bashrc && \
    echo "Done! run source ~/.bashrc for it to take effect."
