# Summary

todo

# Specs
```
5.15.90.1-microsoft-standard-WSL2 x86_64
Intel(R) Core(TM) i5-8600K CPU @ 3.60GHz
7926MiB System memory DDR4
```

# Input

Each input file is the same size, but the type of data is different.

This file represents an average random workload:
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

This file is the extreme case where every line consists of the match target:

```bash
(for i in {1..10000000} ; do
    echo test
done) > test_repeated.txt
```

For filtering by uniqueness, there are two extremes. One is where the entire file consists of the same element repeatedly, which is in `test_repeated.txt`. The other is when every element is different:

```bash
(for i in {1..6388888} ; do
    echo $i
done) > no_duplicates.txt
```

This file consists of a large english dictionary in alphabetical order repeated over and over again. Intuitively it shouldn't be handled much differently from `no_duplicates.txt` in terms of speed, but it is. 

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

# Versions

`choose`: built normally from this repo. git: 3f3e78c

`pcre2grep`: 10.42. compiled with -O3 (otherwise it was slower). Linked against same PCRE2 as choose.

For `sed`, `awk`, compiling from source with -O3 or -Ofast were slower than their default distributions. sed was slightly slower. awk was significantly slower. So instead, the default distributions were used.

`sed`: GNU sed 4.4

`awk`: GNU Awk 4.1.4, API: 1.1 (GNU MPFR 4.0.1, GNU MP 6.1.2)

`sort`: GNU coreutils 8.28

# Grepping

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
perf stat choose -f "test" <test_repeated.txt >/dev/null
    1514.198700 task-clock:u (msec)   #    1.000 CPUs utilized
            174 page-faults:u         #    0.115 K/sec
     6121141342 cycles:u              #    4.042 GHz
    17610290437 instructions:u        #    2.88  insn per cycle
     2722649555 branches:u            # 1798.079 M/sec
        5820203 branch-misses:u       #    0.21% of all branches

   1.514510533 seconds time elapsed
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

# Stream Editing

<table>
<tr>
<th>choose</th>
<th>choose (delimiters)</th>
<th>sed</th>
</tr>
<tr>
<td>

```bash
perf stat choose --sed test --replace banana <plain_text.txt >/dev/null
    177.904700 task-clock:u (msec)   #    0.998 CPUs utilized
           172 page-faults:u         #    0.967 K/sec
     693789171 cycles:u              #    3.900 GHz
    1744531288 instructions:u        #    2.51  insn per cycle
     249849134 branches:u            # 1404.399 M/sec
       2011794 branch-misses:u       #    0.81% of all branches

   0.178198299 seconds time elapsed
```
</td>
<td>

```bash
perf stat choose test -o banana -d <plain_text.txt >/dev/null
    177.644300 task-clock:u (msec)   #    0.999 CPUs utilized
           175 page-faults:u         #    0.985 K/sec
     691145436 cycles:u              #    3.891 GHz
    1807306916 instructions:u        #    2.61  insn per cycle
     259889872 branches:u            # 1462.979 M/sec
       1999087 branch-misses:u       #    0.77% of all branches

   0.177908999 seconds time elapsed
```
</td>
<td>

```bash
perf stat sed "s/test/banana/g" <plain_text.txt >/dev/null
    155.238500 task-clock:u (msec)   #    0.998 CPUs utilized
           111 page-faults:u         #    0.715 K/sec
     561185246 cycles:u              #    3.615 GHz
    1165869857 instructions:u        #    2.08  insn per cycle
     251362578 branches:u            # 1619.203 M/sec
       7617861 branch-misses:u       #    3.03% of all branches

   0.155606800 seconds time elapsed
```

</td>
</tr>
<tr>
<td>

```bash
perf stat choose --sed test --replace banana <test_repeated.txt >/dev/null
    3065.746500 task-clock:u (msec)   #    1.000 CPUs utilized
            173 page-faults:u         #    0.056 K/sec
    12601093537 cycles:u              #    4.110 GHz
    32400877383 instructions:u        #    2.57  insn per cycle
     5682164005 branches:u            # 1853.436 M/sec
        6022966 branch-misses:u       #    0.11% of all branches

   3.066126318 seconds time elapsed
```
</td>
<td>

```bash
perf stat choose test -o banana -d <test_repeated.txt >/dev/null
    1408.920600 task-clock:u (msec)   #    1.000 CPUs utilized
            176 page-faults:u         #    0.125 K/sec
     5772467830 cycles:u              #    4.097 GHz
    16810778875 instructions:u        #    2.91  insn per cycle
     2482145419 branches:u            # 1761.735 M/sec
        5839965 branch-misses:u       #    0.24% of all branches

   1.409210694 seconds time elapsed
```
</td>
<td>

```bash
perf stat sed "s/test/banana/g" <test_repeated.txt >/dev/null
    2402.313700 task-clock:u (msec)   #    1.000 CPUs utilized
            109 page-faults:u         #    0.045 K/sec
     9772987703 cycles:u              #    4.068 GHz
    26661110871 instructions:u        #    2.73  insn per cycle
     5502775099 branches:u            # 2290.615 M/sec
        5901251 branch-misses:u       #    0.11% of all branches

   2.402897100 seconds time elapsed
```
</td>
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
perf stat choose test -o banana -d <no_duplicates.txt >/dev/null
        10.209100 task-clock:u (msec)   #   0.975 CPUs utilized
           173 page-faults:u            #   0.017 M/sec
       6581279 cycles:u                 #   0.645 GHz
      11773483 instructions:u           #   1.79  insn per cycle
       1893267 branches:u               # 185.449 M/sec
         22748 branch-misses:u          #   1.20% of all branches

   0.010466000 seconds time elapsed
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
<td>

```bash
perf stat choose --sed test --replace banana <long_words_alpha.txt >/dev/null
    149.773500 task-clock:u (msec)   #    0.998 CPUs utilized
           172 page-faults:u         #    0.001 M/sec
     574648880 cycles:u              #    3.837 GHz
    1483554984 instructions:u        #    2.58  insn per cycle
     212746634 branches:u            # 1420.456 M/sec
       2009084 branch-misses:u       #    0.94% of all branches

   0.150035201 seconds time elapsed
```
</td>
<td>

```bash
perf stat choose test -o banana -d <long_words_alpha.txt >/dev/null
    152.271100 task-clock:u (msec)   #    0.998 CPUs utilized
           177 page-faults:u         #    0.001 M/sec
     592787852 cycles:u              #    3.893 GHz
    1526427091 instructions:u        #    2.57  insn per cycle
     219458782 branches:u            # 1441.237 M/sec
       2011472 branch-misses:u       #    0.92% of all branches

   0.152563101 seconds time elapsed
```
</td>
<td>

```bash
perf stat sed "s/test/banana/g" <long_words_alpha.txt >/dev/null
    391.912700 task-clock:u (msec)   #    0.999 CPUs utilized
           111 page-faults:u         #    0.283 K/sec
    1548611992 cycles:u              #    3.951 GHz
    3730218026 instructions:u        #    2.41  insn per cycle
     853233847 branches:u            # 2177.102 M/sec
      12118311 branch-misses:u       #    1.42% of all branches

   0.392260900 seconds time elapsed
```

</td>
</tr>
</table>

# Uniqueness

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

# Sorting + Uniqueness

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

