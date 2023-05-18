#!/bin/bash
# this script is run by cmake during installation

if [[ ! -f ~/.bashrc ]]; then
  echo bashrc not found. skipping.
  exit
fi

if [ "$EUID" -ne 0 ]; then
  echo "Please run installer as root"
  exit
fi

while true; do
  read -p "Install hist (y/n)? " -n 1 -r
  echo
  if [[ $REPLY =~ ^[YyNn]$ ]]; then
    if [[ $REPLY =~ ^[Yy]$ ]]; then
      break
    else
      echo "Skipping hist installation"
      exit
    fi
  else
    echo "Invalid input. Please enter y or n."
  fi
done

# https://stackoverflow.com/a/7359006/15534181
USER_HOME=$(getent passwd "$SUDO_USER" | cut -d: -f6)
LINE="[ -f ~/.choose.bash ] && source ~/.choose.bash"
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)

cp -- "$SCRIPT_DIR/choose.bash" "$USER_HOME/.choose.bash" || exit 1
grep -qxF "$LINE" -- "$USER_HOME/.bashrc" || echo "$LINE" >>"$USER_HOME/.bashrc" || exit 1
echo "Done! run source ~/.bashrc for it to take effect."
