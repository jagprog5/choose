[![Tests](https://github.com/jagprog5/choose/actions/workflows/tests.yml/badge.svg)](https://github.com/jagprog5/choose/actions/workflows/tests.yml)
[![Linter](https://github.com/jagprog5/choose/actions/workflows/cpp-linter.yml/badge.svg)](https://github.com/jagprog5/choose/actions/workflows/cpp-linter.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

# choose

choose is a grep-like utility for creating selection dialogs.

## Speed

Its speed is slower but comparable to [pcre2grep](https://www.pcre.org/current/doc/html/pcre2grep.html), which uses the same regex engine but has different functionality:

```bash
# speed test. download 370000 words
wget https://raw.githubusercontent.com/dwyl/english-words/master/words_alpha.txt
time (cat words_alpha.txt | grep "test" > out.txt)          # 0.005s
time (cat words_alpha.txt | pcre2grep "test" > out.txt)     # 0.019s
time (cat words_alpha.txt | choose -f "test" -t > out.txt)  # 0.055s
```

## Dialogs

choose can create selection dialogs (when `--out` and `-t` aren't specified):

```bash
echo $'here\nis\neach\noption' | choose -p "Pick a word! ☺"
```
```
┌─────────────────────────────┐
│Pick a word! ☺               │
└─────────────────────────────┘
> here
  is
  each
  option
```

## Ordered Ops

choose can do transformations in a specified order. This prints every other word:

```bash
echo -n 'every other word is printed here' | choose ' ' -r --out --in-index=after -f '[02468]$' --sub '(.*) [0-9]+' '$1'
```

## Character Visibility

Control characters and whitespaces are printed in a visible way within the UI:

```bash
echo -en '\0\n\tcool text\x2\x3\t   ' | choose $'\t'
```

[comment]: <> (this formatting + colors looks reasonable locally + various github themes)

$\textcolor{lightblue}{\textsf{> }}\textcolor{gray}{\backslash\textsf{0}\backslash\textsf{n}}$  
$\hspace{1em}\textcolor{lightblue}{\textsf{cool text}}\textcolor{gray}{\textsf{STXETX}}$  
$\hspace{1em}\textcolor{gray}{\backslash\textsf{s\\{3 bytes\\}}}$

## Ordering and Uniqueness

This command separates each token by "aaa", and filters for unique elements, and sorts them:

```bash
echo -n "thisaaaisaaaisaaatestaaatestaaa" | choose aaa -ust
```

## Regex

It supports lookarounds / full pcre2 syntax, and can match against patterns in text:

```bash
echo "aaabbbccc" | choose -r --match "(?<=aaa)bbb(...)" -t
```

## Documentation

```bash
choose --help
```

# Install

```bash
sudo apt-get install pkg-config libpcre2-dev libncursesw5-dev # 5 or greater
cd build && cmake .. && sudo cmake --build . --target install # optionally -DBUILD_TESTING=true
```

# Uninstall

```bash
sudo scripts/uninstall.bash
```

# hist

`hist` is a bash function which uses `choose`. During installation, there is an
optional prompt to install it in `~/.bashrc`

It allows a previous command to be re-run, like a better combination of `reverse-i-search` and `history | grep "$whatever"`.

```bash
  git log --oneline
  top
  cat temp.txt
  git commit --amend
  git push
  clear
  cd ~/
  ls
  cd choose/
  git pull
  cd build
  rm -rf *
  cmake ..
  sudo make install
> choose -h
┌────────────────────────────────────────────────────────────────────────────────┐
│Select a line to edit then run.                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## Examples

```bash
hist git
hist hello there
```
