#!/bin/bash
set -e
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
echo "============================Building and Installing============================="
cd -- "$SCRIPT_DIR/../build" && cmake .. && cmake --build . --target install
echo "=================================Adding Script=================================="
cp -v -- "$SCRIPT_DIR/choose.bash" ~/.choose/

if [[ ! -f ~/.bashrc ]]; then
  echo ~/.bashrc not found!
  echo choose has not been added to the path and hist is not installed
  exit 1
fi

echo "================================Amending bashrc================================="
LINE='[ -f ~/.choose/choose.bash ] && source ~/.choose/choose.bash'
if grep -qxF "$LINE" ~/.bashrc; then
  echo Already exists in bashrc:
  echo "    '$LINE'"
else
  echo Adding line to bashrc:
  echo "    '$LINE'"
  echo "$LINE" >>~/.bashrc
fi
echo "================================================================================"
echo Done!
echo Please reload bashrc.
