[![Tests](https://github.com/jagprog5/choose/actions/workflows/tests.yml/badge.svg)](https://github.com/jagprog5/choose/actions/workflows/tests.yml)
[![Linter](https://github.com/jagprog5/choose/actions/workflows/cpp-linter.yml/badge.svg)](https://github.com/jagprog5/choose/actions/workflows/cpp-linter.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

# choose

choose is a stream manipulation tool for creating selection dialogs.
## Dialogs

Here is an example dialog:

```bash
echo $'here\nis\neach\noption' | choose -p "Pick a word! 📋❗"
```
```
┌─────────────────────────────┐
│Pick a word! 📋❗             │
└─────────────────────────────┘
> here
  is
  each
  option
```
## Character Visibility

Control characters and whitespace lines are printed in a visible way within the UI:

```bash
echo -en '\0\n\tcool text\x2\x3\t   ' | choose $'\t'
```

[comment]: <> (this formatting + colors looks reasonable locally + various github themes)

$\textcolor{lightblue}{\textsf{> }}\textcolor{gray}{\backslash\textsf{0}\backslash\textsf{n}}$  
$\hspace{1em}\textcolor{lightblue}{\textsf{cool text}}\textcolor{gray}{\textsf{STXETX}}$  
$\hspace{1em}\textcolor{gray}{\backslash\textsf{s\\{3 bytes\\}}}$

## Delimiters

An arbitrary delimiter can be chosen to separate the input into tokens:

```bash
echo -n "this 1 is 2 a 3 test" | choose -r " [0-9] "
```
## Ordered Ops

choose can do transformations in a specified order. This prints every other word:

```bash
echo -n 'every other word is printed here' | choose ' ' -r --out --in-index=after -f '[02468]$' --sub '(.*) [0-9]+' '$1'
```

## Ordering and Uniqueness

This command separates each token by "===", filters for unique elements, and sorts them:

```bash
echo -n "this===is===is===test===test===" | choose === -ust -o $'\n=-=\n'
```

Sorting and uniqueness can have user defined comparison. In this case, all tokens that start with "John" are first, but otherwise the order is retained:

```bash
echo -en "John Doe\nApple\nJohn Doe\nBanana\nJohn Smith" | choose -r --comp-z $'^John[ a-zA-Z]*\0(?!John)' --comp-sort
```

## Regex

It supports lookarounds / full pcre2 syntax, and can match against patterns in text:

```bash
echo "aaabbbccc" | choose -r --match "(?<=aaa)bbb(...)" -t
```
## Speed

choose is slower than common tools at narrow tasks.

For a simple grep case, its slower but comparable to [pcre2grep](https://www.pcre.org/current/doc/html/pcre2grep.html), which uses the same regex engine:

```bash
# speed test. download 370000 words
wget https://raw.githubusercontent.com/dwyl/english-words/master/words_alpha.txt
time (cat words_alpha.txt | grep "test" > out.txt)          # 0.005s
time (cat words_alpha.txt | pcre2grep "test" > out.txt)     # 0.019s
time (cat words_alpha.txt | choose -f "test" -t > out.txt)  # 0.055s (~2 to 4 times slower that pcre2grep)
```

For a simple substitution case, its slower than sed:

```bash
time (cat words_alpha.txt | sed "s/test/banana/g" > out.txt)        # 0.055s
time (cat words_alpha.txt | choose --sub test banana -t > out.txt)  # 0.293s (~5 times slower than sed)
```

## Documentation

```bash
choose --help
```
# Install / Update

```bash
sudo apt-get install cmake pkg-config libpcre2-dev libncursesw5-dev
scripts/install.bash
source ~/.bashrc
```
# Uninstall

```bash
scripts/uninstall.bash
```
# hist

`hist` is a bash function installed with `choose`. It allows a previous command to be re-run, like [fzf](https://github.com/junegunn/fzf).

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
