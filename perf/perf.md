# Benchmarks Comparing choose to Other Tools

"Results" section is generated from [this script](./gen_perf_stats.bash). It reports the task-clock for each command (which can differ from elapsed time). The script provides options regarding sorting and uniqueness. The defaults were used but different options lead to different results. The matrix of possibilities would be very large, so only the defaults are shown below.

Also note the compile time option called `DISABLE_FIELD`. It disables the `--field` arg, and removes some information associated with each token. This provides a small boost to sorting and uniqueness since there's a smaller memory footprint. `--field` is not disabled by default, and inline with above, the benchmarks are run on the default options.

## Summary

### Grepping

`pcre2grep` and `choose` have about the same speed.

### Stream Editing

`sed` reads input until it reaches a newline character, and puts the content thus far in a buffer where it is then manipulated. Because of this, `sed` performs extremely poorly on input files that contain many small lines (for the `no_duplicates` case below, `sed` with a newline delimiter (default) is x50 to x100 slower than `choose`). To normalize the performance, the `-z` option was used with `sed` (to change the delimiter to a null char, which never occurs in the input). `choose` doesn't use delimiters in this way, and can't come across this type of pathological case. After this normalization, `sed` is faster than `choose` except in cases where there are few substitutions to apply.

### Uniqueness

`choose` is faster than `awk` except in cases where there are few duplicates.

### Sorting, and Sorting + Uniqueness

In most cases, `choose` is faster than `sort` and `sort -u` at sorting and sorting + uniqueness, respectively.

## Input Data

Each input file is the same size (50 million bytes), but the type of data is different.

### plain_text

This file represents an average random workload, which includes text from a novel repeated.

```txt
The Project Gutenberg eBook of Pride and prejudice, by Jane Austen

This eBook is for the use of anyone anywhere in the United States and
most other parts of the world at no cost and with almost no restrictions
whatsoever. You may copy it, give it away or re-use it under the terms
...
```

### test_repeated

This file has the line "test" repeated. "test" is the match target used throughout, below. This includes grepping for the word "test", or substituting "test" to "banana".

```txt
test
test
test
test
test
...
```

### no_duplicates

For filtering by uniqueness, there are two extremes. One is where the entire file consists of the same element repeatedly, which is in `test_repeated.txt`. The other is when every element is different. This file counts upwards from 1 for each line:

```txt
1
2
3
4
5
...
```

### csv_field

This file has a field where sorting and uniqueness should be applied. Note that even though the field is numeric, both sort and choose are using a lexicographical comparison in the benchmark. 

```txt
garbage,1,garbage
garbage,2,garbage
garbage,3,garbage
garbage,4,garbage
garbage,5,garbage
...
```

## Results

### Versions
```txt
choose 0.3.0, ncurses 6.1.20180127, pcre2 10.42
pcre2grep version 10.42 2022-12-11
sed (GNU sed) 4.4
GNU Awk 4.1.4, API: 1.1 (GNU MPFR 4.0.1, GNU MP 6.1.2)
sort (GNU coreutils) 8.28
```
### Specs
```txt
5.15.90.1-microsoft-standard-WSL2
Intel(R) Core(TM) i5-8600K CPU @ 3.60GHz
ram: 8116584 kB
```

### Grepping

| (ms)             | choose | pcre2grep  |
|------------------|--------|------------|
| plain_text       | 238.334100 | 246.104400 | 
| test_repeated    | 1536.390100 | 1446.540000 | 
| no_duplicates    | 321.083200 | 313.054700 | 

### Stream Editing

| (ms)             | choose | sed  |
|------------------|--------|------|
| plain_text       | 173.019600 | 156.455300 | 
| test_repeated    | 2563.258500 | 1024.358400 | 
| no_duplicates    | 8.424300 | 46.834200 | 

(here is a cherry picked great case for choose compared to sed)

| (ms)             | choose | sed (with newline delimiter) |
|------------------|--------|------|
| no_duplicates    | 8.245600 | 437.878300 | 

(a special case, where choose cheats by using a literal replacement string)

| (ms)             | choose (delimiter sub) | sed |
|------------------|------------------------|-----|
| test_repeated    | 1457.271000 | 1010.783600 | 

### Sorting 

| (ms)             | choose | sort |
|------------------|--------|------|
| plain_text       | 694.556000 | 1905.257700 | 
| test_repeated    | 2226.087400 | 1987.113500 | 
| no_duplicates    | 2120.992700 | 5092.179100 | 

(a special case that leverages truncation)

| (ms)             | choose -s --out 5 | sort \| head -n 5 |
|------------------|--------|------|
| no_duplicates    | 251.069600 | 5063.083100 | 

### Uniqueness 

| (ms)             | choose | awk |
|------------------|--------|-----|
| plain_text       | 114.649800 | 208.971700 | 
| test_repeated    | 578.412600 | 972.325200 | 
| no_duplicates    | 2480.435700 | 1477.912300 | 

### Sorting and Uniqueness   -u

| (ms)             | choose | sort |
|------------------|--------|------|
| plain_text       | 106.970100 | 1906.801600 | 
| test_repeated    | 574.516000 | 1961.279100 | 
| no_duplicates    | 4165.485200 | 5670.807600 | 


### Sorting and Uniqueness based on field   -u

| (ms)             | choose | sort |
|------------------|--------|------|
| csv_field        | 1779.289000 | 1987.503500 | 
