# Benchmarks Comparing choose to Other Tools

Results are generated from [this script](./gen_perf_stats.bash).

## Input Data

Each input file is the same size (50 million bytes), but the type of data is different.

### plain_text

This file represents an average random workload, which includes text from a novel repeated

### test_repeated

This file has the line "test" repeated. "test" is the match target used throughout, below.

### no_duplicates

For filtering by uniqueness, there are two extremes. One is where the entire file consists of the same element repeatedly, which is in `test_repeated.txt`. The other is when every element is different. This file counts upwards from 1 for each line:

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
| plain_text       | 249.21 | 280.52 | 
| test_repeated    | 1667.86 | 1613.70 | 
| no_duplicates    | 330.10 | 366.01 | 

### Stream Editing

| (ms)             | choose | sed  |
|------------------|--------|------|
| plain_text       | 189.94 | 134.90 | 
| test_repeated    | 2628.95 | 1155.71 | 
| no_duplicates    | 5.24 | 44.88 | 

(here is a cherry picked great case for choose compared to sed)

| (ms)             | choose | sed (with newline delimiter) |
|------------------|--------|------|
| no_duplicates    | 5.59 | 533.39 | 

### Sorting

| (ms)             | choose | awk  |
|------------------|--------|------|
| plain_text       | 674.98 | 2124.21 | 
| test_repeated    | 1846.22 | 2327.35 | 
| no_duplicates    | 1895.45 | 8123.26 | 

(a cherry picked case that leverages truncation)


| (ms)             | choose --tail 5 | sort \| tail -n 5 |
|------------------|--------|------|
| no_duplicates    | 284.82 | 7137.14 | 

### Uniqueness

| (ms)             | choose | awk |
|------------------|--------|-----|
| plain_text       | 100.32 | 229.74 | 
| test_repeated    | 504.57 | 1140.30 | 
| no_duplicates    | 1971.65 | 1563.60 | 

### Sorting and Uniqueness

| (ms)             | choose | sort -u |
|------------------|--------|---------|
| plain_text       | 99.93 | 2137.85 | 
| test_repeated    | 512.56 | 2380.91 | 
| no_duplicates    | 3722.76 | 8108.00 | 
