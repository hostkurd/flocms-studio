# Stage 0: Kurdish and RTL check

A small, throwaway Qt Quick app that shows two of the approved boards
(board 4, the Arabic overview, and board 7, the Kurdish Doctor) so we can
check that Qt renders Kurdish and Arabic correctly before any real screens
are built (design note D4). The real app starts in stage 1, in its own
folders; this one can be deleted once stage 1 is merged.

## What it checks (D4)

| # | Check | How | Result (Ubuntu, Qt 6.4.2) |
|---|---|---|---|
| 1 | Arabic and Kurdish letters join, including ڕ ڵ ۆ ێ | screenshots | pass |
| 2 | The whole layout mirrors: sidebar, icons, button order | screenshots; the Settings chevron is measured | pass |
| 3 | Code words (`.env`, `storage/cache/`) keep LTR order and punctuation | `--self-test` measures glyph positions | pass |
| 4 | Typing Arabic/Kurdish, cursor and selection | typed into the search field with xdotool | pass |
| 5 | Kurdish uses Arabic digits | `--self-test` (QLocale `ckb_IQ`) | pass |
| 6 | Language switches while the app runs | the buttons at the bottom of the sidebar | pass |

CI runs the self-test and takes screenshots with Qt 6.12 on Windows,
macOS and Ubuntu (`.github/workflows/rtl-check.yml`); the screenshots are
in the workflow run's artifacts.

## How the RTL parts work

- `LayoutMirroring` on the window's root item mirrors everything; labels
  use `Text.AlignLeft`, which mirroring turns into right alignment, so a
  Latin project name in a Kurdish window still starts on the right.
- `Language::set()` swaps the translator, sets `QLocale::setDefault`
  (Arabic-Indic digits for `ar` and `ckb`), sets the application layout
  direction and calls `QQmlEngine::retranslate()`.
- Source strings mark code words with backticks. `Language::rich()` turns
  each into JetBrains Mono wrapped in LRI…PDI isolates, and starts RTL
  strings with an RLM. These marks exist only on screen: `check_ts.py`
  fails if a translation file contains any.
- Command output (the terminal block, `admin:doctor` lines) is always
  shown left to right with mirroring turned off.

## Findings

- Isolates (LRI…PDI) work in Qt's plain, styled and rich text, in Qt 6.4
  and 6.12. Without them `.env` shows as `env.` and `storage/cache/` loses
  its trailing slash in RTL; the self-test fails in that case.
- `StyledText` cannot set a font family, so code words need `RichText`.
- Plural strings (`%Ln table(s)`) need a translation file for English too,
  or English shows "24 table(s)". Arabic has six plural forms, Kurdish one.
- Qt 6.4's `lupdate` refuses to update a `ckb` file that has translations
  ("target language is not recognized"); Qt 6.12's `lupdate` accepts it.
  `lrelease` works in both.
- Fonts are bundled (Noto Sans Arabic for Arabic and Kurdish), so text
  looks the same on every system. Instrument Sans and Bricolage Grotesque
  are not available here yet; this check uses Noto Sans for Latin text.

## Build and run

Needs Qt 6.4 or newer with Qt Quick, Quick Controls and the Linguist tools.

```
cmake -S prototypes/rtl-check -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
build/rtlcheck --lang ckb --page doctor
build/rtlcheck --self-test
build/rtlcheck --lang ar --page overview --dark --screenshot ar.png
python3 prototypes/rtl-check/check_ts.py prototypes/rtl-check/i18n/*.ts
```

Options: `--lang en|ar|ckb`, `--page overview|doctor`, `--dark`,
`--screenshot file.png` (saves the window and quits), `--self-test`.

## Fonts

`fonts/` holds Noto Sans, Noto Sans Arabic and JetBrains Mono, all under
the SIL Open Font License 1.1; see `fonts/OFL.txt`.
