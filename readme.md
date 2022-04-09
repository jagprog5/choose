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

`hist` is a shell function which uses `choose`. It's defined in choose's doc, and should be pasted into `~/.bashrc` or equivalent. It allows a previous command to be re-run, a better combination of `reverse-i-search` and `history | grep "$whatever"`.