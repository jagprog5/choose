# choose

Splits an input into tokens based on a regex separator, and provides a text based ui for selecting which token are sent to the output.

![screenshot](./screenshot.png)

## Installation

```bash
sudo apt-get install -y libncurses-dev
cmake . && sudo cmake --build . --target install 
```

## Docs

```bash
choose --help
```

# hist

`hist` is a shell function which uses `choose`. It's defined in choose's doc, and copy pasted here for convenience:

```bash
hist() { SELECTED=`history | grep "\`echo "$@"\`" | sed 's/^\s*[0-9*]*\s*//' | head -n -1 | tac \
    | choose` && history -s "$SELECTED" && eval "$SELECTED" ; }
```


`hist` should be added to `~/.bashrc`. It allows a previous command to be re-run, a better combination of `reverse-i-search` and `history | grep "$whatever"`.