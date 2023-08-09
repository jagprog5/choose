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
| plain_text       | 248.26 | 284.37 | 
| test_repeated    | 1662.68 | 1611.63 | 
| no_duplicates    | 336.01 | 367.92 | 

### Stream Editing

| (ms)             | choose | sed  |
|------------------|--------|------|
| plain_text       | 180.83 | 140.47 | 
| test_repeated    | 2604.82 | 1163.14 | 
| no_duplicates    | 5.25 | 44.23 | 

(here is a cherry picked great case for choose compared to sed)

| (ms)             | choose | sed (with newline delimiter) |
|------------------|--------|------|
| no_duplicates    | 5.68 | 530.66 | 

### Sorting

| (ms)             | choose | sort |
|------------------|--------|------|
| plain_text       | 650.84 | 2254.89 | 
| test_repeated    | 1884.06 | 2252.19 | 
| no_duplicates    | 1849.60 | 7685.43 | 

(a cherry picked case that leverages truncation)


| (ms)             | choose --tail 5 | sort \| tail -n 5 |
|------------------|--------|------|
| no_duplicates    | 287.62 | 7578.84 | 

### Uniqueness

| (ms)             | choose | awk |
|------------------|--------|-----|
| plain_text       | 98.66 | 210.19 | 
| test_repeated    | 507.41 | 1152.60 | 
| no_duplicates    | 2018.39 | 1576.48 | 

### Sorting and Uniqueness

| (ms)             | choose | sort -u |
|------------------|--------|---------|
| plain_text       | 100.69 | 2100.42 | 
| test_repeated    | 502.81 | 2333.96 | 
| no_duplicates    | 3741.21 | 7988.70 | 
