
[![Tests](https://github.com/jagprog5/choose/actions/workflows/tests.yml/badge.svg)](https://github.com/jagprog5/choose/actions/workflows/tests.yml)
[![Linter](https://github.com/jagprog5/choose/actions/workflows/cpp-linter.yml/badge.svg)](https://github.com/jagprog5/choose/actions/workflows/cpp-linter.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

# choose

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
  | choose -p "Pick a word!"
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

Instead of a newline character, a sequence or regular expression can delimit the input. Additionally, the interface can be skipped by specifying `-t` or `--out`.

<table>
<tr>
<th>Command</th>
<th>Output</th>
</tr>
<tr>
<td>

```bash
echo -n "this 1 is 2 a 3 test"\
  | choose -r " [0-9] " -t
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
  | choose " " -o $'\n=-=\n' -t
```

</td>
<td>
<pre>
this
=-=
test
=-=
here
=-=
</pre>  
</td>
</tr>
</table>

# Ordered Operations

Transformations can be done in a specified order. This command prints every other word by:

1. Suffixing each token with its arrival order.
2. Filtering for tokens that end with an even number
3. Substituting to remove the index

```bash
echo -n 'every other word is printed here' | choose ' ' -r -t\
        --in-index=after   -f '[02468]$'   --sub '(.*) [0-9]+' '$1'
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
  | choose " " -ust
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

And this command sorts such that tokens that start with "John" are first, but otherwise the order is retained and tokens are unique lexicographically:

<table>
<tr>
<th>Command</th>
<th>Output</th>
</tr>
<tr>
<td>

```bash
echo -en "John Doe\nApple\nJohn Doe\nBanana\nJohn Smith"\
 | choose --comp-z '^John[^\0]*\0(?!John)' --comp-sort -rut
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
  | choose --match "bbb(...)" -rt
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

For a simple grep case, its slower but comparable to [pcre2grep](https://www.pcre.org/current/doc/html/pcre2grep.html), which uses the same regex library:

```bash
# speed test. download 370000 words
wget https://raw.githubusercontent.com/dwyl/english-words/master/words_alpha.txt
time (cat words_alpha.txt | grep "test" > out.txt)         # 0.008s
time (cat words_alpha.txt | pcre2grep "test" > out.txt)    # 0.044s
time (cat words_alpha.txt | choose -f "test" -t > out.txt) # 0.065s (50% slower than pcre2grep)
```

For a simple substitution case, it can be **faster** than sed:

```bash
# making the file a single line so its easier for sed
# since sed is line buffered and the file contains many small lines
time (cat words_alpha.txt | tr '\n' ' ' | sed "s/test/banana/g" > out.txt)                 # 0.028s
time (cat words_alpha.txt | tr '\n' ' ' | choose test -o banana -t --no-delimit > out.txt) # 0.021s
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
