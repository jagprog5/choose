<h1 align="center" style=font-size:6em>choose</h1>

[![Tests](https://github.com/jagprog5/choose/actions/workflows/tests.yml/badge.svg)](https://github.com/jagprog5/choose/actions/workflows/tests.yml)
[![Linter](https://github.com/jagprog5/choose/actions/workflows/cpp-linter.yml/badge.svg)](https://github.com/jagprog5/choose/actions/workflows/cpp-linter.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

choose is a tool for creating selection dialogs and doing fancy transformations with regular expressions.
## Install
```bash
sudo apt-get install cmake pkg-config libpcre2-dev libncursesw5-dev
git clone https://github.com/jagprog5/choose.git && cd choose
make install
source ~/.bashrc
```
## Uninstall
```bash
make uninstall
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

Instead of a newline character, a literal sequence or regular expression can delimit the input.

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

# Stream Editing

There's a few different ways that choose can edit a stream. This way is generally the best:

```bash
echo "this is a test" | choose -r --sed "\w+" --replace banana
```

In contrast, this implicitly separates the input into tokens each delimited by a newline. Then, on each token a global substitution is applied:

```bash
echo "this is a test" | choose -r --sub "\w+" banana
```

Lastly, this is a weird hack the leverages the input and output delimiters. The replacement must be a literal string:

```bash
echo "this is a test" | choose -r "\w+" -o banana -d
```

# Speed

See benchmarks [here](./perf.md)

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
