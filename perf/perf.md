# Benchmarks Comparing choose to Other Tools

Results are generated from [this script](./gen_perf_stats.bash).

## Summary

### Grepping

`pcre2grep` and `choose` have about the same speed.

### Stream Editing

`sed` reads input until it reaches a newline character, and puts the content thus far in a buffer where it is then manipulated. Because of this, `sed` performs extremely poorly on input files that contain many small lines (for the `no_duplicates` case below, `sed` with a newline delimiter (default) was 10278% slower than `choose`). To normalize the performance, the `-z` option was used with `sed` (to change the delimiter to a null char, which never occurs in the input). `choose` doesn't use delimiters in this way, and can't come across this type of pathological case. After this normalization, `sed` is faster than `choose` except in cases where there are few substitutions to apply.

### Uniqueness

`choose` is faster than `awk` except in cases where there are few duplicates.

### Sorting, and Sorting + Uniqueness

`choose` is faster than `sort` and `sort -u`.

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

This file has a field where sorting and uniqueness should be applied.

```txt
garbage,1,garbage
garbage,2,garbage
garbage,3,garbage
garbage,4,garbage
garbage,5,garbage
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
| plain_text       | 255.38 | 267.21 | 
| test_repeated    | 1748.80 | 1607.19 | 
| no_duplicates    | 331.00 | 371.78 | 

### Stream Editing

| (ms)             | choose | sed  |
|------------------|--------|------|
| plain_text       | 189.39 | 134.62 | 
| test_repeated    | 2385.72 | 1149.04 | 
| no_duplicates    | 5.14 | 44.85 | 

(here is a cherry picked great case for choose compared to sed)

| (ms)             | choose | sed (with newline delimiter) |
|------------------|--------|------|
| no_duplicates    | 5.18 | 537.59 | 

(a special case, where choose cheats by using a literal replacement string)

| (ms)             | choose (delimiter sub) | sed |
|------------------|------------------------|-----|
| test_repeated    | 1655.90 | 1158.88 | 

### Sorting

| (ms)             | choose | sort |
|------------------|--------|------|
| plain_text       | 492.46 | 2155.83 | 
| test_repeated    | 1915.23 | 2378.51 | 
| no_duplicates    | 1974.91 | 7401.19 | 

(a cherry picked case that leverages truncation)


| (ms)             | choose --tail 5 | sort \| tail -n 5 |
|------------------|--------|------|
| no_duplicates    | 426.37 | 7671.96 | 

### Uniqueness

| (ms)             | choose | awk |
|------------------|--------|-----|
| plain_text       | 114.84 | 208.41 | 
| test_repeated    | 568.89 | 1131.56 | 
| no_duplicates    | 1944.59 | 1681.84 | 

### Sorting and Uniqueness

| (ms)             | choose | sort -u |
|------------------|--------|---------|
| plain_text       | 116.08 | 2081.22 | 
| test_repeated    | 585.39 | 2324.01 | 
| no_duplicates    | 3405.57 | 8049.54 | 
