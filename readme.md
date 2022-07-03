# choose

Splits an input into tokens based on a regex separator, and provides a text based ui for selecting which token are sent to the output.

![screenshot](./screenshot.png)

## Installation

```bash
sudo apt-get install -y libncurses-dev libpcre2-dev
cmake . && sudo cmake --build . --target install 
```

## Docs

```bash
choose --help
```

# hist

`hist` is a bash function which uses `choose`. It should be added to `~/.bashrc`

```bash
hist() {
    HISTTIMEFORMATSAVE="$HISTTIMEFORMAT"
    trap 'HISTTIMEFORMAT="$HISTTIMEFORMATSAVE"' err
    unset HISTTIMEFORMAT
    SELECTED=`history | grep -i "\`echo "$@"\`" | sed 's/^ *[0-9]*[ *] //' | head -n -1 | choose -fr` && \
    history -s "$SELECTED" && HISTTIMEFORMAT="$HISTTIMEFORMATSAVE" && eval "$SELECTED" ; 
}
```

It allows a previous command to be re-run,  
like a better combination of `reverse-i-search` and `history | grep "$whatever"`.
