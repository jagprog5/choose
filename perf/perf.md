# Benchmarks Comparing choose to Other Tools

Results are generated from [this script](./gen_perf_stats.bash).

## Summary

### Grepping

`pcre2grep` and `choose` have about the same speed.

### Stream Editing

`sed` reads input until it reaches a newline character, and puts the content thus far in a buffer where it is then manipulated. Because of this, `sed` performs extremely poorly on input files that contain many small lines (for the `no_duplicates` case below, `sed` with a newline delimiter (default) is x50 to x100 slower than `choose`). To normalize the performance, the `-z` option was used with `sed` (to change the delimiter to a null char, which never occurs in the input). `choose` doesn't use delimiters in this way, and can't come across this type of pathological case. After this normalization, `sed` is faster than `choose` except in cases where there are few substitutions to apply.

### Uniqueness

`choose` is faster than `awk` except in cases where there are few duplicates.

### Sorting, and Sorting + Uniqueness

For lexicographical comparison, `choose` is faster than `sort` and `sort -u`.

Numeric comparison is work in progress, and is slower.

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
| plain_text       | 242.830800 | 258.876600 | 
| test_repeated    | 1510.996600 | 1443.308400 | 
| no_duplicates    | 301.671400 | 322.054300 | 

### Stream Editing

| (ms)             | choose | sed  |
|------------------|--------|------|
| plain_text       | 174.011700 | 153.386400 | 
| test_repeated    | 2499.260000 | 1012.564700 | 
| no_duplicates    | 8.956400 | 49.756100 | 

(here is a cherry picked great case for choose compared to sed)

| (ms)             | choose | sed (with newline delimiter) |
|------------------|--------|------|
| no_duplicates    | 8.858600 | 440.801500 | 

(a special case, where choose cheats by using a literal replacement string)

| (ms)             | choose (delimiter sub) | sed |
|------------------|------------------------|-----|
| test_repeated    | 1474.160000 | 1018.034000 | 

### Sorting 

| (ms)             | choose | sort |
|------------------|--------|------|
| plain_text       | 701.490300 | 1910.916400 | 
| test_repeated    | 1889.095000 | 1995.766000 | 
| no_duplicates    | 1940.931600 | 6014.984300 | 

(a special case that leverages truncation)


| (ms)             | choose -s --tail 5 | sort \| tail -n 5 |
|------------------|--------|------|
| no_duplicates    | 261.985200 | 5648.350300 | 

### Uniqueness

| (ms)             | choose | awk |
|------------------|--------|-----|
| plain_text       | 120.523800 | 216.424400 | 
| test_repeated    | 509.372500 | 968.057900 | 
| no_duplicates    | 2508.705200 | 1496.170200 | 

### Sorting and Uniqueness 

| (ms)             | choose | sort -u |
|------------------|--------|---------|
| plain_text       | 112.288100 | 1919.796800 | 
| test_repeated    | 522.328000 | 2031.872300 | 
| no_duplicates    | 4819.380400 | 5785.641600 | 
