#pragma once

class Language;

// Checks that need no eyes: run with --self-test (works with
// QT_QPA_PLATFORM=offscreen). Prints one PASS/FAIL line per check and
// returns 0 only when every check passed.
int runSelfTest(Language &language);
