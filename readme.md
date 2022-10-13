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
LINE="$(unset HISTTIMEFORMAT && history | sed 's/^ *[0-9]*[ *] //' |\
grep -i "$*" | head -n-1 | tac | cat -n | sort -uk2 | sort -nk1 | \
cut -f2- | choose -p "Select a line to run.")"
# save selection to history and run it
[ ! -z "$LINE" ] && history -s "$LINE" && eval "$LINE" ;
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

# Comparison to fzf

[fzf](https://github.com/junegunn/fzf) is a "command-line fuzzy finder". It is a mature project that does a lot of what choose can do. It has better integration with terminal "reverse-i-search". That being said, here are some differences:

## Flexibility

In fzf, you [can't](https://github.com/junegunn/fzf/issues/1670) change the input separator to anything other than a newline or null. choose is more flexible, and lets you specify whatever separator you want, including regular expressions.

In fzf, the [only](https://github.com/junegunn/fzf/issues/1417) output order is the order in which the user selected the tokens. In choose, you can have the tokens be outputted in the same order they arrived in.

## Simplicity

choose is very simple and light weight, ~280K compared to fzf's 2.4M (and choose could definitely be made smaller). For a fair comparison, this is with everything statically linked. fzf is like this out of the box, while choose needs to be compiled like so:

```bash
clang++ -DPCRE2_STATIC -Os -DNDEBUG -I/usr/include ../choose.cpp /usr/lib/x86_64-linux-gnu/libncursesw.a /usr/lib/x86_64-linux-gnu/libform.a /usr/lib/x86_64-linux-gnu/libtinfo.a /usr/lib/x86_64-linux-gnu/libgpm.a -lpcre2-8
```

If you *really* wanted to, choose could run on an embedded system and provide nice selection dialogs via a terminal over serial connection. Not that it would be practical to do so, but it would be fun.

## Search Bar

choose doesn't have a search bar for additional filtering within the UI, whereas this is fzf's core feature. In the `hist` case above, instead you would cancel (with ctrl+c) and re-run the command with a different search argument. A search bar doesn't fit choose's niche.

## Selection Dialogs

choose is more suitable for creating selection dialogs. It can give a title which will word wrap appropriately if the width is too small. The only equivalent in fzf is `--header-lines` or `--prompt`, neither of which does word wrapping; the text is just cut off instead.
