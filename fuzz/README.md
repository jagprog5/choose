# libFuzzer

Following [this guide](https://github.com/google/fuzzing/blob/master/tutorial/libFuzzerTutorial.md).

## without coverage
```bash
make && ./a.out -max_total_time=28800 # 8 hours or ctrl c when desired
```

## with coverage
```bash
make clean
make cov
./a.out -timeout=10 -max_total_time=28800
make cov-show
```
