.PHONY: all cov cov-show clean

all:
	clang++ -std=gnu++17 -fsanitize=address,fuzzer main.cpp $$(pkg-config libpcre2-8 --libs --cflags) -ltbb -g -O3

clean:
	rm -f ./a.out coverage.info main.gcda main.gcno *.png *.html *.css

cov:
	clang++ -DCHOOSE_FUZZ_DOING_COV --coverage -std=gnu++17 -fsanitize=address,fuzzer main.cpp $$(pkg-config libpcre2-8 --libs --cflags) -ltbb -g -O0

cov-show:
	lcov --gcov-tool ./llvm_gcov.sh --capture --directory . --directory ../src --no-external --output-file coverage.info
	genhtml coverage.info
	sensible-browser index.html
