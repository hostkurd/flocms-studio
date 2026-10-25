"""Arabic (ar) and Central Kurdish (ckb) for Studio's own MariaDB server
(MariaDB design note, PR 3). Written by an agent: each needs a native read.
Word choices to check: "database server" (خادم قاعدة البيانات / سێرڤەری
داتابەیس), "start"/"stop" (تشغيل/إيقاف, دەستپێکردن/وەستاندن), "databases
folder" (مجلد قواعد البيانات / فۆڵدەری داتابەیسەکان), "secure store"."""

T = {
 # Settings › Database server (board 1, 2)
 "Database server": ("خادم قاعدة البيانات", "سێرڤەری داتابەیس"),
 "Server new projects use": ("الخادم الذي تستخدمه المشاريع الجديدة", "ئەو سێرڤەرەی پڕۆژە نوێیەکان بەکاری دەهێنن"),
 "Studio's MariaDB %1": ("MariaDB %1 من Studio", "MariaDB %1ی Studio"),
 "Started by Studio · port %1": ("يشغّله Studio · المنفذ %1", "Studio دەستی پێدەکات · پۆرتی %1"),
 "%1 · start and stop it in %2": ("%1 · يُشغَّل ويُوقَف من %2", "%1 · لە %2 دەستی پێبکە و بیوەستێنە"),
 "%1 · Studio doesn't start or stop it": ("%1 · لا يشغّله Studio ولا يوقفه", "%1 · Studio دەستی پێناکات و نایوەستێنێت"),
 "Another server": ("خادم آخر", "سێرڤەرێکی تر"),
 "Host, port and user you give": ("المضيف والمنفذ والمستخدم من اختيارك", "هۆست، پۆرت و بەکارهێنەر کە خۆت دەیدەیت"),
 "Existing projects keep the server in their `.env`.": ("تحتفظ المشاريع الموجودة بالخادم المكتوب في `.env` الخاص بها.", "پڕۆژە هەبووەکان ئەو سێرڤەرە دەهێڵنەوە کە لە `.env`ەکەیاندا هەیە."),
 "Studio can run its own MariaDB from Homebrew: install it with `brew install mariadb`, then search again.":
   ("يستطيع Studio تشغيل MariaDB خاص به من Homebrew: ثبّته بالأمر `brew install mariadb` ثم ابحث مجددًا.",
    "Studio دەتوانێت MariaDBی تایبەتی خۆی لە Homebrew ـەوە کار پێبکات: بە `brew install mariadb` دایبمەزرێنە، پاشان دووبارە بگەڕێ."),
 "Studio can download its own MariaDB and start and stop it for you. Its databases stay in a folder for your user.":
   ("يستطيع Studio تنزيل MariaDB خاص به وتشغيله وإيقافه نيابةً عنك. تبقى قواعد بياناته في مجلد خاص بمستخدمك.",
    "Studio دەتوانێت MariaDBی تایبەتی خۆی دابگرێت و بۆت دەستی پێبکات و بیوەستێنێت. داتابەیسەکانی لە فۆڵدەرێکی تایبەت بە بەکارهێنەرەکەت دەمێننەوە."),
 "Search again": ("البحث مجددًا", "دووبارە گەڕان"),
 "Download MariaDB…": ("تنزيل MariaDB…", "داگرتنی MariaDB…"),
 "Studio's MariaDB": ("MariaDB من Studio", "MariaDBی Studio"),
 "running since just now": ("يعمل منذ لحظات", "تازە دەستی پێکردووە"),
 "Open log": ("فتح السجل", "کردنەوەی تۆمار"),
 "Start": ("تشغيل", "دەستپێکردن"),
 "Stop": ("إيقاف", "وەستاندن"),
 "Starting…": ("جارٍ التشغيل…", "دەستپێدەکات…"),
 "Stopping…": ("جارٍ الإيقاف…", "دەوەستێت…"),
 "Version": ("الإصدار", "وەشان"),
 "Databases folder": ("مجلد قواعد البيانات", "فۆڵدەری داتابەیسەکان"),
 "Open folder": ("فتح المجلد", "کردنەوەی فۆڵدەر"),
 "Port": ("المنفذ", "پۆرت"),
 "Host": ("المضيف", "هۆست"),
 "User": ("المستخدم", "بەکارهێنەر"),
 "Used from the next start. Only this computer can connect (`127.0.0.1`).":
   ("يُستخدم من التشغيل التالي. لا يتصل به إلا هذا الحاسوب (`127.0.0.1`).",
    "لە دەستپێکردنی داهاتووەوە بەکاردێت. تەنها ئەم کۆمپیوتەرە دەتوانێت پەیوەندی پێوە بکات (`127.0.0.1`)."),
 "User `root`": ("المستخدم `root`", "بەکارهێنەری `root`"),
 "Hidden password": ("كلمة مرور مخفية", "وشەی نهێنیی شاراوە"),
 "Hide": ("إخفاء", "شاردنەوە"),
 "Show": ("إظهار", "پیشاندان"),
 "Copy": ("نسخ", "کۆپی"),
 "Kept in %1. For HeidiSQL or phpMyAdmin.": ("محفوظة في %1. لاستخدامها في HeidiSQL أو phpMyAdmin.", "لە %1دا هەڵگیراوە. بۆ HeidiSQL یان phpMyAdmin."),
 "Start the server when Studio opens": ("تشغيل الخادم عند فتح Studio", "دەستپێکردنی سێرڤەر کاتێک Studio دەکرێتەوە"),
 "Studio stops the server cleanly when it closes.": ("يوقف Studio الخادم بشكل سليم عند إغلاقه.", "Studio کاتێک دادەخرێت، سێرڤەرەکە بە پاکی دەوەستێنێت."),
 "Delete its databases…": ("حذف قواعد بياناته…", "سڕینەوەی داتابەیسەکانی…"),
 "Delete Studio's MariaDB and its databases?": ("حذف MariaDB الخاص بـ Studio وقواعد بياناته؟", "MariaDBی Studio و داتابەیسەکانی بسڕدرێنەوە؟"),
 "The program and every database in `%1` are deleted, with the password of `root`. Projects that use them stop working.":
   ("سيُحذف البرنامج وكل قاعدة بيانات في `%1`، مع كلمة مرور `root`. ستتوقف المشاريع التي تستخدمها عن العمل.",
    "بەرنامەکە و هەموو داتابەیسێک لە `%1` دەسڕدرێنەوە، لەگەڵ وشەی نهێنیی `root`. ئەو پڕۆژانەی بەکاریان دەهێنن کار ناکەن."),
 "Delete": ("حذف", "سڕینەوە"),
 "Remove MariaDB…": ("إزالة MariaDB…", "لابردنی MariaDB…"),
 "Remove Studio's MariaDB %1?": ("إزالة MariaDB %1 الخاص بـ Studio؟", "MariaDB %1ی Studio لاببرێت؟"),
 "The program is deleted. The databases stay in `%1`; download MariaDB %2 again to use them.":
   ("سيُحذف البرنامج. تبقى قواعد البيانات في `%1`؛ نزّل MariaDB %2 مجددًا لاستخدامها.",
    "بەرنامەکە دەسڕدرێتەوە. داتابەیسەکان لە `%1` دەمێننەوە؛ بۆ بەکارهێنانیان MariaDB %2 دووبارە دابگرە."),
 "Remove": ("إزالة", "لابردن"),

 # Database tab (board 4)
 "This project uses Studio's MariaDB (`127.0.0.1:%1`), which is stopped.":
   ("يستخدم هذا المشروع MariaDB الخاص بـ Studio (`127.0.0.1:%1`)، وهو متوقف.",
    "ئەم پڕۆژەیە MariaDBی Studio بەکاردەهێنێت (`127.0.0.1:%1`)، کە وەستاوە."),
 "Start the database server": ("تشغيل خادم قاعدة البيانات", "دەستپێکردنی سێرڤەری داتابەیس"),
 "Stop the database server": ("إيقاف خادم قاعدة البيانات", "وەستاندنی سێرڤەری داتابەیس"),

 # Downloads › MariaDB (board 3)
 "MariaDB %1 LTS": ("MariaDB %1 LTS", "MariaDB %1 LTS"),
 "Another version: MariaDB %1 LTS": ("إصدار آخر: MariaDB %1 LTS", "وەشانێکی تر: MariaDB %1 LTS"),
 "Newest · Studio starts and stops it · from %1": ("الأحدث · يشغّله Studio ويوقفه · من %1", "نوێترین · Studio دەستی پێدەکات و دەیوەستێنێت · لە %1"),
 "from %1": ("من %1", "لە %1"),
 "Downloaded": ("نُزّل", "داگیراوە"),
 "No MariaDB": ("بدون MariaDB", "بێ MariaDB"),
 "Only projects that use MySQL need one; SQLite needs nothing":
   ("تحتاجه فقط المشاريع التي تستخدم MySQL؛ SQLite لا يحتاج شيئًا",
    "تەنها ئەو پڕۆژانەی MySQL بەکاردەهێنن پێویستیان پێیەتی؛ SQLite هیچی ناوێت"),
 "Studio's MariaDB %1 is running on port %2.": ("MariaDB %1 من Studio يعمل على المنفذ %2.", "MariaDB %1ی Studio لەسەر پۆرتی %2 کار دەکات."),
 "Setting up Studio's MariaDB %1": ("جارٍ إعداد MariaDB %1 من Studio", "ئامادەکردنی MariaDB %1ی Studio"),
 "Studio's MariaDB %1 is stopped.": ("MariaDB %1 من Studio متوقف.", "MariaDB %1ی Studio وەستاوە."),
 "Studio's MariaDB %1 is downloaded.": ("نُزّل MariaDB %1 من Studio.", "MariaDB %1ی Studio داگیراوە."),
 "Set up and start": ("الإعداد والتشغيل", "ئامادەکردن و دەستپێکردن"),
 "Downloaded and checked against its `SHA-256`": ("نُزّل وفُحص مقابل `SHA-256` الخاص به", "داگیرا و بە `SHA-256`ەکەی پشکنرا"),
 "Databases folder made, `root` password saved in %1": ("أُنشئ مجلد قواعد البيانات، وحُفظت كلمة مرور `root` في %1", "فۆڵدەری داتابەیسەکان دروستکرا، وشەی نهێنیی `root` لە %1دا هەڵگیرا"),
 "Starting on port `%1`": ("جارٍ التشغيل على المنفذ `%1`", "دەستپێدەکات لەسەر پۆرتی `%1`"),
 "Password of `root` set": ("عُيّنت كلمة مرور `root`", "وشەی نهێنیی `root` دانرا"),

 # Problems (boards 2, 6)
 "Port `%1` is in use by another program. Studio can start its MariaDB on port `%2`.":
   ("المنفذ `%1` يستخدمه برنامج آخر. يستطيع Studio تشغيل MariaDB الخاص به على المنفذ `%2`.",
    "پۆرتی `%1` بەرنامەیەکی تر بەکاری دەهێنێت. Studio دەتوانێت MariaDBەکەی لەسەر پۆرتی `%2` دەست پێبکات."),
 "Port `%1` is in use by another program. Choose another port below.":
   ("المنفذ `%1` يستخدمه برنامج آخر. اختر منفذًا آخر أدناه.", "پۆرتی `%1` بەرنامەیەکی تر بەکاری دەهێنێت. لە خوارەوە پۆرتێکی تر هەڵبژێرە."),
 "It can't start without `libaio.so.1`. Install it once, then try again. Studio doesn't change system folders itself.":
   ("لا يمكنه العمل دون `libaio.so.1`. ثبّتها مرة واحدة ثم أعد المحاولة. لا يغيّر Studio مجلدات النظام بنفسه.",
    "بەبێ `libaio.so.1` دەست پێناکات. یەک جار دایبمەزرێنە، پاشان دووبارە هەوڵ بدەوە. Studio خۆی فۆڵدەرەکانی سیستەم ناگۆڕێت."),
 "It can't start without `%1`. Install it with your package manager, then try again.":
   ("لا يمكنه العمل دون `%1`. ثبّتها بمدير الحزم ثم أعد المحاولة.", "بەبێ `%1` دەست پێناکات. بە بەڕێوەبەری پاکێجەکان دایبمەزرێنە، پاشان دووبارە هەوڵ بدەوە."),
 "Ubuntu 24.04 and newer call the file `libaio.so.1t64`; Studio points its own copy of MariaDB at it, inside Studio's folder. Other systems: the `libaio` package.":
   ("يسمّي Ubuntu 24.04 وما بعده الملف `libaio.so.1t64`؛ يوجّه Studio نسخته من MariaDB إليه، داخل مجلد Studio. الأنظمة الأخرى: الحزمة `libaio`.",
    "Ubuntu 24.04 و نوێترەکان ناوی فایلەکە دەنێن `libaio.so.1t64`؛ Studio کۆپیی MariaDBەکەی خۆی ئاراستەی ئەو دەکات، لەناو فۆڵدەری Studio. سیستەمەکانی تر: پاکێجی `libaio`."),
 "Start on `%1`": ("التشغيل على `%1`", "دەستپێکردن لەسەر `%1`"),
 "Try again": ("أعد المحاولة", "دووبارە هەوڵبدەوە"),

 # Texts: states and stores
 "Running": ("يعمل", "کار دەکات"),
 "Stopped": ("متوقف", "وەستاوە"),
 "Didn't start": ("لم يبدأ", "دەستی پێنەکرد"),
 "Windows Credential Manager": ("مدير بيانات الاعتماد في Windows", "بەڕێوەبەری بڕوانامەکانی Windows"),
 "the macOS Keychain": ("سلسلة مفاتيح macOS", "Keychainی macOS"),
 "the system's keyring": ("حلقة مفاتيح النظام", "Keyringی سیستەم"),

 # Texts: error messages
 "The port is in use by another program, so Studio's MariaDB can't start on it.":
   ("المنفذ يستخدمه برنامج آخر، لذا لا يمكن تشغيل MariaDB الخاص بـ Studio عليه.", "پۆرتەکە بەرنامەیەکی تر بەکاری دەهێنێت، بۆیە MariaDBی Studio ناتوانێت لەسەری دەست پێبکات."),
 "MariaDB needs a library this system doesn't have.": ("يحتاج MariaDB إلى مكتبة غير موجودة في هذا النظام.", "MariaDB پێویستی بە کتێبخانەیەکە کە ئەم سیستەمە نییەتی."),
 "Studio's MariaDB doesn't run on this computer.": ("لا يعمل MariaDB الخاص بـ Studio على هذا الحاسوب.", "MariaDBی Studio لەسەر ئەم کۆمپیوتەرە کار ناکات."),
 "Studio could not set up the databases folder.": ("تعذّر على Studio إعداد مجلد قواعد البيانات.", "Studio نەیتوانی فۆڵدەری داتابەیسەکان ئامادە بکات."),
 "MariaDB stopped while starting. The end of its log:": ("توقف MariaDB أثناء التشغيل. نهاية سجله:", "MariaDB لە کاتی دەستپێکردندا وەستا. کۆتایی تۆمارەکەی:"),
 "MariaDB didn't answer in time. The end of its log:": ("لم يستجب MariaDB في الوقت المحدد. نهاية سجله:", "MariaDB لە کاتی خۆیدا وەڵامی نەدایەوە. کۆتایی تۆمارەکەی:"),
 "Studio could not set the password of MariaDB's `root` user.": ("تعذّر على Studio تعيين كلمة مرور المستخدم `root` في MariaDB.", "Studio نەیتوانی وشەی نهێنیی بەکارهێنەری `root`ی MariaDB دابنێت."),
 "The password of `root` is missing from the system's secure store, so Studio can't use these databases. Delete the databases in Settings to start again.":
   ("كلمة مرور `root` غير موجودة في المخزن الآمن للنظام، لذا لا يستطيع Studio استخدام قواعد البيانات هذه. احذف قواعد البيانات من الإعدادات للبدء من جديد.",
    "وشەی نهێنیی `root` لە شوێنی پارێزراوی سیستەمدا نییە، بۆیە Studio ناتوانێت ئەم داتابەیسانە بەکاربهێنێت. بۆ دەستپێکردنەوە، داتابەیسەکان لە ڕێکخستنەکان بسڕەوە."),
 "Studio could not use the system's secure store for the password of `root`.": ("تعذّر على Studio استخدام المخزن الآمن للنظام لكلمة مرور `root`.", "Studio نەیتوانی شوێنی پارێزراوی سیستەم بۆ وشەی نهێنیی `root` بەکاربهێنێت."),
 "MariaDB stopped unexpectedly. The end of its log:": ("توقف MariaDB بشكل غير متوقع. نهاية سجله:", "MariaDB لەناکاو وەستا. کۆتایی تۆمارەکەی:"),
}

T.update({
 "Port `%1` is in use by another program. Choose another port in Settings.":
   ("المنفذ `%1` يستخدمه برنامج آخر. اختر منفذًا آخر في الإعدادات.", "پۆرتی `%1` بەرنامەیەکی تر بەکاری دەهێنێت. لە ڕێکخستنەکان پۆرتێکی تر هەڵبژێرە."),
 "Delete the databases of Studio's MariaDB?": ("حذف قواعد بيانات MariaDB الخاص بـ Studio؟", "داتابەیسەکانی MariaDBی Studio بسڕدرێنەوە؟"),
 "Every database in `%1` is deleted, with the password of `root`. Homebrew's MariaDB stays. Projects that use them stop working.":
   ("ستُحذف كل قاعدة بيانات في `%1`، مع كلمة مرور `root`. يبقى MariaDB الخاص بـ Homebrew. ستتوقف المشاريع التي تستخدمها عن العمل.",
    "هەموو داتابەیسێک لە `%1` دەسڕدرێتەوە، لەگەڵ وشەی نهێنیی `root`. MariaDBی Homebrew دەمێنێتەوە. ئەو پڕۆژانەی بەکاریان دەهێنن کار ناکەن."),
})

P = {
 "running for %Ln min": (
   ["running for %Ln min", "running for %Ln min"],
   ["يعمل منذ %Ln دقيقة", "يعمل منذ دقيقة", "يعمل منذ دقيقتين", "يعمل منذ %Ln دقائق", "يعمل منذ %Ln دقيقة", "يعمل منذ %Ln دقيقة"],
   ["%Ln خولەکە کار دەکات", "%Ln خولەکە کار دەکات"]),
}
