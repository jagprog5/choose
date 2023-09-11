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

`sort` is using naive byte order (via `LC_ALL=C`), as this is the fairest. `sort` is faster than `choose` at sorting. If truncation is leveraged, or if there are many duplicates (when applying uniqueness as well), then `choose` is faster than `sort`.

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
choose 0.3.0, ncurses 6.2.20200212, pcre2 10.43
pcre2grep version 10.43-DEV 2023-04-14
sed (GNU sed) 4.7
GNU Awk 5.0.1, API: 2.0 (GNU MPFR 4.0.2, GNU MP 6.2.0)
sort (GNU coreutils) 8.30
```
### Specs
```txt
5.15.90.1-microsoft-standard-WSL2
AMD Ryzen 7 3800X 8-Core Processor
ram: 16331032 kB
```

### Grepping

| (ms)             | choose | pcre2grep  |
|------------------|--------|------------|
| plain_text       | 247.17 | 269.69 | 
| test_repeated    | 1620.34 | 1583.77 | 
| no_duplicates    | 323.59 | 370.16 | 

### Stream Editing

| (ms)             | choose | sed  |
|------------------|--------|------|
| plain_text       | 179.57 | 135.57 | 
| test_repeated    | 2725.50 | 1157.39 | 
| no_duplicates    | 5.10 | 44.00 | 

(here is a cherry picked great case for choose compared to sed)

| (ms)             | choose | sed (with newline delimiter) |
|------------------|--------|------|
| no_duplicates    | 5.13 | 543.93 | 

(a special case, where choose cheats by using a literal replacement string)

| (ms)             | choose (delimiter sub) | sed |
|------------------|------------------------|-----|
| test_repeated    | 1521.93 | 1156.64 | 

### Sorting 

| (ms)             | choose | sort |
|------------------|--------|------|
| plain_text       | 1628.88 | 448.06 | 
| test_repeated    | 1850.89 | 1616.13 | 
| no_duplicates    | 3714.94 | 1036.93 | 

(a special case that leverages truncation)

| (ms)             | choose -s --out 5 | sort \| head -n 5 |
|------------------|--------|------|
| no_duplicates    | 354.20 | 1059.81 | 

### Uniqueness 

| (ms)             | choose | awk |
|------------------|--------|-----|
| plain_text       | 111.95 | 214.41 | 
| test_repeated    | 565.31 | 1147.75 | 
| no_duplicates    | 2340.37 | 1496.42 | 

### Sorting and Uniqueness   -u

| (ms)             | choose | sort |
|------------------|--------|------|
| plain_text       | 122.80 | 440.57 | 
| test_repeated    | 558.86 | 1640.79 | 
| no_duplicates    | 5742.11 | 1168.84 | 


### Sorting and Uniqueness based on field   -u

| (ms)             | choose | sort |
|------------------|--------|------|
| csv_field        | 2770.27 | 474.02 | 
