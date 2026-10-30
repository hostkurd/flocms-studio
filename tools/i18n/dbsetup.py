"""Arabic (ar) and Central Kurdish (ckb) for 0.1.4 (design note of 30 Oct):
a missing database in the wizard (create it or skip the database steps),
the server per project, the Database tab's set-up, and Open terminal.
Written by an agent: they need a native read ("skip" تخطي / تێپەڕاندن,
"terminal" الطرفية / تێرمیناڵ, "set up" إعداد / ڕێکخستن)."""

T = {
 # Overview card and the Database tab's set-up (part C)
 "This project has no database yet. Studio can make one and write the `DB_` lines into `.env`.":
   ("لا توجد لهذا المشروع قاعدة بيانات بعد. يستطيع Studio إنشاء واحدة وكتابة أسطر `DB_` في `.env`.",
    "ئەم پڕۆژەیە هێشتا داتابەیسی نییە. Studio دەتوانێت یەکێک دروست بکات و دێڕەکانی `DB_` لە `.env` بنووسێت."),
 "Set up database": ("إعداد قاعدة البيانات", "داتابەیس ڕێکبخە"),
 "This project has no database yet: no `DB_NAME` in `.env`. Studio can make one and write the `DB_` lines for you.":
   ("لا توجد لهذا المشروع قاعدة بيانات بعد: لا يوجد `DB_NAME` في `.env`. يستطيع Studio إنشاء واحدة وكتابة أسطر `DB_` نيابةً عنك.",
    "ئەم پڕۆژەیە هێشتا داتابەیسی نییە: هیچ `DB_NAME`ێک لە `.env` نییە. Studio دەتوانێت یەکێک دروست بکات و دێڕەکانی `DB_` بۆت بنووسێت."),
 "On Studio's MariaDB (`127.0.0.1:%1`). Studio makes the user's password and keeps it in %2.":
   ("على MariaDB الخاص بـ Studio (`127.0.0.1:%1`). ينشئ Studio كلمة مرور المستخدم ويحفظها في %2.",
    "لەسەر MariaDBی Studio (`127.0.0.1:%1`). Studio وشەی نهێنیی بەکارهێنەرەکە دروست دەکات و لە %2 هەڵیدەگرێت."),
 "Studio's MariaDB is stopped: Studio starts it first (port `%1`). It makes the user's password and keeps it in %2.":
   ("MariaDB الخاص بـ Studio متوقف: سيشغّله Studio أولًا (المنفذ `%1`). وينشئ كلمة مرور المستخدم ويحفظها في %2.",
    "MariaDBی Studio وەستاوە: Studio یەکەم جار دەیخاتە کار (پۆرتی `%1`). وشەی نهێنیی بەکارهێنەرەکە دروست دەکات و لە %2 هەڵیدەگرێت."),
 "The database may exist already; if not, Studio offers to create it.":
   ("قد تكون قاعدة البيانات موجودة؛ وإن لم تكن، يعرض Studio إنشاءها.",
    "لەوانەیە داتابەیسەکە هەبێت؛ ئەگەر نەبوو، Studio پێشنیاری دروستکردنی دەکات."),
 "Setting up…": ("جارٍ الإعداد…", "ڕێکدەخرێت…"),
 "Starting Studio's MariaDB…": ("جارٍ تشغيل MariaDB الخاص بـ Studio…", "MariaDBی Studio دەخرێتە کار…"),
 "Creating the database…": ("جارٍ إنشاء قاعدة البيانات…", "داتابەیسەکە دروست دەکرێت…"),
 "Checking the connection…": ("جارٍ فحص الاتصال…", "پەیوەندییەکە دەپشکنرێت…"),
 "The server on `%1:%2` answered, but it has no database called `%3`. Studio can create it with this login.":
   ("استجاب الخادم على `%1:%2`، لكن لا توجد عليه قاعدة بيانات باسم `%3`. يستطيع Studio إنشاءها ببيانات الدخول هذه.",
    "سێرڤەر لەسەر `%1:%2` وەڵامی دایەوە، بەڵام داتابەیسێکی بە ناوی `%3` نییە. Studio دەتوانێت بەم چوونەژوورەوەیە دروستی بکات."),
 "Create database “%1”": ("إنشاء قاعدة البيانات «%1»", "داتابەیسی «%1» دروست بکە"),
 "The migrations and tables show here once the database is set up.":
   ("تظهر الترحيلات والجداول هنا بعد إعداد قاعدة البيانات.",
    "مایگرەیشنەکان و خشتەکان دوای ڕێکخستنی داتابەیسەکە لێرە دەردەکەون."),
 # The server per project (part D)
 "Default server for new projects": ("الخادم الافتراضي للمشاريع الجديدة", "سێرڤەری بنەڕەتی بۆ پڕۆژە نوێیەکان"),
 "Each project can pick its own when it is made or set up. Existing projects keep the server in their `.env`.":
   ("يمكن لكل مشروع اختيار خادمه عند إنشائه أو إعداده. تحتفظ المشاريع الموجودة بالخادم المذكور في `.env` الخاص بها.",
    "هەر پڕۆژەیەک دەتوانێت لە کاتی دروستکردن یان ڕێکخستندا سێرڤەری خۆی هەڵبژێرێت. پڕۆژە هەبووەکان سێرڤەرەکەی ناو `.env`ی خۆیان دەهێڵنەوە."),
 "For this project only. The default for new projects is in Settings.":
   ("لهذا المشروع فقط. الافتراضي للمشاريع الجديدة في الإعدادات.",
    "تەنها بۆ ئەم پڕۆژەیە. بنەڕەتی پڕۆژە نوێیەکان لە ڕێکخستنەکاندایە."),
 "If it doesn't exist yet, Studio offers to create it when it makes the project.":
   ("إن لم تكن موجودة بعد، يعرض Studio إنشاءها عندما ينشئ المشروع.",
    "ئەگەر هێشتا نەبوو، Studio لە کاتی دروستکردنی پڕۆژەکەدا پێشنیاری دروستکردنی دەکات."),
 # The wizard's step 4 (part A)
 "Skipped": ("تم تخطيها", "تێپەڕێنرا"),
 "The server on `%1:%2` answered, but it has no database called `%3`. Studio can create it with the login from step 2, then go on.":
   ("استجاب الخادم على `%1:%2`، لكن لا توجد عليه قاعدة بيانات باسم `%3`. يستطيع Studio إنشاءها ببيانات الدخول من الخطوة ٢ ثم المتابعة.",
    "سێرڤەر لەسەر `%1:%2` وەڵامی دایەوە، بەڵام داتابەیسێکی بە ناوی `%3` نییە. Studio دەتوانێت بە چوونەژوورەوەی هەنگاوی ٢ دروستی بکات و بەردەوام بێت."),
 "You can also finish the project without its database steps and set the database up later.":
   ("يمكنك أيضًا إنهاء المشروع دون خطوات قاعدة البيانات وإعدادها لاحقًا.",
    "دەتوانیت پڕۆژەکە بەبێ هەنگاوەکانی داتابەیس تەواو بکەیت و داتابەیسەکە دواتر ڕێکبخەیت."),
 "Skip database steps…": ("تخطي خطوات قاعدة البيانات…", "هەنگاوەکانی داتابەیس تێپەڕێنە…"),
 "Skip the database steps?": ("تخطي خطوات قاعدة البيانات؟", "هەنگاوەکانی داتابەیس تێپەڕێنرێن؟"),
 "The project is made, but its database isn't ready: no tables and no admin user. Create the database `%1` (or correct the `DB_` lines in `.env`), then run the migrations on the Database tab and create the admin with `php flo user:create` in a terminal (More, Open terminal).":
   ("يُنشأ المشروع، لكن قاعدة بياناته ليست جاهزة: لا جداول ولا مستخدم مسؤول. أنشئ قاعدة البيانات `%1` (أو صحّح أسطر `DB_` في `.env`)، ثم شغّل الترحيلات من تبويب قاعدة البيانات وأنشئ المسؤول عبر `php flo user:create` في طرفية (المزيد، فتح الطرفية).",
    "پڕۆژەکە دروست دەکرێت، بەڵام داتابەیسەکەی ئامادە نییە: نە خشتەی هەیە و نە بەکارهێنەری بەڕێوەبەر. داتابەیسی `%1` دروست بکە (یان دێڕەکانی `DB_` لە `.env` ڕاست بکەرەوە)، پاشان مایگرەیشنەکان لە تابی داتابەیس کار پێ بکە و بەڕێوەبەرەکە بە `php flo user:create` لە تێرمیناڵێکدا دروست بکە (زیاتر، تێرمیناڵ بکەرەوە)."),
 "Skip and finish": ("تخطَّ وأنهِ", "تێپەڕێنە و تەواوی بکە"),
 "Its database isn't ready: no tables and no admin user.":
   ("قاعدة بياناته ليست جاهزة: لا جداول ولا مستخدم مسؤول.",
    "داتابەیسەکەی ئامادە نییە: نە خشتەی هەیە و نە بەکارهێنەری بەڕێوەبەر."),
 "Create the database `%1` (or correct the `DB_` lines in `.env`), then run the migrations on the Database tab and create the admin with `php flo user:create` in a terminal.":
   ("أنشئ قاعدة البيانات `%1` (أو صحّح أسطر `DB_` في `.env`)، ثم شغّل الترحيلات من تبويب قاعدة البيانات وأنشئ المسؤول عبر `php flo user:create` في طرفية.",
    "داتابەیسی `%1` دروست بکە (یان دێڕەکانی `DB_` لە `.env` ڕاست بکەرەوە)، پاشان مایگرەیشنەکان لە تابی داتابەیس کار پێ بکە و بەڕێوەبەرەکە بە `php flo user:create` لە تێرمیناڵێکدا دروست بکە."),
 # Open terminal (part B)
 "Open terminal": ("فتح الطرفية", "تێرمیناڵ بکەرەوە"),
 "Terminal": ("الطرفية", "تێرمیناڵ"),
 "Automatic: Windows Terminal, else PowerShell": ("تلقائي: Windows Terminal، وإلا PowerShell", "خۆکار: Windows Terminal، ئەگەر نا PowerShell"),
 "Automatic: Terminal": ("تلقائي: Terminal", "خۆکار: Terminal"),
 "Automatic: this system's terminal": ("تلقائي: طرفية هذا النظام", "خۆکار: تێرمیناڵی ئەم سیستەمە"),
 "Terminal command": ("أمر الطرفية", "فەرمانی تێرمیناڵ"),
 "Empty: Studio picks the terminal and puts its PHP and Composer first in it. Or a command with `{folder}`, for example `wezterm start --cwd \"{folder}\"`.":
   ("فارغ: يختار Studio الطرفية ويضع PHP وComposer الخاصين به أولًا فيها. أو أمر يحتوي `{folder}`، مثل `wezterm start --cwd \"{folder}\"`.",
    "بەتاڵ: Studio تێرمیناڵەکە هەڵدەبژێرێت و PHP و Composerی خۆی تێیدا دەخاتە پێشەوە. یان فەرمانێک بە `{folder}`ەوە، بۆ نموونە `wezterm start --cwd \"{folder}\"`."),
 "No terminal was found. Enter one in Settings > Editor > Terminal.":
   ("لم يُعثر على طرفية. أدخل واحدة في الإعدادات > المحرر > الطرفية.",
    "هیچ تێرمیناڵێک نەدۆزرایەوە. یەکێک لە ڕێکخستنەکان > دەستکاریکەر > تێرمیناڵ بنووسە."),
 "The terminal did not start.": ("لم تبدأ الطرفية.", "تێرمیناڵەکە دەستی پێ نەکرد."),
 "Studio could not write the `php` and `composer` launchers for the terminal.":
   ("لم يتمكن Studio من كتابة مشغّلَي `php` و`composer` للطرفية.",
    "Studio نەیتوانی دەستپێکەرەکانی `php` و `composer` بۆ تێرمیناڵەکە بنووسێت."),
 "Studio is setting up another project's database. This one can be set up when that is done.":
   ("يُعدّ Studio قاعدة بيانات مشروع آخر الآن. يمكن إعداد هذا المشروع بعد انتهاء ذلك.",
    "Studio ئێستا داتابەیسی پڕۆژەیەکی تر ڕێکدەخات. ئەم پڕۆژەیە دوای تەواوبوونی ئەو دەتوانرێت ڕێکبخرێت."),
 "The server did not create the database.": ("لم ينشئ الخادم قاعدة البيانات.", "سێرڤەر داتابەیسەکەی دروست نەکرد."),
}

P = {}
