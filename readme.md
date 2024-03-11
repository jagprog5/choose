<h1 align="center" style=font-size:6em>choose</h1>

[![Tests](https://github.com/jagprog5/choose/actions/workflows/tests.yml/badge.svg)](https://github.com/jagprog5/choose/actions/workflows/tests.yml)
[![Linter](https://github.com/jagprog5/choose/actions/workflows/cpp-linter.yml/badge.svg)](https://github.com/jagprog5/choose/actions/workflows/cpp-linter.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

choose is a tool for creating selection dialogs. It can also:

 - grep using pcre2 syntax with arbitrary delimiters
 - [stream edit](https://stackoverflow.com/a/77025816/15534181)
 - apply [uniqueness](https://stackoverflow.com/a/77034520/15534181) and (limited) [sorting](https://stackoverflow.com/a/77025562/15534181)

See benchmarks [here](./perf/perf.md) comparing it to other tools.

## Install
```bash
sudo apt-get install cmake pkg-config libpcre2-dev libncursesw5-dev libtbb-dev
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

1. Suffixing each token with its arrival order.
2. Filtering for tokens that end with an even number.
3. Substituting to remove the arrival order.

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

The former is restricted to working with `lines`, whereas the latter works with `tokens`. Tokens are contiguous ranges and can contain newline characters, whereas lines can't. choose is line oriented by default, but doesn't have to be.

# Sorting and Uniqueness

**PLEASE** read this entire section before sorting large files. When in doubt, just pipe to gnu sort.

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

That command only stores the lowest 5 entries throughout its lifetime; the memory usage remains bounded appropriately, no matter the size of the input. The equivalent: `sort | head -n5` does not do this and will be slower for large inputs.

## Compared to gnu sort

### Uniqueness

gnu sort implements uniqueness in the following way:

1. Read the input and sort it.
2. Remove consecutive duplicate entries from the sorted elements, like the `uniq` command.

choose instead applies uniqueness upfront:

1. For every element in the input, check if it's been seen before.
2. If it hasn't yet been seen, add it to the output.
3. Sort the output.

This allows choose to be fast for inputs that have many duplicates.

### Sorting

gnu sort does an [external sort](https://en.wikipedia.org/wiki/External_sorting); it writes sorted chunks to temporary files which are merged later. This makes it great at sorting large files, where the size is comparable to available memory.

In contrast, choose reads the input into memory before sorting. This means it's not suited for sorting large files, unless the memory usage is bounded due to truncation (`--out/--tail`, see `--is-bounded`), or if the input is expected to contain many duplicates (upfront uniqueness, via `-u`). An external sort would not allow for either of those features.

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

sed works per line of the input. choose doesn't assume the distinction of lines. suppose there is a file that consists of the newline character repeatedly. sed will apply the entire throughput of its logic on each empty line even though its not necessary to do so when apply the subsitution. choose is faster in these cases.

To emphasize a point, here is a tricky substitution which has a target that includes a newline and null character:

```bash
echo -e "this\n\0is\na\ntest" | choose -r --sed 'is\n\0is' --replace something
```

sed can't make a substitution if the target contains the delimiter (a newline character); the input is split into lines before substitution occurs, so the delimiter never makes it to the substitution logic. The way this is avoided is to use `sed -z`, which changes the delimiter from newline to null. But in this case, the target includes null too! So it can't process the input properly. One quick way of fixing this is to use `tr` to change the input before it gets to sed (by changing null to a different character), but this can lose information and lead to ambiguous cases (if the delimiter is changed to something found naturally in the input).

# ch_hist

`ch_hist` is a bash function installed with choose (only if bash is the shell being used). It allows a previous command to be re-run, like [fzf](https://github.com/junegunn/fzf).

```txt
  cd build
  rm -rf *
  cmake ..
  make install
> choose -h
┌────────────────────────────────────────────────────────────────────────────────┐
│Select a line to edit then run.                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```
## Examples

```bash
ch_hist
ch_hist git
ch_hist hello there
```
