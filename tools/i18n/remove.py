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
  "The folder could not be moved to the Recycle Bin. A program may be using a file in it, or the drive has no Recycle Bin. Nothing was deleted.":
   ("تعذّر نقل المجلد إلى سلة المحذوفات. ربما يستخدم برنامج ملفًا فيه، أو لا توجد سلة محذوفات على القرص. لم يُحذف شيء.",
    "بوخچەکە نەگوازرایەوە بۆ سەبەتەی خۆڵ. لەوانەیە بەرنامەیەک فایلێکی ناوی بەکاربهێنێت، یان ئەو دیسکە سەبەتەی خۆڵی نییە. هیچ شتێک نەسڕایەوە."),
 "The folder could not be moved to the Trash. A program may be using a file in it, or the disk has no Trash. Nothing was deleted.":
   ("تعذّر نقل المجلد إلى سلة المهملات. ربما يستخدم برنامج ملفًا فيه، أو لا توجد سلة مهملات على القرص. لم يُحذف شيء.",
    "بوخچەکە نەگوازرایەوە بۆ سەبەتەی خۆڵ. لەوانەیە بەرنامەیەک فایلێکی ناوی بەکاربهێنێت، یان ئەو دیسکە سەبەتەی خۆڵی نییە. هیچ شتێک نەسڕایەوە."),
 "The project was deleted, but Studio could not delete its database on its MariaDB. You can delete it with a database tool (HeidiSQL, DBeaver…), as `root`: Settings > Database server has its password.":
   ("حُذف المشروع، لكن Studio لم يتمكن من حذف قاعدة بياناته على MariaDB الخاص به. يمكنك حذفها بأداة قواعد بيانات (HeidiSQL وDBeaver…) بصفة `root`: كلمة مروره في الإعدادات > خادم قاعدة البيانات.",
    "پڕۆژەکە سڕایەوە، بەڵام Studio نەیتوانی داتابەیسەکەی لەسەر MariaDBـەکەی بسڕێتەوە. دەتوانیت بە ئامرازێکی داتابەیس (HeidiSQL، DBeaver…) وەک `root` بیسڕیتەوە: وشەی نهێنییەکەی لە ڕێکخستنەکان > سێرڤەری داتابەیس هەیە."),
 "The database `%1`, with all its data; its user `%2` stays.":
   ("قاعدة البيانات `%1` بكل بياناتها؛ ويبقى مستخدمها `%2`.", "داتابەیسی `%1` لەگەڵ هەموو داتاکانی؛ بەکارهێنەرەکەی `%2` دەمێنێتەوە."),
 "Its database `%1` stays: another project uses it.":
   ("تبقى قاعدة بياناته `%1`: يستخدمها مشروع آخر.", "داتابەیسەکەی `%1` دەمێنێتەوە: پڕۆژەیەکی تر بەکاری دەهێنێت."),
 "Its database stays as it is.": ("تبقى قاعدة بياناته كما هي.", "داتابەیسەکەی وەک خۆی دەمێنێتەوە."),
 "Studio doesn't delete this folder: it is, holds or is inside a folder Studio or your computer needs (Studio's own files, your home or Documents folder, a project folder or another project).":
   ("لا يحذف Studio هذا المجلد: فهو مجلد يحتاجه Studio أو حاسوبك، أو يحويه أو يقع داخله (ملفات Studio نفسه، أو مجلد المنزل أو المستندات، أو مجلد مشاريع، أو مشروع آخر).",
    "Studio ئەم بوخچەیە ناسڕێتەوە: خۆی، یان بوخچەیەکی ناوی، یان ئەو بوخچەیەی تێیدایە پێویستی Studio یان کۆمپیوتەرەکەتە (فایلەکانی خودی Studio، بوخچەی ماڵەوە یان Documents، بوخچەی پڕۆژەکان، یان پڕۆژەیەکی تر)."),
 "This project's folder is a link to another folder, so Studio doesn't delete it. Delete the folder itself, then remove the project from Studio.":
   ("مجلد هذا المشروع رابط إلى مجلد آخر، لذا لا يحذفه Studio. احذف المجلد نفسه، ثم أزِل المشروع من Studio.",
    "بوخچەی ئەم پڕۆژەیە بەستەرێکە بۆ بوخچەیەکی تر، بۆیە Studio نایسڕێتەوە. خودی بوخچەکە بسڕەوە، پاشان پڕۆژەکە لە Studio لاببە."),
 "Studio is still deleting another project. Try again when it has finished.":
   ("ما زال Studio يحذف مشروعًا آخر. حاول مجددًا بعد أن ينتهي.", "Studio هێشتا پڕۆژەیەکی تر دەسڕێتەوە. کاتێک تەواو بوو دووبارە هەوڵ بدەرەوە."),
 "The project's website server didn't stop, so its folder wasn't deleted. Stop the server, then try again.":
   ("لم يتوقف خادم موقع المشروع، فلم يُحذف مجلده. أوقف الخادم ثم حاول مجددًا.", "سێرڤەری ماڵپەڕی پڕۆژەکە نەوەستا، بۆیە بوخچەکەی نەسڕایەوە. سێرڤەرەکە بوەستێنە، پاشان دووبارە هەوڵ بدەرەوە."),
}

P = {}
