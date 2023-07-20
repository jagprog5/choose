# Benchmarks Comparing choose to Other Tools

## Input Data

Each input file is the same size (50 million bytes), but the type of data is different.

### plain_text

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

## Summary

### Grepping

| (ms)             | choose | pcre2grep |
|------------------|--------|-----------|
| plain_text       | 245    | 245       |
| test_repeated    | 1489   | 1434      |
| no_duplicates    | 304    | 315       |

`pcre2grep` and `choose` are around the same speed.

### Stream Editing

| (ms)             | choose | sed  |
|------------------|--------|------|
| plain_text       | 202    | 168  |
| test_repeated    | 2513   | 1028 |
| no_duplicates    | 32     | 64   |

sed reads input until it reaches a newline character, and puts the content thus far in a buffer where it is then manipulated. Because of this, sed performs extremely poorly on input files that contain many small lines (for `no_duplicates`, `sed` was 4812% slower than `choose`; see special cases' results below). To normalize the performance, `tr` is used to make the input a single large line. This is applied to all input files, and for both `sed` and `choose`, to make the context the same.

After this normalization is applied, choose is slower than sed except in cases where there are few substitutions to apply.

### Uniqueness

| (ms)          | choose | awk  |
|---------------|--------|------|
| plain_text    | 107    | 215  |
| test_repeated | 481    | 971  |
| no_duplicates | 2343   | 1594 |

`choose` is faster than `awk` except in cases where there are very few duplicates.

### Sorting + Uniqueness

| (ms)          | choose | sort |
|---------------|--------|------|
| plain_text    | 110    | 1942 |
| test_repeated | 484    | 2062 |
| no_duplicates | 4208   | 5868 |

`choose` is faster than `sort -u` in all observed cases.

# Details

## Specs
```
5.15.90.1-microsoft-standard-WSL2 x86_64
Intel(R) Core(TM) i5-8600K CPU @ 3.60GHz
7926MiB System memory
```

## Versions

`choose`: built normally from this repo

`pcre2grep`: 10.42. compiled with -O3 (otherwise it was slower). Linked against same PCRE2 as choose.

For `sed`, `awk`, compiling from source with -O3 or -Ofast was slower than their default distributions. `sed` was slightly slower. `awk` was significantly slower. So instead, the default distributions are used. The same is assumed for `sort`.

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
   2513.224300 task-clock:u (msec)   #    1.000 CPUs utilized
           174 page-faults:u         #    0.069 K/sec
   10211540625 cycles:u              #    4.063 GHz
   26790884965 instructions:u        #    2.62  insn per cycle
    4542166170 branches:u            # 1807.306 M/sec
       6077567 branch-misses:u       #    0.13% of all branches

   2.513608207 seconds time elapsed
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
     31.622500 task-clock:u (msec)   #    0.369 CPUs utilized
           174 page-faults:u         #    0.006 M/sec
      14401583 cycles:u              #    0.455 GHz
      11920871 instructions:u        #    0.83  insn per cycle
       1943296 branches:u            #   61.453 M/sec
         47956 branch-misses:u       #    2.47% of all branches

   0.085747798 seconds time elapsed
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
    106.750500 task-clock:u (msec)   #    0.998 CPUs utilized
           949 page-faults:u         #    0.009 M/sec
     410042688 cycles:u              #    3.841 GHz
    1023962447 instructions:u        #    2.50  insn per cycle
     215846511 branches:u            # 2021.972 M/sec
       1896941 branch-misses:u       #    0.88% of all branches

   0.106990793 seconds time elapsed
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
    481.670400 task-clock:u (msec)   #    0.999 CPUs utilized
           165 page-faults:u         #    0.343 K/sec
    1968609243 cycles:u              #    4.087 GHz
    5394918025 instructions:u        #    2.74  insn per cycle
    1190860710 branches:u            # 2472.356 M/sec
         32754 branch-misses:u       #    0.00% of all branches

   0.481974105 seconds time elapsed
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
   2343.634700 task-clock:u (msec)   #    1.000 CPUs utilized
         97783 page-faults:u         #    0.042 M/sec
    8193885512 cycles:u              #    3.496 GHz
    9292060393 instructions:u        #    1.13  insn per cycle
    1927760309 branches:u            #  822.552 M/sec
       1335796 branch-misses:u       #    0.07% of all branches

   2.344090848 seconds time elapsed
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
    110.169500 task-clock:u (msec)   #    0.997 CPUs utilized
           949 page-faults:u         #    0.009 M/sec
     416212469 cycles:u              #    3.778 GHz
    1032117161 instructions:u        #    2.48  insn per cycle
     217117847 branches:u            # 1970.762 M/sec
       1994659 branch-misses:u       #    0.92% of all branches

   0.110467800 seconds time elapsed
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
    484.082800 task-clock:u (msec)   #    0.999 CPUs utilized
           166 page-faults:u         #    0.343 K/sec
    1980268873 cycles:u              #    4.091 GHz
    5364908806 instructions:u        #    2.71  insn per cycle
    1180858324 branches:u            # 2439.373 M/sec
         36116 branch-misses:u       #    0.00% of all branches

   0.484365000 seconds time elapsed
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
   4208.411300 task-clock:u (msec)   #    0.999 CPUs utilized
         98150 page-faults:u         #    0.023 M/sec
   15525102544 cycles:u              #    3.689 GHz
   23800391635 instructions:u        #    1.53  insn per cycle
    4857920278 branches:u            # 1154.336 M/sec
      45476563 branch-misses:u       #    0.94% of all branches

   4.211042118 seconds time elapsed
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
</table>

