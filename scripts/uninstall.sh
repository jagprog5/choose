#!/bin/sh

if [ -f "$HOME/.bashrc" ]; then
    rm -rfv -- "$HOME/.choose"
    unset -f ch_hist
    BEFORE=$(wc -l -- "$HOME/.bashrc" | cut -d' ' -f1)
    sed -i '/[ -f ~\/.choose\/choose.bash ] && source ~\/.choose\/choose.bash/d' -- "$HOME/.bashrc"
    AFTER=$(wc -l "$HOME/.bashrc" | cut -d' ' -f1)

    DIFF=$(($BEFORE - $AFTER))
    if [ "$DIFF" != "0" ]; then
        echo "$DIFF line removed from $HOME/.bashrc: '[ -f ~/.choose/choose.bash ] && source ~/.choose/choose.bash'"
    fi
else
    SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
    cd -- "$SCRIPT_DIR/../build"
    sudo cmake --build . --target remove_exe
fi
