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
Dialogs can be used to select between options. By default, each option is delimited by a newline character:
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

┌───────────────────────┐  
&#8198;│Pick a word!&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;│  
└───────────────────────┘  
\> here❗  
&emsp;is  
&emsp;each  
&emsp;option📋  

</td>
</tr>
</table>

There are different modifiers on the interface (like `--prompt`), such as:

| | |
|-|-|
|`--end`|Places the prompt and cursor at the end|
|`--multi`|Allow multiple options to be selected|
|`--selection-order`|Retains and displays the order as tokens are selected|
|`--tenacious`|Allows groups of selections|

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

1. Suffixing each token with its arrival order
2. Filtering for tokens that end with an even number
3. Substituting to remove the index

```bash
echo -n 'every other word is printed here' | choose ' ' -r -t\
        --in-index=after   -f '[02468]$'   --sub '(.*) [0-9]+' '$1'
```

# Ordering and Uniqueness

choose allows for lexicographical comparison and **user defined** comparison between tokens. Using this comparison, it can apply ordering and uniqueness.

For example, this command sorts the inputs and leaves only unique entires:

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

Rather than specifying how tokens are terminated, the tokens themselves can be matched for. A match and each match group forms a token.

<table>
<tr>
<th>Command</th>
<th>Output</th>
</tr>
<tr>
<td>

```bash
echo "aaabbbccc"\
  | choose --match "(?<=aaa)bbb(...)" -rt
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

choose is slower than common tools at narrow tasks.

For a simple grep case, its slower but comparable to [pcre2grep](https://www.pcre.org/current/doc/html/pcre2grep.html), which uses the same regex library:

```bash
# speed test. download 370000 words
wget https://raw.githubusercontent.com/dwyl/english-words/master/words_alpha.txt
time (cat words_alpha.txt | grep "test" > out.txt)          # 0.008s
time (cat words_alpha.txt | pcre2grep "test" > out.txt)     # 0.044s
time (cat words_alpha.txt | choose -f "test" -t > out.txt)  # 0.065s (50% slower than pcre2grep)
```

For a simple substitution case, its slower than sed:

```bash
time (cat words_alpha.txt | sed "s/test/banana/g" > out.txt)        # 0.058s
time (cat words_alpha.txt | choose --sub test banana -t > out.txt)  # 0.206s (~4 times slower than sed)
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
