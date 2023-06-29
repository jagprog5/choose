<h1 align="center" style=font-size:6em>choose</h1>

[![Tests](https://github.com/jagprog5/choose/actions/workflows/tests.yml/badge.svg)](https://github.com/jagprog5/choose/actions/workflows/tests.yml)
[![Linter](https://github.com/jagprog5/choose/actions/workflows/cpp-linter.yml/badge.svg)](https://github.com/jagprog5/choose/actions/workflows/cpp-linter.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

choose is a tool for creating selection dialogs and doing fancy transformations with regular expressions.
## Install
```bash
sudo apt-get install cmake pkg-config libpcre2-dev libncursesw5-dev
git clone https://github.com/jagprog5/choose.git && cd choose
scripts/install.bash
source ~/.bashrc
```
## Uninstall
```bash
scripts/uninstall.bash
```
## Documentation
```bash
choose --help
```
# Dialogs
Dialogs can be used to select between tokens. By default, each token is delimited by a newline character:
<table>
<tr>
<th>Command</th>
<th>Interface</th>
</tr>
<tr>
<td>

```bash
echo $'here❗\nis\neach\noption📋'\
  | choose --tui -p "Pick a word!"
```

</td>
<td>

<pre>
┌───────────────────────┐  
│Pick a word!           │  
└───────────────────────┘  
> here❗  
  is  
  each  
  option📋  
</pre>

</td>
</tr>
</table>

# Delimiters

Instead of a newline character, a sequence or regular expression can delimit the input.

<table>
<tr>
<th>Command</th>
<th>Output</th>
</tr>
<tr>
<td>

```bash
echo -n "this 1 is 2 a 3 test"\
  | choose -r " [0-9] "
```

</td>
<td>
<pre>
this  
is  
a  
test
</pre>  
</td>
</tr>
</table>

The delimiter in the output can also be set

<table>
<tr>
<th>Command</th>
<th>Output</th>
</tr>
<tr>
<td>

```bash
echo -n "this test here"\
  | choose " " -o $'\n===\n'
```

</td>
<td>
<pre>
this
===
test
===
here
===
</pre>  
</td>
</tr>
</table>

# Ordered Operations

Transformations can be done in a specified order. This command prints every other word by:

1. Suffixing each token with its arrival order
2. Filtering for tokens that end with an even number
3. Substituting to remove the index

```bash
echo -n 'every other word is printed here' | \
  choose -r ' ' --in-index=after\        # 1
                -f '[02468]$'\           # 2
                --sub '(.*) [0-9]+' '$1' # 3
```

# Ordering and Uniqueness

choose allows for lexicographical comparison and **user defined** comparison between tokens. Using this comparison, it can apply ordering and uniqueness.

For example, this command sorts the input and leaves only unique entries:

<table>
<tr>
<th>Command</th>
<th>Output</th>
</tr>
<tr>
<td>

```bash
echo -n "this is is test test "\
  | choose " " -us
```

</td>
<td>
<pre>
is
test
this
</pre>  
</td>
</tr>
</table>

And this command puts all tokens that start with "John" first, but otherwise the order is retained and tokens are unique lexicographically:

<table>
<tr>
<th>Command</th>
<th>Output</th>
</tr>
<tr>
<td>

```bash
echo -en "John Doe\nApple\nJohn Doe\nBanana\nJohn Smith"\
 | choose --comp '^John' --comp-sort -ru
```

</td>
<td>
<pre>
John Doe
John Smith
Apple
Banana
</pre>  
</td>
</tr>
</table>

# Matching

Rather than specifying how tokens are terminated, the tokens themselves can be matched for. A match and each match group form a token.

<table>
<tr>
<th>Command</th>
<th>Output</th>
</tr>
<tr>
<td>

```bash
echo "aaabbbccc"\
  | choose --match "bbb(...)" -r
```

</td>
<td>
<pre>
bbbccc
ccc
</pre>  
</td>
</tr>
</table>

# Speed

For a grep-like task, its the same speed as pcre2grep, which uses the same regex library:

```bash
# speed test. download 370000 words
wget https://raw.githubusercontent.com/dwyl/english-words/master/words_alpha.txt
time (cat words_alpha.txt | grep "test" > /dev/null)      # 0.008s
# pcre2grep 10.42, compiled with -O3 to be the same as choose, linked with same PCRE2
time (cat words_alpha.txt | pcre2grep "test" > /dev/null) # 0.033s
time (cat words_alpha.txt | choose -f "test" > /dev/null) # 0.033s
```

For a substitution task, it's **faster** than sed:

```bash
# using tr to make the file a single line, since sed is line buffered and slow otherwise
# GNU sed 4.9.32, compiled from source with -O3 to be the same as choose
time (cat words_alpha.txt | tr '\n' ' ' | sed "s/test/banana/g" > /dev/null)       # 0.025s
time (cat words_alpha.txt | tr '\n' ' ' | choose test -o banana --sed > /dev/null) # 0.017s
```

For getting unique elements, it's slightly faster than awk:

```bash
# GNU Awk 4.1.4
time (cat words_alpha.txt | awk '!a[$0]++' > /dev/null) # 0.220s
time (cat words_alpha.txt | choose -u > /dev/null)      # 0.193s
```

For sorting and uniqueness, it's **faster** than sort:

```bash
# GNU sort 8.28
time (cat words_alpha.txt | sort -u > /dev/null)    # 0.336s
time (cat words_alpha.txt | choose -su > /dev/null) # 0.254s
```

# hist

`hist` is a bash function installed with `choose`. It allows a previous command to be re-run, like [fzf](https://github.com/junegunn/fzf).

```txt
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
