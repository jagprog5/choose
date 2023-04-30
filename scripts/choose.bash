#!/bin/bash
hist() {
  local LINE=$(
    builtin fc -lnr -2147483648 |
    tail -c +3 | head -c -1 | sed -z 's/\n\t[ *]/\x0/g' |
    { IFS= read -r -d ''; cat; } | grep -zi "$*" |
    choose -0ue --no-delimit --flip -p "Select an entry for input to the tty.")
  printf '%s' "$LINE" | choose_injector_wrapper.bash
}
