# choose

Do you hate pressing [ctrl + r](https://codeburst.io/use-reverse-i-search-to-quickly-navigate-through-your-history-917f4d7ffd37) over and over again to re-run a command? Do you resort to grepping through your bash history then copy-pasting the line you want? Disgusting! So many key presses! So much visual strain!

If so, then you've come to the right place.

choose is a ncurses based token selector. It gives a nice terminal user interface for selecting tokens. Selecting a line from the bash history is only one of its use cases.

![screenshot](./screenshot.png)

## Install

```bash
sudo apt-get install pkg-config libpcre2-dev libncursesw5-dev # 5 or greater
cd build && cmake .. && sudo cmake --build . --target install 
```

## Uninstall

```bash
sudo scripts/uninstall.bash
```

## Docs

```bash
choose --help
```

# hist

`hist` is a bash function which uses `choose`. It is optionally installed in `~/.bashrc`

It allows a previous command to be re-run,  
like a better combination of `reverse-i-search` and `history | grep "$whatever"`.

```bash
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│Select a line to run.                                                                            │
└─────────────────────────────────────────────────────────────────────────────────────────────────┘
> choose -h
  sudo make install
  cmake ..
  rm -rf *
  cd build
  git pull
  cd choose/
  ls
  cd ~/
  clear
  git push
  git commit --amend
  echo -n "this 1 is 2 a 3 test" | choose -r " [0-9] "
  echo -n "a b c" | choose -o "," -b $'\n' " " -dmst > temp.txt
  cat temp.txt
  top
  git log --oneline
```

## Examples

```bash
hist git
hist hello there
```

Comparison to [fzf](./fzf.md).
