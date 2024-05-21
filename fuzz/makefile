.PHONY: all cov cov-show clean dict

all: dict
	clang++ -std=gnu++17 -fsanitize=address,fuzzer main.cpp $$(pkg-config libpcre2-8 --libs --cflags) -ltbb -g -O3

dict: 
	choose --auto-completion-strings | choose -r --sub '.*' '"$$0\x00"' > dict.txt

clean:
	rm -f dict.txt ./a.out coverage.info main.gcda main.gcno *.png *.html *.css

cov: dict
	clang++ -DCHOOSE_FUZZ_DOING_COV --coverage -std=gnu++17 -fsanitize=address,fuzzer main.cpp $$(pkg-config libpcre2-8 --libs --cflags) -ltbb -g -O3

cov-show:
	lcov --gcov-tool ../scripts/llvm_gcov.sh --capture --directory . --directory ../src --no-external --output-file coverage.info
	genhtml coverage.info
	sensible-browser index.html
