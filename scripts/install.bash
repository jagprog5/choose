#!/bin/bash
set -e

if [[ ! -f ~/.bashrc ]]; then
  echo Bashrc not found!
  exit 1
fi

LINE='[ -f ~/.choose/choose.bash ] && source ~/.choose/choose.bash'
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)

if [ ! -d ~/.choose ] ; then
  (cd -- "$SCRIPT_DIR/../build" && cmake .. && cmake --build . --target install)
fi

cp -- "$SCRIPT_DIR/choose.bash" ~/.choose/choose.bash || exit 1
grep -qxF "$LINE" ~/.bashrc || echo "$LINE" >>~/.bashrc || exit 1
echo -e "/-------------\\ \n|Reload bashrc|\n\\-------------/"
