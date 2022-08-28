# choose

Splits an input into tokens based on a regex separator, and provides a text based ui for selecting which token are sent to the output.

![screenshot](./screenshot.png)

## Installation

```bash
sudo apt-get install -y pkg-config libpcre2-dev libncursesw5-dev # 5 or greater
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
    SELECTED=`history | grep -i "\`echo "$@"\`" | sed 's/^ *[0-9]*[ *] //' | head -n -1 | choose -f` && \
    history -s "$SELECTED" && HISTTIMEFORMAT="$HISTTIMEFORMATSAVE" && eval "$SELECTED" ; 
}
```

It allows a previous command to be re-run,  
like a better combination of `reverse-i-search` and `history | grep "$whatever"`.

*<sub><sup>Multiline history entries are not displayed correctly because gnu-history [doesn't](https://askubuntu.com/a/1210371) support this by default and parsing is a pain.</sup></sub>
