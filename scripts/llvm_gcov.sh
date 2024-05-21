#!/bin/bash

# http://logan.tw/posts/2015/04/28/check-code-coverage-with-clang-and-lcov/

# this file is only needed if there's a version conflict when using gcov and llvm-cov
# which may be the case since llvm-cov is from clang's toolchain and gcov is from gcc's toolchain

exec llvm-cov gcov "$@"
