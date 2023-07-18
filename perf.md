# Benchmarks Comparing choose to Other Tools

## Input Data

### plain_text

Each input file is the same size (50 million bytes), but the type of data is different.

This file represents an average random workload, which includes text from a novel repeated:
```bash
wget https://www.gutenberg.org/files/1342/1342-0.txt
rm -f plain_text.txt
for i in {1..65}
do
    cat 1342-0.txt >> plain_text.txt
done
rm 1342-0.txt
truncate -s 50000000 plain_text.txt
```

### test_repeated

This file has the line "test" repeated. "test" is the match target used throughout, below.

```bash
(for i in {1..10000000} ; do
    echo test
done) > test_repeated.txt
```

### no_duplicates

For filtering by uniqueness, there are two extremes. One is where the entire file consists of the same element repeatedly, which is in `test_repeated.txt`. The other is when every element is different. This file counts upwards from 1 for each line:

```bash
(for i in {1..6388888} ; do
    echo $i
done) > no_duplicates.txt
```

### long_words_alpha

This file consists of a large english dictionary in alphabetical order repeated. Intuitively it shouldn't be needed since the other input files have the same properties here, in particular `no_duplicates.txt` looks roughly the same. That being said, it gives some interesting results.

```bash
wget https://raw.githubusercontent.com/dwyl/english-words/master/words_alpha.txt
rm -f long_words_alpha.txt
for i in {1..12}
do
    cat words_alpha.txt >> long_words_alpha.txt
done
rm words_alpha.txt
truncate -s 50000000 long_words_alpha.txt
```


## Summary

### Grepping

| (ms)             | choose | pcre2grep |
|------------------|--------|-----------|
| plain_text       | 245    | 245       |
| test_repeated    | 1489   | 1434      |
| no_duplicates    | 304    | 315       |
| long_words_alpha | 337    | 337       |

`pcre2grep` and `choose` have the same speed.

### Stream Editing

| (ms)             | choose | sed  |
|------------------|--------|------|
| plain_text       | 202    | 168  |
| test_repeated    | 3073   | 1028 |
| no_duplicates    | 29     | 64   |
| long_words_alpha | 178    | 151  |

sed reads input until it reaches a newline character, and puts the content thus far in a buffer where it is then manipulated. Because of this, sed performs extremely poorly on input files that contain many small lines (for `no_duplicates`, `sed` was 4812% slower than `choose`). To normalize the performance, `tr` is used to make the input a single large line. This is applied to all input files, and for both `sed` and `choose`, to make the context the same.

After this normalization is applied, choose is slower than sed except in cases where there are few substitutions to apply.

If the delimiter form of substitution is used (invoked with `choose test -o banana -d`), then it significantly outperforms both normal `choose` and `sed` on `test_repeated`. However, this invocation is cheating since the replacement string must be a literal when `choose` is invoked this way, and since the input contains the substitution target repeatedly then effect is magnified.

### Uniqueness

todo

### Sorting + Uniqueness

todo

# Details


## Specs
```
5.15.90.1-microsoft-standard-WSL2 x86_64
Intel(R) Core(TM) i5-8600K CPU @ 3.60GHz
7926MiB System memory
```

## Versions

`choose`: built normally from this repo. git: 3f3e78c

`pcre2grep`: 10.42. compiled with -O3 (otherwise it was slower). Linked against same PCRE2 as choose.

For `sed`, `awk`, compiling from source with -O3 or -Ofast were slower than their default distributions. sed was slightly slower. awk was significantly slower. So instead, the default distributions were used.

`sed`: GNU sed 4.4

`awk`: GNU Awk 4.1.4, API: 1.1 (GNU MPFR 4.0.1, GNU MP 6.1.2)

`sort`: GNU coreutils 8.28

## Grepping Results

<table>
<tr>
<th>choose</th>
<th>pcre2grep</th>
</tr>
<tr>
<td>

```bash
perf stat choose -f "test" <plain_text.txt >/dev/null
    245.267300 task-clock:u (msec)   #    0.999 CPUs utilized
           174 page-faults:u         #    0.709 K/sec
     986816904 cycles:u              #    4.023 GHz
    2619315952 instructions:u        #    2.65  insn per cycle
     426820963 branches:u            # 1740.228 M/sec
       4374232 branch-misses:u       #    1.02% of all branches

   0.245568844 seconds time elapsed
```

</td>
<td>

```bash
perf stat pcre2grep "test" <plain_text.txt >/dev/null
    244.996800 task-clock:u (msec)   #    0.999 CPUs utilized
            69 page-faults:u         #    0.282 K/sec
     990857478 cycles:u              #    4.044 GHz
    2320841161 instructions:u        #    2.34  insn per cycle
     473689384 branches:u            # 1933.451 M/sec
       4386347 branch-misses:u       #    0.93% of all branches

   0.245266600 seconds time elapsed
```

</td>
</tr>
<tr>
<td>

```bash
perf stat choose -f "test" < test_repeated.txt > /dev/null
   1489.005200 task-clock:u (msec)   #    1.000 CPUs utilized
           172 page-faults:u         #    0.116 K/sec
    6119824366 cycles:u              #    4.110 GHz
   17610232734 instructions:u        #    2.88  insn per cycle
    2722636944 branches:u            # 1828.494 M/sec
       6084810 branch-misses:u       #    0.22% of all branches

   1.489278046 seconds time elapsed
```
</td>
<td>

```bash
perf stat pcre2grep "test" <test_repeated.txt >/dev/null
    1434.231600 task-clock:u (msec)   #    1.000 CPUs utilized
             72 page-faults:u         #    0.050 K/sec
     5902328795 cycles:u              #    4.115 GHz
    15746185065 instructions:u        #    2.67  insn per cycle
     2712006195 branches:u            # 1890.912 M/sec
        6022593 branch-misses:u       #    0.22% of all branches

   1.434517699 seconds time elapsed
```

</td>
</tr>
<tr>
<td>

```bash
perf stat choose -f "test" <no_duplicates.txt >/dev/null
    304.094000 task-clock:u (msec)   #    0.999 CPUs utilized
           171 page-faults:u         #    0.562 K/sec
    1233747918 cycles:u              #    4.057 GHz
    4384641960 instructions:u        #    3.55  insn per cycle
     625033133 branches:u            # 2055.394 M/sec
       1740901 branch-misses:u       #    0.28% of all branches

   0.304377133 seconds time elapsed
```
</td>
<td>

```bash
perf stat pcre2grep "test" <no_duplicates.txt >/dev/null
    315.174000 task-clock:u (msec)   #    0.999 CPUs utilized
            66 page-faults:u         #    0.209 K/sec
    1274733605 cycles:u              #    4.045 GHz
    4353056609 instructions:u        #    3.41  insn per cycle
     688265449 branches:u            # 2183.763 M/sec
       1574828 branch-misses:u       #    0.23% of all branches

   0.315426799 seconds time elapsed
```

</td>
</tr>
<tr>
<td>

```bash
perf stat choose -f "test" <long_words_alpha.txt >/dev/null
    337.093200 task-clock:u (msec)   #    0.999 CPUs utilized
           174 page-faults:u         #    0.516 K/sec
    1347980934 cycles:u              #    3.999 GHz
    3622508874 instructions:u        #    2.69  insn per cycle
     542919459 branches:u            # 1610.592 M/sec
       9683638 branch-misses:u       #    1.78% of all branches

   0.337406399 seconds time elapsed
```
</td>
<td>

```bash
perf stat pcre2grep "test" <long_words_alpha.txt >/dev/null
    337.126000 task-clock:u (msec)   #    0.999 CPUs utilized
            69 page-faults:u         #    0.205 K/sec
    1367403297 cycles:u              #    4.056 GHz
    3508979967 instructions:u        #    2.57  insn per cycle
     596864850 branches:u            # 1770.450 M/sec
       9558352 branch-misses:u       #    1.60% of all branches

   0.337368714 seconds time elapsed
```

</td>
</tr>
</table>

## Stream Editing Results

### Special Cases

<table>
<tr>
<th>choose (without tr)</th>
<th>sed (without tr)</th>
</tr>
<tr>
<td>

```bash
perf stat choose --sed test --replace banana <no_duplicates.txt >/dev/null
        8.888000 task-clock:u (msec)   #    0.970 CPUs utilized
             176 page-faults:u         #    0.020 M/sec
         6281924 cycles:u              #    0.707 GHz
        11687801 instructions:u        #    1.86  insn per cycle
         1881726 branches:u            #  211.715 M/sec
           22542 branch-misses:u       #    1.20% of all branches

   0.009164200 seconds time elapsed
```
</td>
<td>

```bash
perf stat sed "s/test/banana/g" <no_duplicates.txt >/dev/null
    436.616900 task-clock:u (msec)   #    0.999 CPUs utilized
           105 page-faults:u         #    0.240 K/sec
    1731991544 cycles:u              #    3.967 GHz
    5242487237 instructions:u        #    3.03  insn per cycle
    1208653630 branches:u            # 2768.225 M/sec
       3097768 branch-misses:u       #    0.26% of all branches

   0.437018000 seconds time elapsed
```
</td>
</tr>
<tr>
<th>choose (delimiter sub)</th>
</tr>
<tr>
<td>

```bash
cat test_repeated.txt | tr '\n' ' ' | perf stat ./choose test -o banana -d >/dev/null
   1620.451200 task-clock:u (msec)   #    1.000 CPUs utilized
           180 page-faults:u         #    0.111 K/sec
    6550636865 cycles:u              #    4.042 GHz
   16810745767 instructions:u        #    2.57  insn per cycle
    2482139150 branches:u            # 1531.758 M/sec
       5909715 branch-misses:u       #    0.24% of all branches

   1.620749112 seconds time elapsed
```
</td>
</tr>
</table>

### Normal Cases

<table>
<tr>
<th>choose</th>
<th>sed</th>
</tr>
<tr>
<td>

```bash
cat plain_text.txt | tr '\n' ' ' | perf stat choose sed test --replace banana >/dev/null
        201.818600 task-clock:u (msec)   #    0.999 CPUs utilized
               174 page-faults:u         #    0.862 K/sec
         682150871 cycles:u              #    3.380 GHz
        1744528913 instructions:u        #    2.56  insn per cycle
         249848443 branches:u            # 1237.985 M/sec
           2093876 branch-misses:u       #    0.84% of all branches

       0.202052499 seconds time elapsed
```
</td>
<td>

```bash
cat plain_text.txt | tr '\n' ' ' | perf stat sed "s/test/banana/g" >/dev/null
    167.801100 task-clock:u (msec)   #    0.732 CPUs utilized
          6254 page-faults:u         #    0.037 M/sec
     457966261 cycles:u              #    2.729 GHz
    1100177524 instructions:u        #    2.40  insn per cycle
     273711137 branches:u            # 1631.164 M/sec
       3749726 branch-misses:u       #    1.37% of all branches

   0.229282700 seconds time elapsed
```

</td>
</tr>
<tr>
<td>

```bash
cat test_repeated.txt | tr '\n' ' ' | perf stat choose --sed test --replace banana >/dev/null
   3073.173700 task-clock:u (msec)   #    1.000 CPUs utilized
           173 page-faults:u         #    0.056 K/sec
   12609185588 cycles:u              #    4.103 GHz
   32400871677 instructions:u        #    2.57  insn per cycle
    5682162355 branches:u            # 1848.956 M/sec
       6113574 branch-misses:u       #    0.11% of all branches

   3.073484894 seconds time elapsed
```
</td>
<td>

```bash
cat test_repeated.txt | tr '\n' ' ' | perf stat sed "s/test/banana/g" >/dev/null
   1028.177100 task-clock:u (msec)   #    0.946 CPUs utilized
          6969 page-faults:u         #    0.007 M/sec
    3992654364 cycles:u              #    3.883 GHz
   12370359018 instructions:u        #    3.10  insn per cycle
    2552417906 branches:u            # 2482.469 M/sec
         31733 branch-misses:u       #    0.00% of all branches

   1.087033603 seconds time elapsed
```
</td>
</tr>
<tr>
<td>

```bash
cat no_duplicates.txt | tr '\n' ' ' | perf stat choose --sed test --replace banana >/dev/null
     29.634700 task-clock:u (msec)   #    0.341 CPUs utilized
           174 page-faults:u         #    0.006 M/sec
      12351289 cycles:u              #    0.417 GHz
      11878313 instructions:u        #    0.96  insn per cycle
       1932301 branches:u            #   65.204 M/sec
         42279 branch-misses:u       #    2.19% of all branches

       0.086829402 seconds time elapsed
```
</td>
<td>

```bash
cat no_duplicates.txt | tr '\n' ' ' | perf stat sed "s/test/banana/g" >/dev/null
     64.197900 task-clock:u (msec)   #    0.543 CPUs utilized
          3693 page-faults:u         #    0.058 M/sec
     110831439 cycles:u              #    1.726 GHz
     370316620 instructions:u        #    3.34  insn per cycle
      52406336 branches:u            #  816.325 M/sec
         17699 branch-misses:u       #    0.03% of all branches

   0.118165890 seconds time elapsed
```

</td>
</tr>
<tr>
<td>

```bash
cat long_words_alpha.txt | tr '\n' ' ' | perf stat choose --sed test --replace banana >/dev/null
    178.899100 task-clock:u (msec)   #    0.998 CPUs utilized
           175 page-faults:u         #    0.978 K/sec
     590514697 cycles:u              #    3.301 GHz
    1483567971 instructions:u        #    2.51  insn per cycle
     212748274 branches:u            # 1189.208 M/sec
       2081461 branch-misses:u       #    0.98% of all branches

   0.179172107 seconds time elapsed
```
</td>
<td>

```bash
cat long_words_alpha.txt | tr '\n' ' ' | perf stat sed "s/test/banana/g" >/dev/null
    151.848400 task-clock:u (msec)   #    0.763 CPUs utilized
          5885 page-faults:u         #    0.039 M/sec
     415952744 cycles:u              #    2.739 GHz
     992536491 instructions:u        #    2.39  insn per cycle
     248891288 branches:u            # 1639.077 M/sec
       3316158 branch-misses:u       #    1.33% of all branches

   0.199078000 seconds time elapsed
```

</td>
</tr>
</table>

## Uniqueness Results

<table>
<tr>
<th>choose</th>
<th>awk</th>
</tr>
<tr>
<td>

```bash
perf stat choose -u <plain_text.txt >/dev/null
    329.207700 task-clock:u (msec)   #    0.999 CPUs utilized
           647 page-faults:u         #    0.002 M/sec
    1329956511 cycles:u              #    4.040 GHz
    2438797874 instructions:u        #    1.83  insn per cycle
     607867796 branches:u            # 1846.457 M/sec
      12451381 branch-misses:u       #    2.05% of all branches

   0.329466498 seconds time elapsed
```
</td>
<td>

```bash
perf stat awk '!a[$0]++' <plain_text.txt >/dev/null
    215.254200 task-clock:u (msec)   #    0.998 CPUs utilized
          1049 page-faults:u         #    0.005 M/sec
     814981754 cycles:u              #    3.786 GHz
    1633754905 instructions:u        #    2.00  insn per cycle
     296201839 branches:u            # 1376.056 M/sec
       2440128 branch-misses:u       #    0.82% of all branches

   0.215661300 seconds time elapsed
```
</td>
</tr>
<tr>
<td>

```bash
perf stat choose -u <test_repeated.txt >/dev/null
    348.813900 task-clock:u (msec)   #    0.999 CPUs utilized
           168 page-faults:u         #    0.482 K/sec
    1404598535 cycles:u              #    4.027 GHz
    5714910280 instructions:u        #    4.07  insn per cycle
    1270856070 branches:u            # 3643.364 M/sec
         29231 branch-misses:u       #    0.00% of all branches

   0.349076101 seconds time elapsed
```
</td>
<td>

```bash
perf stat awk '!a[$0]++' <test_repeated.txt >/dev/null
    971.913300 task-clock:u (msec)   #    1.000 CPUs utilized
           161 page-faults:u         #    0.166 K/sec
    3967108478 cycles:u              #    4.082 GHz
   10812848964 instructions:u        #    2.73  insn per cycle
    2420601846 branches:u            # 2490.553 M/sec
         58013 branch-misses:u       #    0.00% of all branches

   0.972395301 seconds time elapsed
```
</td>
</tr>
<tr>
<td>

```bash
perf stat choose -u <no_duplicates.txt >/dev/null
    2228.767300 task-clock:u (msec)  #    1.000 CPUs utilized
        129558 page-faults:u         #    0.058 M/sec
    7981664481 cycles:u              #    3.581 GHz
   21514327829 instructions:u        #    2.70  insn per cycle
    4766278918 branches:u            # 2138.527 M/sec
      34183913 branch-misses:u       #    0.72% of all branches

   2.229146992 seconds time elapsed
```
</td>
<td>

```bash
perf stat awk '!a[$0]++' <no_duplicates.txt >/dev/null
    1594.974100 task-clock:u (msec)   #    1.000 CPUs utilized
         152133 page-faults:u         #    0.095 M/sec
     5312234528 cycles:u              #    3.331 GHz
    12616181165 instructions:u        #    2.37  insn per cycle
     2927911330 branches:u            # 1835.711 M/sec
         378756 branch-misses:u       #    0.01% of all branches

   1.595443472 seconds time elapsed
```
</td>
</tr>
<tr>
<td>

```bash
perf stat choose -u <long_words_alpha.txt >/dev/null
    885.863700 task-clock:u (msec)   #    1.000 CPUs utilized
          9743 page-faults:u         #    0.011 M/sec
    3579921324 cycles:u              #    4.041 GHz
    7883667536 instructions:u        #    2.20  insn per cycle
    1787854537 branches:u            # 2018.205 M/sec
      43493612 branch-misses:u       #    2.43% of all branches

   0.886154601 seconds time elapsed
```
</td>
<td>

```bash
perf stat awk '!a[$0]++' <long_words_alpha.txt >/dev/null
    1296.258500 task-clock:u (msec)   #    1.000 CPUs utilized
          23784 page-faults:u         #    0.018 M/sec
     5150404081 cycles:u              #    3.973 GHz
     5414895654 instructions:u        #    1.05  insn per cycle
     1167756630 branches:u            #  900.867 M/sec
       13376948 branch-misses:u       #    1.15% of all branches

   1.296707223 seconds time elapsed
```
</td>
</tr>
</table>

## Sorting + Uniqueness Results

<table>
<tr>
<th>choose</th>
<th>sort</th>
</tr>
<tr>
<td>

```bash
perf stat choose -su <plain_text.txt >/dev/null
    332.082100 task-clock:u (msec)   #    0.999 CPUs utilized
           646 page-faults:u         #    0.002 M/sec
    1340307609 cycles:u              #    4.036 GHz
    2446866366 instructions:u        #    1.83  insn per cycle
     609238054 branches:u            # 1834.601 M/sec
      12614807 branch-misses:u       #    2.07% of all branches

   0.332367899 seconds time elapsed
```
</td>
<td>

```bash
perf stat sort -u <plain_text.txt >/dev/null
    1942.393400 task-clock:u (msec)   #    3.263 CPUs utilized
            296 page-faults:u         #    0.152 K/sec
     7500410100 cycles:u              #    3.861 GHz
    18195960765 instructions:u        #    2.43  insn per cycle
     3635054650 branches:u            # 1871.431 M/sec
       20608629 branch-misses:u       #    0.57% of all branches

   0.595291500 seconds time elapsed
```
</td>
</tr>
<tr>
<td>

```bash
perf stat choose -su <test_repeated.txt >/dev/null
    344.770800 task-clock:u (msec)   #    0.999 CPUs utilized
           166 page-faults:u         #    0.481 K/sec
    1405007808 cycles:u              #    4.075 GHz
    5684901686 instructions:u        #    4.05  insn per cycle
    1260853922 branches:u            # 3657.079 M/sec
         28616 branch-misses:u       #    0.00% of all branches

   0.345058201 seconds time elapsed
```
</td>
<td>

```bash
perf stat sort -u <test_repeated.txt >/dev/null
    2062.822300 task-clock:u (msec)   #    3.008 CPUs utilized
            635 page-faults:u         #    0.308 K/sec
     6975343917 cycles:u              #    3.381 GHz
    16580811739 instructions:u        #    2.38  insn per cycle
     4321550723 branches:u            # 2094.970 M/sec
        6071608 branch-misses:u       #    0.14% of all branches

   0.685817001 seconds time elapsed
```
</td>
</tr>
<tr>
<td>

```bash
perf stat choose -su <no_duplicates.txt >/dev/null
    3841.756200 task-clock:u (msec)  #    1.000 CPUs utilized
         129637 page-faults:u        #    0.034 M/sec
    14639931113 cycles:u             #    3.811 GHz
    35434893900 instructions:u       #    2.42  insn per cycle
     7710905888 branches:u           # 2007.130 M/sec
       78067476 branch-misses:u      #    1.01% of all branches

   3.842201496 seconds time elapsed
```
</td>
<td>

```bash
perf stat sort -u <no_duplicates.txt >/dev/null
    5868.992300 task-clock:u (msec)   #    2.561 CPUs utilized
            597 page-faults:u         #    0.102 K/sec
    22785020119 cycles:u              #    3.882 GHz
    83465749266 instructions:u        #    3.66  insn per cycle
    15899241077 branches:u            # 2709.024 M/sec
       46244840 branch-misses:u       #    0.29% of all branches

   2.292028637 seconds time elapsed
```
</td>
</tr>
<tr>
<td>

```bash
perf stat choose -su <long_words_alpha.txt >/dev/null
    938.033400 task-clock:u (msec)   #    1.000 CPUs utilized
          9078 page-faults:u         #    0.010 M/sec
    3787891576 cycles:u              #    4.038 GHz
    8356018771 instructions:u        #    2.21  insn per cycle
    1887367129 branches:u            # 2012.047 M/sec
      45760466 branch-misses:u       #    2.42% of all branches

   0.938293297 seconds time elapsed
```
</td>
<td>

```bash
perf stat sort -u <long_words_alpha.txt >/dev/null
    4214.658100 task-clock:u (msec)   #    3.022 CPUs utilized
            406 page-faults:u         #    0.096 K/sec
    15742138759 cycles:u              #    3.735 GHz
    56147112078 instructions:u        #    3.57  insn per cycle
    10597271764 branches:u            # 2514.385 M/sec
       41854676 branch-misses:u       #    0.39% of all branches

   1.394753167 seconds time elapsed
```
</td>
</tr>
</table>

