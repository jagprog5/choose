#!/bin/bash
set -e
rm -rf ~/.choose
sed -i '/[ -f ~\/.choose\/choose.bash ] && source ~\/.choose\/choose.bash/d' ~/.bashrc
echo ok
