#!/bin/sh
set -e
SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
cd -- "$SCRIPT_DIR/../build"
cmake ..
cmake --build .

if [ -f "$HOME/.bashrc" ]; then
  # places output exe in ~/.choose/bin
  cmake --install . --prefix "$HOME/.choose"
  # places choose bash script in ~/.choose
  # that script adds ~/.choose/bin to the path and adds the function ch_hist
  cp -v -- "$SCRIPT_DIR/choose.bash" "$HOME/.choose/"
  # add a line to bashrc which sources choose bash script so it works
  LINE='[ -f ~/.choose/choose.bash ] && source ~/.choose/choose.bash'
  if grep -qxF "$LINE" "$HOME/.bashrc"; then
    echo Already exists in bashrc:
    echo "    '$LINE'"
  else
    echo Adding line to bashrc:
    echo "    '$LINE'"
    echo "$LINE" >> "$HOME/.bashrc"
  fi
  echo "================================================================================"
  echo "Done! It may be necessary to reload bashrc. e.g. 'source $HOME/.bashrc'"
else
  # places output exe into usr local bin and doesn't do anything else
  sudo cmake --install .
  echo "================================================================================"
  echo "since $HOME/.bashrc wasn't found, couldn't:"
  echo "\t - add arg auto-completion"
  echo "\t - install ch_hist function in shell"
fi
