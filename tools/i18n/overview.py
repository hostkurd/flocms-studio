"""Arabic (ar) and Central Kurdish (ckb) for the Overview redesign (26 Oct)
and its Database and Doctor tiles. The first four were already translated
on the old Overview and moved with their card. Written by an agent: the
new ones need a native read ("quick commands" details, "migrations up to
date", Studio's MariaDB lines)."""

T = {
 # Moved from the old Overview's database card, same translations
 "Not set up yet": ("لم تُضبط بعد", "هێشتا ڕێکنەخراوە"),
 "from `DB_DSN`": ("من `DB_DSN`", "لە `DB_DSN` ـەوە"),
 "Set `DB_NAME` (and `DB_USERNAME` for MySQL) in `.env`. The Database tab will do this for you.":
   ("اضبط `DB_NAME` (و`DB_USERNAME` لـ MySQL) في `.env`. سيقوم تبويب قاعدة البيانات بذلك نيابة عنك.",
    "`DB_NAME` (و `DB_USERNAME` بۆ MySQL) لە `.env` دابنێ. بەشی داتابەیس ئەمە بۆت دەکات."),
 "`.env` sets %1 twice. The website uses the first value, some `php flo` commands the last: remove one.":
   ("يضبط `.env` المفتاح %1 مرتين. يستخدم الموقع القيمة الأولى، وبعض أوامر `php flo` القيمة الأخيرة: احذف إحداهما.",
    "`.env` دوو جار %1 دادەنێت. ماڵپەڕەکە یەکەم بەها بەکاردەهێنێت و هەندێک فەرمانی `php flo` دوایین بەها: یەکێکیان بسڕەوە."),
 # The Database tile
 "migrations up to date": ("الترحيلات محدّثة", "هیچ مایگرەیشنێک چاوەڕێ ناکات"),
 "Another MariaDB is running on port %1.": ("يعمل MariaDB آخر على المنفذ %1.", "MariaDBیەکی تر لەسەر پۆرتی %1 کار دەکات."),
 "Studio's MariaDB is running on port %1.": ("يعمل MariaDB الخاص بـ Studio على المنفذ %1.", "MariaDBی Studio لەسەر پۆرتی %1 کار دەکات."),
 "Studio's MariaDB didn't start.": ("لم يبدأ MariaDB الخاص بـ Studio.", "MariaDBی Studio دەستی پێنەکرد."),
 "Studio's MariaDB is stopped.": ("MariaDB الخاص بـ Studio متوقف.", "MariaDBی Studio وەستاوە."),
 "Database tab": ("تبويب قاعدة البيانات", "بەشی داتابەیس"),
 # The Doctor tile
 "All checks pass": ("كل الفحوص ناجحة", "هەموو پشکنینەکان سەرکەوتوون"),
 "Not checked yet": ("لم يُفحص بعد", "هێشتا نەپشکنراوە"),
 # Packages and quick commands
 "Packages tab": ("تبويب الحزم", "بەشی پاکێجەکان"),
 "A page in the admin panel": ("صفحة في لوحة الإدارة", "پەڕەیەک لە پانێڵی بەڕێوەبردن"),
 "A new table or a change to one": ("جدول جديد أو تعديل على جدول", "خشتەیەکی نوێ یان گۆڕانکاری لە خشتەیەک"),
}

P = {
 "and %Ln more": (
   ["and %Ln more", "and %Ln more"],
   ["ولا شيء آخر", "وواحد آخر", "واثنان آخران", "و%Ln أخرى", "و%Ln أخرى", "و%Ln أخرى"],
   ["و %Lnی تر", "و %Lnی تر"]),
}
