#!/bin/bash

PATH="$PATH:$HOME/.choose/bin"

__choose_script_completion() {
    local curr_arg
    curr_arg=${COMP_WORDS[COMP_CWORD]}
    completions=$(choose --auto-completion-strings)
    COMPREPLY=($(compgen -W "${completions[*]}" -- "$curr_arg"))
}

complete -F __choose_script_completion choose

ch_hist() {
  if [ -z "$VISUAL" ] && [ -z "$EDITOR" ]; then
    echo "Error. Please set the VISUAL or EDITOR variable appropriately."
    return 1
  fi

  # get the history lines, do various cleanup on the stream:
  #   - remove the leading \n\t
  #   - remove the trailing \n
  #   - sub \n\t separator between lines to null
  #   - remove the latest entry of the history
  # pipe to grep for the arg, then display in choose
  local LINE=$(builtin fc -lnr -2147483648 |
    tail -c +3 | head -c -1 | sed -z 's/\n\t[ *]/\x0/g' |
    {
      IFS= read -r -d ''
      cat
    } | grep -zi -- "$*" |
    choose -r "\x00" -ue --delimit-not-at-end --flip -p "Select a line to edit then run.")
  
  if [ -z "$LINE" ]; then
    echo "empty line"
    return 0
  fi

  local TEMPFILE=$(mktemp)
  trap 'rm -f -- "$TEMPFILE"' RETURN EXIT HUP INT QUIT PIPE TERM

  printf "%s\n" "$LINE" >"$TEMPFILE"

  "${VISUAL:-${EDITOR}}" -- "$TEMPFILE"

  if [ $? -ne 0 ]; then
    echo "editor exited with non-zero code."
    return 1
  fi

  history -s -- $(cat "$TEMPFILE")

  # run on current shell
  source -- "$TEMPFILE"
}

ch_branch() {
  local branch="$(git branch | grep -i -- "$*" | choose -re --tui-select '^\*' --sub '^[ *] ' '' --sort-reverse --delimit-not-at-end -p 'git switch')"
  if [ -z "$branch" ]; then
    git switch "$branch"
  fi
}
