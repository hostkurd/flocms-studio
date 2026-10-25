"""Arabic (ar) and Central Kurdish (ckb) for the strings of the UI fixes
after stage 8 (the downloads dialog). Each needs a native read."""

T = {
 "this computer has %1": ("هذا الحاسوب فيه %1", "ئەم کۆمپیوتەرە %1ی هەیە"),
}

# The downloads dialog by tool (PHP, Composer, MariaDB)
T.update({
 "Tool": ("الأداة", "ئامراز"),
 "On this computer": ("على هذا الحاسوب", "لەسەر ئەم کۆمپیوتەرە"),
 "Not found on this computer": ("غير موجود على هذا الحاسوب", "لەسەر ئەم کۆمپیوتەرە نەدۆزرایەوە"),
 "Downloaded by Studio": ("نزّله Studio", "Studio دایگرتووە"),
 "From XAMPP": ("من XAMPP", "لە XAMPP ـەوە"),
 "From Laragon": ("من Laragon", "لە Laragon ـەوە"),
 "From Herd": ("من Herd", "لە Herd ـەوە"),
 "Installed on this computer": ("مثبّت على هذا الحاسوب", "لەسەر ئەم کۆمپیوتەرە دامەزراوە"),
 "Upgrade to Composer %1": ("الترقية إلى Composer %1", "بەرزکردنەوە بۆ Composer %1"),
 "Already downloaded": ("نُزّل مسبقًا", "پێشتر داگیراوە"),
 "This computer has the same or a newer version": ("في هذا الحاسوب الإصدار نفسه أو أحدث", "ئەم کۆمپیوتەرە هەمان وەشان یان نوێتری هەیە"),
 "Studio's own copy, used instead of %1; that one stays as it is · from %2":
   ("نسخة Studio الخاصة، تُستخدم بدل %1، الذي يبقى كما هو · من %2", "کۆپیی تایبەتی Studio، لە جیاتی %1 بەکاردێت؛ ئەویان وەک خۆی دەمێنێتەوە · لە %2"),
 "Another version: MariaDB %1": ("إصدار آخر: MariaDB %1", "وەشانێکی تر: MariaDB %1"),
 "Studio's own, next to the one this computer has; that one stays as it is · from %1":
   ("نسخة Studio الخاصة، بجانب الموجودة على هذا الحاسوب، التي تبقى كما هي · من %1",
    "هی تایبەتی Studio، لە تەنیشت ئەوەی ئەم کۆمپیوتەرە هەیەتی؛ ئەویان وەک خۆی دەمێنێتەوە · لە %1"),
 "MariaDB makes no builds for macOS. Install it with Homebrew: `brew install mariadb`.":
   ("لا تصدر MariaDB نسخًا لنظام macOS. ثبّتها عبر Homebrew: `brew install mariadb`.",
    "MariaDB هیچ وەشانێک بۆ macOS دروست ناکات. بە Homebrew دایبمەزرێنە: `brew install mariadb`."),
 "Studio can't download MariaDB for this system yet. Install it with your package manager, for example `sudo apt install mariadb-server`.":
   ("لا يستطيع Studio تنزيل MariaDB لهذا النظام بعد. ثبّتها بمدير الحزم، مثلًا `sudo apt install mariadb-server`.",
    "Studio هێشتا ناتوانێت MariaDB بۆ ئەم سیستەمە دابگرێت. بە بەڕێوەبەری پاکێجەکان دایبمەزرێنە، بۆ نموونە `sudo apt install mariadb-server`."),
})
