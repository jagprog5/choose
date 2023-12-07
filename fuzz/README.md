# libFuzzer

Following [this guide](https://github.com/google/fuzzing/blob/master/tutorial/libFuzzerTutorial.md).

## without coverage
```bash
make && ./a.out -dict=dict.txt -timeout=20  -max_total_time=10800 # 3 hours or ctrl c when desired
```

## with coverage
```bash
make clean
make cov
./a.out -dict=dict.txt -timeout=20 -max_total_time=10800
make cov-show
```
