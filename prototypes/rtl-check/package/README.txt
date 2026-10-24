FloCMS Studio - Kurdish and RTL check (stage 0)
===============================================

This is a test build, not FloCMS Studio itself. It shows two screens of
the approved design so you can check Kurdish and Arabic on your own PC.

Run it
------
Double-click rtlcheck.exe. Keep the files in this folder together: the
exe needs the Qt files next to it.

At the bottom of the sidebar:
  - English / العربية / کوردی   switch the language while it runs
  - Dark theme                   switch light and dark
The Overview and Doctor tabs work; the other tabs are greyed out.
Type Kurdish or Arabic into the "Search projects" field to test typing,
the cursor and selecting text.

From Command Prompt, in this folder:
  rtlcheck.exe --lang ckb --page doctor
  rtlcheck.exe --lang ar --page overview --dark
  rtlcheck.exe --self-test

--self-test prints nothing when started by double-click; run it from
Command Prompt and add  > selftest.txt  to save the result to a file.

What to look at
---------------
  1. Letters join correctly, including ڕ ڵ ۆ ێ
  2. The whole window mirrors: sidebar on the right, buttons in order
  3. Code words such as .env and storage/cache/ keep their dots and
     slashes in the right place
  4. Typing, the cursor and selection in the search field
  5. Kurdish and Arabic show Arabic digits (٢٤)
  6. The language changes without restarting

The source is in prototypes/rtl-check/ of
https://github.com/hostkurd/flocms-studio (MIT). Qt is used under the
LGPL v3 (licences in the Qt folders and at https://www.qt.io/licensing/);
the fonts are under the SIL Open Font License 1.1.
