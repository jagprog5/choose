#!/bin/bash

# writes a perf summary as a markdown file to stdout

set -e
sudo echo -n '' # do nothing. perf requires sudo. doing the prompt at the beginning

# e.g. -n makes the benchmarks apply sorting and uniqueness numerically
COMP_FLAGS=
# e.g. --unique-use-set
UNIQUE_FLAGS=

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# point to different perf paths for issues like this: https://github.com/microsoft/WSL/issues/9917
PERF_TOOL="${PERF_TOOL:-perf}" 

if ! command -v "$PERF_TOOL" &> /dev/null ; then
    echo "$PERF_TOOL could not be found. typically needs apt package linux-tools-generic"
    exit 1
fi

CHOOSE_PATH=~/.choose/choose
if ! command -v "$CHOOSE_PATH" &> /dev/null ; then
    echo "choose couldn't be found."
    echo " - make sure it's installed"
    echo " - try running this script without sudo (since choose is installed under user dir)"
    exit 1
fi

if [ ! -e "$SCRIPT_DIR/plain_text.txt" ]; then
    echo "generating plain_text.txt"
    wget -q https://www.gutenberg.org/files/1342/1342-0.txt
    for i in {1..65}
    do
        cat 1342-0.txt >> "$SCRIPT_DIR/plain_text.txt"
    done
    rm 1342-0.txt
    truncate -s 50000000 "$SCRIPT_DIR/plain_text.txt"
fi

if [ ! -e "$SCRIPT_DIR/test_repeated.txt" ]; then
    echo "generating test_repeated.txt"
    yes "test" | head -n 10000000 > "$SCRIPT_DIR/test_repeated.txt"
fi

if [ ! -e "$SCRIPT_DIR/no_duplicates.txt" ]; then
    echo "generating no_duplicates.txt"
    seq 6388888 > "$SCRIPT_DIR/no_duplicates.txt"
fi

run_get_time() {
    echo -n $(2>&1 sudo "$PERF_TOOL" stat --field-separator " " -- "${@:2}" < "$1" > /dev/null) | cut -d " " -f1 | tr -d '\n'
    echo -n " | "
}

echo -e "### Versions\n\`\`\`txt"
choose --version
pcre2grep --version
sed --version | head -n 1
awk --version | head -n 1
sort --version | head -n 1

echo -e "\`\`\`\n### Specs\n\`\`\`txt"
uname -r
if command -v lscpu &> /dev/null ; then
lscpu | grep -oP '^Model name: *\K.*' --color=never
fi
echo -n "ram: "
cat /proc/meminfo | grep -oP '^MemTotal: *\K.*' --color=never

echo -en "\`\`\`\\n
### Grepping

| (ms)             | choose | pcre2grep  |
|------------------|--------|------------|
| plain_text       | "

run_get_time "$SCRIPT_DIR/plain_text.txt" "$CHOOSE_PATH" -f test
run_get_time "$SCRIPT_DIR/plain_text.txt" pcre2grep test
echo -en "\n| test_repeated    | "
run_get_time "$SCRIPT_DIR/test_repeated.txt" "$CHOOSE_PATH" -f test
run_get_time "$SCRIPT_DIR/test_repeated.txt" pcre2grep test
echo -en "\n| no_duplicates    | "
run_get_time "$SCRIPT_DIR/no_duplicates.txt" "$CHOOSE_PATH" -f test
run_get_time "$SCRIPT_DIR/no_duplicates.txt" pcre2grep test
echo -en "\n\n\
### Stream Editing

| (ms)             | choose | sed  |
|------------------|--------|------|
| plain_text       | "

run_get_time "$SCRIPT_DIR/plain_text.txt" "$CHOOSE_PATH" --sed test --replace banana
run_get_time "$SCRIPT_DIR/plain_text.txt" sed -z "s/test/banana/g"
echo -en "\n| test_repeated    | "
run_get_time "$SCRIPT_DIR/test_repeated.txt" "$CHOOSE_PATH" --sed test --replace banana
run_get_time "$SCRIPT_DIR/test_repeated.txt" sed -z "s/test/banana/g"
echo -en "\n| no_duplicates    | "
run_get_time "$SCRIPT_DIR/no_duplicates.txt" "$CHOOSE_PATH" --sed test --replace banana
run_get_time "$SCRIPT_DIR/no_duplicates.txt" sed -z "s/test/banana/g"

echo -en "\n\n(here is a cherry picked great case for choose compared to sed)

| (ms)             | choose | sed (with newline delimiter) |
|------------------|--------|------|
| no_duplicates    | "

run_get_time "$SCRIPT_DIR/no_duplicates.txt" "$CHOOSE_PATH" --sed test --replace banana
run_get_time "$SCRIPT_DIR/no_duplicates.txt" sed "s/test/banana/g"

echo -en "\n\n(a special case, where choose cheats by using a literal replacement string)

| (ms)             | choose (delimiter sub) | sed |
|------------------|------------------------|-----|
| test_repeated    | "

run_get_time "$SCRIPT_DIR/test_repeated.txt" "$CHOOSE_PATH" test -o banana -d
run_get_time "$SCRIPT_DIR/test_repeated.txt" sed -z "s/test/banana/g"

echo -en "\n\n\
### Sorting $COMP_FLAGS

| (ms)             | choose | sort |
|------------------|--------|------|
| plain_text       | "

run_get_time "$SCRIPT_DIR/plain_text.txt" "$CHOOSE_PATH" -s $COMP_FLAGS 
run_get_time "$SCRIPT_DIR/plain_text.txt" sort $COMP_FLAGS
echo -en "\n| test_repeated    | "
run_get_time "$SCRIPT_DIR/test_repeated.txt" "$CHOOSE_PATH" -s $COMP_FLAGS
run_get_time "$SCRIPT_DIR/test_repeated.txt" sort $COMP_FLAGS
echo -en "\n| no_duplicates    | "
run_get_time "$SCRIPT_DIR/no_duplicates.txt" "$CHOOSE_PATH" -s $COMP_FLAGS
run_get_time "$SCRIPT_DIR/no_duplicates.txt" sort $COMP_FLAGS

echo -en "\n\n(a special case that leverages truncation)\n\n\

| (ms)             | choose -s --tail 5 | sort \| tail -n 5 |
|------------------|--------|------|
| no_duplicates    | "

run_get_time "$SCRIPT_DIR/no_duplicates.txt" "$CHOOSE_PATH" -s --tail 5 $COMP_FLAGS
run_get_time "$SCRIPT_DIR/no_duplicates.txt" sort $COMP_FLAGS | tail -n 5

echo -en "\n\n\
### Uniqueness

| (ms)             | choose | awk |
|------------------|--------|-----|
| plain_text       | "

run_get_time "$SCRIPT_DIR/plain_text.txt" "$CHOOSE_PATH" -u $UNIQUE_FLAGS
run_get_time "$SCRIPT_DIR/plain_text.txt" awk '!a[$0]++'
echo -en "\n| test_repeated    | "
run_get_time "$SCRIPT_DIR/test_repeated.txt" "$CHOOSE_PATH" -u $UNIQUE_FLAGS
run_get_time "$SCRIPT_DIR/test_repeated.txt" awk '!a[$0]++'
echo -en "\n| no_duplicates    | "
run_get_time "$SCRIPT_DIR/no_duplicates.txt" "$CHOOSE_PATH" -u $UNIQUE_FLAGS
run_get_time "$SCRIPT_DIR/no_duplicates.txt" awk '!a[$0]++'

echo -en "\n\n\
### Sorting and Uniqueness $COMP_FLAGS

| (ms)             | choose | sort -u |
|------------------|--------|---------|
| plain_text       | "

run_get_time "$SCRIPT_DIR/plain_text.txt" "$CHOOSE_PATH" -su $COMP_FLAGS $UNIQUE_FLAGS
run_get_time "$SCRIPT_DIR/plain_text.txt" sort -u $COMP_FLAGS
echo -en "\n| test_repeated    | "
run_get_time "$SCRIPT_DIR/test_repeated.txt" "$CHOOSE_PATH" -su $COMP_FLAGS $UNIQUE_FLAGS
run_get_time "$SCRIPT_DIR/test_repeated.txt" sort -u $COMP_FLAGS
echo -en "\n| no_duplicates    | "
run_get_time "$SCRIPT_DIR/no_duplicates.txt" "$CHOOSE_PATH" -su $COMP_FLAGS $UNIQUE_FLAGS
run_get_time "$SCRIPT_DIR/no_duplicates.txt" sort -u $COMP_FLAGS
echo ""
