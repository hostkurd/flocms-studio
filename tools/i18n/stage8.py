"""Arabic (ar) and Central Kurdish (ckb) for the strings stage 8 added (the
Packages tab).

Each needs a native read (listed in the PR). Words used before keep their
translation: الحزمة / پاکێج (package), لوحة الإدارة / پانێڵی بەڕێوەبردن
(admin panel), ترحيل / مایگرەیشن. Filled into app/i18n with
tools/fill_translations.py.
"""

T = {
 "FloCMS packages": ("حزم FloCMS", "پاکێجەکانی FloCMS"),
 "Check for updates": ("البحث عن تحديثات", "پشکنینی نوێکردنەوەکان"),
 "Everything is up to date.": ("كل شيء محدّث.", "هەموو شتێک نوێیە."),
 "Checked just now.": ("فُحص للتو.", "ئێستا پشکنرا."),
 "Installed": ("المثبّت", "دامەزراو"),
 "Latest": ("الأحدث", "نوێترین"),
 "Not installed": ("غير مثبّت", "دانەمەزراوە"),
 "Update": ("تحديث", "نوێکردنەوە"),
 "New major version": ("إصدار رئيسي جديد", "وەشانی سەرەکیی نوێ"),
 "Through another package": ("عبر حزمة أخرى", "لە ڕێگەی پاکێجێکی ترەوە"),
 "Review update": ("مراجعة التحديث", "پێداچوونەوەی نوێکردنەوە"),
 "Add the API": ("إضافة الـAPI", "زیادکردنی API"),
 "Add the admin panel": ("إضافة لوحة الإدارة", "زیادکردنی پانێڵی بەڕێوەبردن"),
 "Other packages (%1)": ("حزم أخرى (%1)", "پاکێجەکانی تر (%1)"),
 "Add the JSON API": ("إضافة JSON API", "زیادکردنی JSON API"),
 "Update %1": ("تحديث %1", "نوێکردنەوەی %1"),
 "A new major version: it may change how things work. Read the changelog first.":
   ("إصدار رئيسي جديد: قد يغيّر طريقة عمل الأشياء. اقرأ سجل التغييرات أولًا.",
    "وەشانێکی سەرەکیی نوێ: لەوانەیە شێوەی کارکردنی شتەکان بگۆڕێت. سەرەتا تۆماری گۆڕانکارییەکان بخوێنەوە."),
 "Read the changelog": ("قراءة سجل التغييرات", "خوێندنەوەی تۆماری گۆڕانکارییەکان"),
 "Read UPGRADING.md": ("قراءة UPGRADING.md", "خوێندنەوەی UPGRADING.md"),
 "After Composer adds it": ("بعد أن يضيفها Composer", "دوای ئەوەی Composer زیادی دەکات"),
 "After the update": ("بعد التحديث", "دوای نوێکردنەوە"),
 "Studio runs `admin:install`, which only adds what is missing, and then `migrate`.":
   ("يشغّل Studio الأمر `admin:install`، الذي يضيف الناقص فقط، ثم `migrate`.",
    "Studio فەرمانی `admin:install` جێبەجێ دەکات، کە تەنها ئەوەی کەمە زیادی دەکات، پاشان `migrate`."),
 "Studio runs `api:install`, which only adds what is missing, and then `migrate`.":
   ("يشغّل Studio الأمر `api:install`، الذي يضيف الناقص فقط، ثم `migrate`.",
    "Studio فەرمانی `api:install` جێبەجێ دەکات، کە تەنها ئەوەی کەمە زیادی دەکات، پاشان `migrate`."),
 "Studio runs `migrate` for the package's new tables.":
   ("يشغّل Studio الأمر `migrate` لجداول الحزمة الجديدة.", "Studio فەرمانی `migrate` بۆ خشتە نوێیەکانی پاکێجەکە جێبەجێ دەکات."),
 "`APP_ENV` says this is a production site: `migrate` changes its database.":
   ("يقول `APP_ENV` إن هذا موقع إنتاج: سيغيّر `migrate` قاعدة بياناته.", "`APP_ENV` دەڵێت ئەمە ماڵپەڕێکی بەرهەمهێنانە: `migrate` داتابەیسەکەی دەگۆڕێت."),
 "Commands": ("الأوامر", "فەرمانەکان"),
 "Added. The project's tabs show it now.": ("أُضيفت. تعرضها تبويبات المشروع الآن.", "زیادکرا. تابەکانی پڕۆژەکە ئێستا پیشانی دەدەن."),
 "Updated to %1.": ("حُدّثت إلى %1.", "نوێکرایەوە بۆ %1."),
 "A command failed (exit code %1). What ran before it stays done.":
   ("فشل أمر (رمز الخروج %1). ما نُفّذ قبله يبقى منجزًا.", "فەرمانێک سەرکەوتوو نەبوو (کۆدی دەرچوون %1). ئەوەی پێشتر جێبەجێ کرا وەک خۆی دەمێنێتەوە."),
 "Add": ("إضافة", "زیادکردن"),
 "Composer could not say what is newer. It asks Packagist, so this needs the internet.":
   ("تعذّر على Composer معرفة ما هو أحدث. فهو يسأل Packagist، لذا يحتاج هذا إلى الإنترنت.",
    "Composer نەیتوانی بڵێت چی نوێترە. پرسیار لە Packagist دەکات، بۆیە پێویستی بە ئینتەرنێتە."),
}

P = {
 "%Ln update(s).": (
   ["%Ln update.", "%Ln updates."],
   ["لا تحديثات.", "تحديث واحد.", "تحديثان.", "%Ln تحديثات.", "%Ln تحديثًا.", "%Ln تحديث."],
   ["%Ln نوێکردنەوە.", "%Ln نوێکردنەوە."]),
 "Checked %Ln min ago.": (
   ["Checked %Ln min ago.", "Checked %Ln min ago."],
   ["فُحص قبل %Ln دقيقة.", "فُحص قبل دقيقة.", "فُحص قبل دقيقتين.", "فُحص قبل %Ln دقائق.", "فُحص قبل %Ln دقيقة.", "فُحص قبل %Ln دقيقة."],
   ["پێش %Ln خولەک پشکنرا.", "پێش %Ln خولەک پشکنرا."]),
}
