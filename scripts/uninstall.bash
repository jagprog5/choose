#!/bin/bash
set -e
rm -rfv ~/.choose

BEFORE=$(wc -l ~/.bashrc | cut -d' ' -f1)
sed -i '/[ -f ~\/.choose\/choose.bash ] && source ~\/.choose\/choose.bash/d' ~/.bashrc
AFTER=$(wc -l ~/.bashrc | cut -d' ' -f1)

DIFF=$(($BEFORE - $AFTER))
if [ "$DIFF" != "0" ]; then
    echo "$DIFF" line removed from ~/.bashrc
fi

unset -f ch_hist
