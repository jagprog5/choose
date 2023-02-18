#!/bin/bash
USER_HOME=$(getent passwd $SUDO_USER | cut -d: -f6)
rm /usr/bin/choose /usr/bin/choose_injector /usr/bin/choose_injector_wrapper.bash "$USER_HOME/.choose.bash"
sed -i '/[ -f ~\/.choose.bash ] && source ~\/.choose.bash/d' "$USER_HOME/.bashrc"
