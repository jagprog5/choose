# Benchmarks Comparing choose to Other Tools

"Results" section is generated from [this script](./gen_perf_stats.bash).

## Summary

### Grepping

`pcre2grep` and `choose` have about the same speed.

### Stream Editing

`sed` reads input until it reaches a newline character, and puts the content thus far in a buffer where it is then manipulated. Because of this, `sed` performs extremely poorly on input files that contain many small lines (for the `no_duplicates` case below, `sed` with a newline delimiter (default) is x50 to x100 slower than `choose`). To normalize the performance, the `-z` option was used with `sed` (to change the delimiter to a null char, which never occurs in the input). `choose` doesn't use delimiters in this way, and can't come across this type of pathological case. After this normalization, `sed` is faster than `choose` except in cases where there are few substitutions to apply.

### Uniqueness

`choose` is faster than `awk` except in cases where there are few duplicates.

### Sorting, and Sorting + Uniqueness

For lexicographical comparison, most of the time `choose` is faster than `sort` and `sort -u`.

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
| plain_text       | 258.897200 | 265.892700 | 
| test_repeated    | 1529.802700 | 1482.345200 | 
| no_duplicates    | 328.903000 | 330.548900 | 

### Stream Editing

| (ms)             | choose | sed  |
|------------------|--------|------|
| plain_text       | 176.831800 | 214.069400 | 
| test_repeated    | 2534.028300 | 1032.267900 | 
| no_duplicates    | 8.632200 | 52.020400 | 

(here is a cherry picked great case for choose compared to sed)

| (ms)             | choose | sed (with newline delimiter) |
|------------------|--------|------|
| no_duplicates    | 10.246200 | 456.193200 | 

(a special case, where choose cheats by using a literal replacement string)

| (ms)             | choose (delimiter sub) | sed |
|------------------|------------------------|-----|
| test_repeated    | 1448.012400 | 1043.297400 | 

### Sorting 

| (ms)             | choose | sort |
|------------------|--------|------|
| plain_text       | 757.884800 | 1966.817600 | 
| test_repeated    | 2105.637600 | 2052.248300 | 
| no_duplicates    | 2198.931600 | 5843.326100 | 

(a special case that leverages truncation)


| (ms)             | choose -s --tail 5 | sort \| tail -n 5 |
|------------------|--------|------|
| no_duplicates    | 244.461000 | 5221.271300 | 

### Uniqueness 

| (ms)             | choose | awk |
|------------------|--------|-----|
| plain_text       | 123.980200 | 221.344200 | 
| test_repeated    | 540.381100 | 995.740400 | 
| no_duplicates    | 2499.557400 | 1543.825300 | 

### Sorting and Uniqueness  

| (ms)             | choose | sort |
|------------------|--------|------|
| plain_text       | 120.672800 | 2054.020300 | 
| test_repeated    | 546.397400 | 1973.324700 | 
| no_duplicates    | 4395.422500 | 5829.518700 | 


### Sorting and Uniqueness based on field  

| (ms)             | choose | sort |
|------------------|--------|------|
| csv_field        | 1808.892400 | 2114.263400 | 
