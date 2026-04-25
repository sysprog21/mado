#!/usr/bin/env bash

set -e -u -o pipefail

cd "$(git rev-parse --show-toplevel)"

FAIL=0
while IFS= read -r -d '' file; do
    if ! clang-format-20 -- "${file}" | diff -u -p --label="${file}" --label="expected coding style" "${file}" -; then
        FAIL=1
    fi
done < <(git ls-files -z '*.c' '*.h' '*.cxx' '*.cpp' '*.hpp')

exit ${FAIL}
