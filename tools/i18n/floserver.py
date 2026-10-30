"""Arabic (ar) and Central Kurdish (ckb) for the strings design D14 added
(FloServer in Settings).

Each needs a native read (listed in the PR). FloServer, PHP, Composer and
floserver.json stay as they are. Filled into app/i18n with
tools/fill_translations.py.
"""

T = {
 "FloServer %1 found": ("تم العثور على FloServer %1", "FloServer %1 دۆزرایەوە"),
 "Use FloServer's PHP and Composer": ("استخدام PHP وComposer من FloServer", "بەکارهێنانی PHP و Composer ی FloServer"),
 "Turning this off brings back the PHP and Composer you had before.":
   ("إيقاف هذا يعيد PHP وComposer اللذين كنت تستخدمهما من قبل.",
    "کوژاندنەوەی ئەمە PHP و Composer ی پێشووت دەگەڕێنێتەوە."),
 "FloServer isn't installed on this computer.": ("FloServer غير مثبّت على هذا الحاسوب.", "FloServer لەسەر ئەم کۆمپیوتەرە دانەمەزراوە."),
 "FloServer, per project (the version in each project's floserver.json)":
   ("FloServer، لكل مشروع (الإصدار المحدد في floserver.json الخاص بكل مشروع)",
    "FloServer، بۆ هەر پرۆژەیەک (ئەو وەشانەی لە floserver.json ی هەر پرۆژەیەکدا هەیە)"),
 "FloServer has no PHP installed yet.": ("لم يُثبَّت أي PHP في FloServer بعد.", "هێشتا هیچ PHP یەک لە FloServer دا دانەمەزراوە."),
 "FloServer's Composer %1": ("Composer %1 من FloServer", "Composer %1 ی FloServer"),
 "FloServer has no Composer installed yet.": ("لم يُثبَّت Composer في FloServer بعد.", "هێشتا Composer لە FloServer دا دانەمەزراوە."),
 "FloServer can't be found now, so Studio uses the PHP and Composer below until it is back.":
   ("تعذّر العثور على FloServer الآن، لذا يستخدم Studio PHP وComposer أدناه إلى أن يعود.",
    "ئێستا FloServer نادۆزرێتەوە، بۆیە Studio تا دەگەڕێتەوە PHP و Composer ی خوارەوە بەکاردەهێنێت."),
}
