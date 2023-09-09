<h1 align="center" style=font-size:6em>choose</h1>

[![Tests](https://github.com/jagprog5/choose/actions/workflows/tests.yml/badge.svg)](https://github.com/jagprog5/choose/actions/workflows/tests.yml)
[![Linter](https://github.com/jagprog5/choose/actions/workflows/cpp-linter.yml/badge.svg)](https://github.com/jagprog5/choose/actions/workflows/cpp-linter.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

choose is a tool for performing transformations with regular expressions. It also applies sorting and uniqueness, and creates selection dialogs.

# Why?

Here's a few use cases that other tools have trouble with:

 - [sort csv and truncate output](https://stackoverflow.com/a/77025562/15534181)
 - [sort csv with embedded commas](https://stackoverflow.com/a/77034520/15534181)
 - [stream edit with lookarounds](https://stackoverflow.com/a/77025816/15534181)

Also it's fast. See benchmarks [here](./perf/perf.md) comparing choose to other tools with similar functionality.

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
Dialogs can be used to select between tokens. By default, each token is delimited by a newline character. This command:

```bash
echo $'here❗\nis\neach\noption📋'\
  | choose --tui -p "Pick a word!"
```

Gives this interface:

<pre>
┌───────────────────────┐  
│Pick a word!           │  
└───────────────────────┘  
> here❗  
  is  
  each  
  option📋  
</pre>

# Delimiters

Instead of a newline character, a literal sequence or regular expression can delimit the input.

```bash
echo -n "this 1 is 2 a 3 test"\
  | choose -r " [0-9] "
```

<pre>
this  
is  
a  
test
</pre>  

The delimiter in the output can also be set.

```bash
echo -n "this test here"\
  | choose " " -o $'\n===\n'
```

<pre>
this
===
test
===
here
===
</pre>  

# Ordered Operations

Transformations can be done in a specified order. This command prints every other word by:

1. Suffixing each token with its arrival order
2. Filtering for tokens that end with an even number
3. Substituting to remove the arrival order

```bash
echo -n 'every other word is printed here' | \
  choose -r ' ' --index=after            `# <-- 1` \
                -f '[02468]$'            `# <-- 2` \
                --sub '(.*) [0-9]+' '$1' `# <-- 3`
```

## Lines vs Tokens

There's a difference between a typical shell pipeline like:

```bash
cat some_content | grep "test" | head -n 5
```

Compared to this:

```bash
cat some_content | choose -f "test" --head 5
```

The former is restricted to working with `lines`, whereas the latter works with `tokens`. Tokens are contiguous ranges and can contain newline characters, whereas lines can't.

# Sorting and Uniqueness

choose uses lexicographical, numerical, or general numeric comparison between tokens. Using this comparison, it can apply sorting and uniqueness.

For example, this command sorts the input and leaves only unique entries:

```bash
echo -n "this is is test test "\
  | choose " " -us
```

<pre>
is
test
this
</pre>

And this command sorts based on a specified field:

```bash
echo "1,gamma,1
3,alpha,3
2,beta,2"\
  | choose -s --field '^[^,]*+.\K[^,]*+'
```

<pre>
3,alpha,3
2,beta,2
1,gamma,1
</pre>

Sorting is implemented to effectively leverage truncation. For example:

```bash
cat very_large_file | choose --sort --out=5
```

That command only stores the lowest 5 entries throughout its lifetime; the memory usage remains bounded appropriately, no matter the size of the input. The equivalent: `sort | head -n5` does not do this and will be slower.

## Compared to sort -u

gnu sort implements uniqueness in the following way:

1. Read the input and sort it.
2. Remove consecutive duplicate entries from the sorted elements, like the `uniq` command.

choose instead applies uniqueness upfront:

1. For every element in the input, check if it's been seen before.
2. If it hasn't yet been seen, add it to the output.
3. Sort the output.

A bonus of this implementation is that uniqueness and sorting can use different comparison types. For example, choose can apply uniqueness numerically, but sorting lexicographically. Whereas sort needs to use the same comparison for both.

A drawback is that it can use more memory, since a separate data structure is maintained to determine if new elements are unique. But, there's a degree of control since uniqueness related args are provided. choose can also revert back to the way sort does things by doing `choose -s --uniq`.

# Matching

Rather than specifying how tokens are terminated, the tokens themselves can be matched for. A match and each match group form a token. This is like `grep -o`.

```bash
echo "aaabbbccc"\
  | choose --match "bbb(...)" -r
```

<pre>
bbbccc
ccc
</pre>

# Monitoring

Suppose there's an input that's running for a really long time. For example, a python http server, with an output like this:

```txt
127.0.0.1 - - [08/Sep/2023 22:11:48] "GET /tester.txt HTTP/1.1" 200 -
192.168.1.42 - - [08/Sep/2023 22:11:58] "GET /tester.txt HTTP/1.1" 200 -
...
```

The goal is to monitor the output and print unique IPs:

```bash
# serves current dir on 8080
python3 -m http.server --directory . 8080 2>&1 >/dev/null | choose --match --multiline -r "^(?>(?:\d++\.){3})\d++" --unique-limit 1000 --flush
```

This form of uniqueness keeps the last N unique ips; least recently received ips are forgotten and will appear in the output again. This keeps the memory usage bounded.

# Stream Editing

There's a few different ways that choose can edit a stream. This way is generally the best:

```bash
echo "this is a test" | choose -r --sed "\w+" --replace banana
```

In contrast, this implicitly separates the input into tokens each delimited by a newline. Then, on each token a global substitution is applied:

```bash
echo "this is a test" | choose -r --sub "\w+" banana
```

Lastly, this is a weird hack that leverages the input and output delimiters. It can be faster, but the replacement must be a literal string:

```bash
echo "this is a test" | choose -r "\w+" -o banana -d
```

## Compared to sed

choose uses [PCRE2](https://www.pcre.org/current/doc/html/pcre2syntax.html), which allows for lookarounds + various other regex features, compared to sed which allows only [sed regex](https://www.gnu.org/software/sed/manual/html_node/Regular-Expressions.html). This requires [different logic](https://www.pcre.org/current/doc/html/pcre2partial.html#SEC4) for management of the match buffer, since lookbehind bytes must be properly retained as tokens are created. Meaning sed can't match expressions like this:

```bash
echo "banana test test" | choose -r --sed '(?<!banana )test' --replace hello
```

sed works per line of the input. choose doesn't assume the distinction of lines. To emphasize a point, here is a tricky substitution which has a target that includes a newline and null character:

```bash
echo -e "this\n\0is\na\ntest" | choose -r --sed 'is\n\0is' --replace something
```

sed can't make a substitution if the target contains the delimiter (a newline character); the input is split into lines before substitution occurs, so the delimiter never makes it to the substitution logic. The way this is avoided is to use `sed -z`, which changes the delimiter from newline to null. But in this case, the target includes null too! So it can't process the input properly. One quick way of fixing this is to use `tr` to change the input before it gets to sed (by changing null to a different character), but this can lose information and lead to ambiguous cases (if the delimiter is changed to something found naturally in the input).

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
