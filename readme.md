# choose

Do you hate pressing [ctrl + r](https://codeburst.io/use-reverse-i-search-to-quickly-navigate-through-your-history-917f4d7ffd37) over and over again to re-run a command? Do you resort to grepping through your bash history then copy-pasting the line you want? Disgusting! So many key presses! So much visual strain!

If so, then you've come to the right place.

choose is a ncurses based token selector. It gives a nice terminal user interface for selecting tokens. Selecting a line from the bash history is only one of its use cases.

![screenshot](./screenshot.png)

## Installation

```bash
sudo apt-get install pkg-config libpcre2-dev libncursesw5-dev # 5 or greater
cmake . && sudo cmake --build . --target install 
```

## Docs

```bash
choose --help
```

# hist

`hist` is a bash function which uses `choose`. It should be added to `~/.bashrc`

```bash
hist() { # copy paste this into ~/.bashrc
local LINE
# parse history lines, grep, and filter for latest unique entries
LINE="$(unset HISTTIMEFORMAT && builtin history | sed 's/^ *[0-9]*[ *] //' |\
grep -i -- "$*" | head -n-1 | tac | cat -n | sort -uk2 | sort -nk1 |\
cut -f2- | choose -p "Select a line to run.")"
# give prompt, save selection to history, and run it
read -e -p "> " -i "$LINE" && builtin history -s "$REPLY" && eval "$REPLY" ;
}
```

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

*<sub><sup>Multiline history entries are not displayed correctly because gnu-history [doesn't](https://askubuntu.com/a/1210371) support this by default and parsing is a pain.</sup></sub>

Comparison to [fzf](./fzf.md).