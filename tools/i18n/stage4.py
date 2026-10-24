"""Arabic (ar) and Central Kurdish (ckb) for the strings stage 4 added.

Each needs a native read (listed in the PR). Keys are the English source
strings; P holds plurals: English (2 forms), Arabic (6), Kurdish (2).
Filled into app/i18n with tools/fill_translations.py."""

T = {
 "Server running at %1": ("الخادم يعمل على %1", "سێرڤەر لەسەر %1 کار دەکات"),
 "Server stopped": ("توقف الخادم", "سێرڤەر وەستا"),
 "Stopped a server of this project that was still running from the last time Studio closed unexpectedly.":
   ("أُوقف خادم لهذا المشروع كان لا يزال يعمل منذ أن أُغلق Studio آخر مرة بشكل غير متوقع.",
    "سێرڤەرێکی ئەم پڕۆژەیە وەستێنرا کە هێشتا لە دوایین داخستنی چاوەڕواننەکراوی Studio ـەوە کاری دەکرد."),
 "Log": ("السجل", "تۆمار"),
 "All": ("الكل", "هەموو"),
 "App log": ("سجل التطبيق", "تۆماری ئەپ"),
 "Problems only (%1)": ("المشكلات فقط (%1)", "تەنها کێشەکان (%1)"),
 "Problems only": ("المشكلات فقط", "تەنها کێشەکان"),
 "Search": ("بحث", "گەڕان"),
 "Search the log": ("البحث في السجل", "گەڕان لە تۆمارەکەدا"),
 "Copy": ("نسخ", "کۆپی"),
 "Clear": ("مسح", "پاککردنەوە"),
 "Open log file": ("فتح ملف السجل", "کردنەوەی فایلی تۆمار"),
 "Log lines": ("أسطر السجل", "دێڕەکانی تۆمار"),
 "Nothing logged yet. Start the server to see what it prints; FloCMS's own log appears as it is written.":
   ("لا شيء في السجل بعد. شغّل الخادم لترى ما يطبعه؛ ويظهر سجل FloCMS نفسه حين يُكتب.",
    "هێشتا هیچ تۆمار نەکراوە. سێرڤەر دەستپێبکە بۆ بینینی ئەوەی چاپی دەکات؛ تۆماری خودی FloCMS کاتێک دەنووسرێت دەردەکەوێت."),
 "No lines match.": ("لا توجد أسطر مطابقة.", "هیچ دێڕێک ناگونجێت."),
 "Newest": ("الأحدث", "نوێترین"),
 "Open site": ("فتح الموقع", "کردنەوەی ماڵپەڕ"),
 "Open admin": ("فتح لوحة الإدارة", "کردنەوەی پانێڵی بەڕێوەبردن"),
 "Stop server": ("إيقاف الخادم", "وەستاندنی سێرڤەر"),
 "Start server": ("تشغيل الخادم", "دەستپێکردنی سێرڤەر"),
 "Running": ("يعمل", "کار دەکات"),
 "Starting…": ("جارٍ التشغيل…", "دەستپێدەکات…"),
 "Stopping…": ("جارٍ الإيقاف…", "دەوەستێت…"),
 "Not running": ("لا يعمل", "کار ناکات"),
 "Stopped": ("متوقف", "وەستاوە"),
 "started just now": ("بدأ للتو", "ئێستا دەستیپێکرد"),
 "From other devices on your network:": ("من الأجهزة الأخرى على شبكتك:", "لە ئامێرەکانی تری تۆڕەکەتەوە:"),
 "Runs the site with `php flo serve` on port %1.": ("يشغّل الموقع بالأمر `php flo serve` على المنفذ %1.", "ماڵپەڕەکە بە `php flo serve` لەسەر پۆرتی %1 کار پێدەکات."),
 "The server did not start.": ("لم يبدأ الخادم.", "سێرڤەر دەستی پێنەکرد."),
 "The server said it started, but nothing answers at its address. Another program may hold the port: try another port in Settings > Server.":
   ("قال الخادم إنه بدأ، لكن لا شيء يجيب على عنوانه. ربما يشغل برنامج آخر المنفذ: جرّب منفذًا آخر في الإعدادات > الخادم.",
    "سێرڤەر گوتی دەستیپێکرد، بەڵام هیچ شتێک لە ناونیشانەکەیدا وەڵام ناداتەوە. لەوانەیە بەرنامەیەکی تر پۆرتەکەی گرتبێت: پۆرتێکی تر لە ڕێکخستنەکان > سێرڤەر تاقی بکەرەوە."),
 "The server stopped by itself.": ("توقف الخادم من تلقاء نفسه.", "سێرڤەر خۆی وەستا."),
 "PHP was not found. Download it in This computer, or choose it in Settings > PHP and tools.":
   ("لم يُعثر على PHP. نزّله من «هذا الحاسوب»، أو اختره في الإعدادات > PHP والأدوات.",
    "PHP نەدۆزرایەوە. لە «ئەم کۆمپیوتەرە» دایبگرە، یان لە ڕێکخستنەکان > PHP و ئامرازەکان هەڵیبژێرە."),
 "PHP is a `.bat` or `.cmd` file, and the project folder's name has characters `cmd.exe` would read itself ( ) & ! %. Choose `php.exe` in Settings > PHP and tools.":
   ("PHP ملف `.bat` أو `.cmd`، واسم مجلد المشروع فيه أحرف يقرؤها `cmd.exe` بنفسه ( ) & ! %. اختر `php.exe` في الإعدادات > PHP والأدوات.",
    "PHP فایلێکی `.bat` یان `.cmd` ـە و ناوی فۆڵدەری پڕۆژەکە پیتی تێدایە کە `cmd.exe` خۆی دەیانخوێنێتەوە ( ) & ! %. `php.exe` لە ڕێکخستنەکان > PHP و ئامرازەکان هەڵبژێرە."),
 "The project has no `flo` launcher yet: run `composer install` in it.":
   ("لا يحتوي المشروع على المشغّل `flo` بعد: شغّل `composer install` فيه.",
    "پڕۆژەکە هێشتا دەستپێکەری `flo` ـی نییە: `composer install` تێیدا جێبەجێ بکە."),
 "There is no `storage/logs/app.log` yet. FloCMS writes it at the first message.":
   ("لا يوجد `storage/logs/app.log` بعد. يكتبه FloCMS عند أول رسالة.",
    "هێشتا `storage/logs/app.log` نییە. FloCMS لە یەکەم پەیامدا دەینووسێت."),
}
P = {
 "started %Ln min ago": (
   ["started %Ln min ago", "started %Ln min ago"],
   ["بدأ قبل %Ln دقيقة", "بدأ قبل دقيقة", "بدأ قبل دقيقتين", "بدأ قبل %Ln دقائق", "بدأ قبل %Ln دقيقة", "بدأ قبل %Ln دقيقة"],
   ["%Ln خولەک لەمەوبەر دەستیپێکرد", "%Ln خولەک لەمەوبەر دەستیپێکرد"]),
}
