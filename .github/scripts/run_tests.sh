#!/usr/bin/env bash
# Runs the tests (ctest options as arguments) and repeats what matters as
# annotations: job logs can't always be read from outside GitHub.
#   .github/scripts/run_tests.sh build [ctest options...]
set +e
build="$1"; shift
ctest --test-dir "$build" --output-on-failure "$@" > ctest.log 2>&1; status=$?
cat ctest.log
if [ $status -ne 0 ]; then
  grep -E "FAIL!|Loc: |Actual|Expected|QFATAL|QWARN|\*\*\*Failed|\*\*\*Timeout|\*\*\*Exception|Not Run|runtime error:|ERROR: AddressSanitizer|SUMMARY:" ctest.log \
    | head -40 | sed 's/^/::error::/' || true
fi
grep -E "tests passed" ctest.log | sed 's/^/::notice::/' || true
exit $status
