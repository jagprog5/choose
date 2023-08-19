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

`choose` doesn't do numeric comparison very speedily at this point. It's ok but has room for improvement.

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
| plain_text       | 255.571900 | 255.602800 | 
| test_repeated    | 1541.437300 | 1456.649400 | 
| no_duplicates    | 335.363600 | 328.095000 | 

### Stream Editing

| (ms)             | choose | sed  |
|------------------|--------|------|
| plain_text       | 178.085900 | 157.202200 | 
| test_repeated    | 2519.054900 | 1027.284900 | 
| no_duplicates    | 8.691000 | 52.048900 | 

(here is a cherry picked great case for choose compared to sed)

| (ms)             | choose | sed (with newline delimiter) |
|------------------|--------|------|
| no_duplicates    | 9.701600 | 455.517000 | 

(a special case, where choose cheats by using a literal replacement string)

| (ms)             | choose (delimiter sub) | sed |
|------------------|------------------------|-----|
| test_repeated    | 1489.126500 | 1045.871200 | 

### Sorting 

| (ms)             | choose | sort |
|------------------|--------|------|
| plain_text       | 758.170400 | 2032.359300 | 
| test_repeated    | 1890.213400 | 2023.206800 | 
| no_duplicates    | 2014.638900 | 5743.573000 | 

(a special case that leverages truncation)


| (ms)             | choose -s --tail 5 | sort \| tail -n 5 |
|------------------|--------|------|
| no_duplicates    | 234.092700 | 5297.302100 | 

### Uniqueness

| (ms)             | choose | awk |
|------------------|--------|-----|
| plain_text       | 119.411900 | 217.741600 | 
| test_repeated    | 510.382300 | 1004.464100 | 
| no_duplicates    | 2435.280500 | 1502.059800 | 

### Sorting and Uniqueness 

| (ms)             | choose | sort -u |
|------------------|--------|---------|
| plain_text       | 122.549100 | 2066.645100 | 
| test_repeated    | 511.756500 | 2001.259400 | 
| no_duplicates    | 4034.020900 | 5780.361300 | 
