# Comparison to fzf

[fzf](https://github.com/junegunn/fzf) is a "command-line fuzzy finder". It's similar to choose, but different.

 - In fzf, you [can't](https://github.com/junegunn/fzf/issues/1670) change the input separator to anything other than a newline or null. choose is more flexible, and lets you specify whatever separator you want, including regular expressions.
 - In fzf, the [only](https://github.com/junegunn/fzf/issues/1417) output order is the order in which the user selected the tokens. In choose, you can have the tokens be outputted in the same order they arrived in.
 - choose doesn't have a search bar for additional filtering within the UI (this is fzf's core feature).
 - choose allows output to be flushed while still selecting items, via the batch functionality (see `-t` in the doc).
 - fzf doesn't apply word wrapping on prompts (via `--header-lines` or `--prompt`). The text is just cut off instead.
