"""Arabic (ar) and Central Kurdish (ckb) for "Remove from Studio" and
"Delete project…" (27 Oct). Written by an agent: they need a native read
(Recycle Bin / Trash wording, the database lines)."""

T = {
 "Recycle Bin": ("سلة المحذوفات", "سەبەتەی خۆڵ"),
 "Trash": ("سلة المهملات", "سەبەتەی خۆڵ"),
 "Delete %1?": ("حذف %1؟", "%1 بسڕدرێتەوە؟"),
 "Its folder goes to the %1, so you can still bring it back from there. Its website server stops, and the project leaves Studio.":
   ("ينتقل مجلده إلى %1، فيمكنك استعادته من هناك. يتوقف خادم موقعه، ويُزال المشروع من Studio.",
    "بوخچەکەی دەچێتە %1، بۆیە هێشتا دەتوانیت لەوێوە بیگەڕێنیتەوە. سێرڤەری ماڵپەڕەکەی دەوەستێت و پڕۆژەکە لە Studio لادەبرێت."),
 "Also delete its database on Studio's MariaDB": ("احذف أيضًا قاعدة بياناته على MariaDB الخاص بـ Studio", "داتابەیسەکەشی لەسەر MariaDBی Studio بسڕەوە"),
 "The database `%1`, with all its data. The user `%2` stays: another project uses it.":
   ("قاعدة البيانات `%1` بكل بياناتها. يبقى المستخدم `%2`: يستخدمه مشروع آخر.",
    "داتابەیسی `%1` لەگەڵ هەموو داتاکانی. بەکارهێنەری `%2` دەمێنێتەوە: پڕۆژەیەکی تر بەکاری دەهێنێت."),
 "The database `%1`, with all its data, and its user `%2`.":
   ("قاعدة البيانات `%1` بكل بياناتها، ومستخدمها `%2`.", "داتابەیسی `%1` لەگەڵ هەموو داتاکانی، و بەکارهێنەرەکەی `%2`."),
 "This can't be undone.": ("لا يمكن التراجع عن هذا.", "ئەمە ناگەڕێتەوە."),
 "Cancel": ("إلغاء", "هەڵوەشاندنەوە"),
 "Delete project": ("حذف المشروع", "سڕینەوەی پڕۆژە"),
 "Remove from Studio": ("إزالة من Studio", "لابردن لە Studio"),
 "Delete project…": ("حذف المشروع…", "سڕینەوەی پڕۆژە…"),
 "Deleting…": ("جارٍ الحذف…", "دەسڕدرێتەوە…"),
 "More": ("المزيد", "زیاتر"),
 "More actions for this project": ("إجراءات أخرى لهذا المشروع", "کاری زیاتر بۆ ئەم پڕۆژەیە"),
 "A command is still running in this project. Remove or delete it when it has finished.":
   ("ما زال أمر يعمل في هذا المشروع. أزِله أو احذفه بعد أن ينتهي.",
    "هێشتا فەرمانێک لەم پڕۆژەیەدا کار دەکات. کاتێک تەواو بوو لای ببە یان بیسڕەوە."),
 "Studio doesn't delete this folder: it is or holds a folder Studio or your computer needs.":
   ("لا يحذف Studio هذا المجلد: فهو مجلد يحتاجه Studio أو حاسوبك، أو يحوي مجلدًا كهذا.",
    "Studio ئەم بوخچەیە ناسڕێتەوە: خۆی یان بوخچەیەکی ناوی پێویستی Studio یان کۆمپیوتەرەکەتە."),
 "The folder could not be moved to the Recycle Bin. A program may be using a file in it, or the drive has no Recycle Bin. Nothing was deleted.":
   ("تعذّر نقل المجلد إلى سلة المحذوفات. ربما يستخدم برنامج ملفًا فيه، أو لا توجد سلة محذوفات على القرص. لم يُحذف شيء.",
    "بوخچەکە نەگوازرایەوە بۆ سەبەتەی خۆڵ. لەوانەیە بەرنامەیەک فایلێکی ناوی بەکاربهێنێت، یان ئەو دیسکە سەبەتەی خۆڵی نییە. هیچ شتێک نەسڕایەوە."),
 "The folder could not be moved to the Trash. A program may be using a file in it, or the disk has no Trash. Nothing was deleted.":
   ("تعذّر نقل المجلد إلى سلة المهملات. ربما يستخدم برنامج ملفًا فيه، أو لا توجد سلة مهملات على القرص. لم يُحذف شيء.",
    "بوخچەکە نەگوازرایەوە بۆ سەبەتەی خۆڵ. لەوانەیە بەرنامەیەک فایلێکی ناوی بەکاربهێنێت، یان ئەو دیسکە سەبەتەی خۆڵی نییە. هیچ شتێک نەسڕایەوە."),
 "The project was deleted, but Studio could not delete its database on its MariaDB. You can delete it with a database tool (HeidiSQL, DBeaver…), as `root`: Settings > Database server has its password.":
   ("حُذف المشروع، لكن Studio لم يتمكن من حذف قاعدة بياناته على MariaDB الخاص به. يمكنك حذفها بأداة قواعد بيانات (HeidiSQL وDBeaver…) بصفة `root`: كلمة مروره في الإعدادات > خادم قاعدة البيانات.",
    "پڕۆژەکە سڕایەوە، بەڵام Studio نەیتوانی داتابەیسەکەی لەسەر MariaDBـەکەی بسڕێتەوە. دەتوانیت بە ئامرازێکی داتابەیس (HeidiSQL، DBeaver…) وەک `root` بیسڕیتەوە: وشەی نهێنییەکەی لە ڕێکخستنەکان > سێرڤەری داتابەیس هەیە."),
}

P = {}
