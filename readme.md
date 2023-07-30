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

## Lines vs Tokens

There's a difference between a typical shell pipeline like:

```bash
cat some_content | grep "test" | head -n 5
```

Compared to this:

```bash
cat some_content | choose -f "test" --out 5
```

The former is restricted to working with `lines`, whereas the latter works with `tokens`. Tokens are arbitrary and can contain newline characters, whereas lines can't.

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
 | choose --comp-sort '^John' -ru
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

Rather than specifying how tokens are terminated, the tokens themselves can be matched for. A match and each match group form a token. This is like `grep -o`.

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

Lastly, this is a weird hack that leverages the input and output delimiters. The replacement must be a literal string:

```bash
echo "this is a test" | choose -r "\w+" -o banana -d
```

## Compared to sed

choose uses [PCRE2](https://www.pcre.org/current/doc/html/pcre2syntax.html), which allows for lookarounds + various other regex features, compared to sed which only allows for [basic expressions](https://www.gnu.org/software/sed/manual/html_node/Regular-Expressions.html). This requires a different implementation as the matched buffer must be manage to properly retain lookbehind bytes as tokens are created. An expression like this can't be done in sed:

```bash
echo "banana test test" | choose -r --sed '(?<!banana )test' --replace hello
```

Additionally, sed works per line of the input. choose doesn't make this distinction. For example, here's a substitution which has a target that includes a newline and null character:

```bash
echo -e "this\n\0is\na\ntest" | choose -r --sed 'is\n\0is' --replace something
```

sed can't make a substitution where the target contains the delimiter, since the input is split into lines based on a delimiter before substitution occurs. The way this is avoided is to use `sed -z`, which changes the delimiter from newline to null. But in this case, the target includes null too! So it can't process the input properly.

# Speed

See benchmarks [here](./perf.md) comparing choose to other tools with similar functionality.

# ch_hist

`ch_hist` is a bash function installed with choose. It allows a previous command to be re-run, like [fzf](https://github.com/junegunn/fzf).

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
ch_hist git
ch_hist hello there
```
