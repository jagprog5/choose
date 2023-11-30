all:
	clang++ -std=gnu++17 -fsanitize=address,fuzzer main.cpp $$(pkg-config libpcre2-8 --libs --cflags) -ltbb -g -O3