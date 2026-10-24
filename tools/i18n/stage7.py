"""Arabic (ar) and Central Kurdish (ckb) for the strings stage 7 added (the
Generate tab and its preview).

Each needs a native read (listed in the PR). Keys are the English source
strings. Words used before keep their translation: مورد إدارة /
سەرچاوەی بەڕێوەبردن (admin resource), ترحيل / مایگرەیشن (migration),
جدول / خشتە (table), لوحة الإدارة / پانێڵی بەڕێوەبردن (admin panel).
Filled into app/i18n with tools/fill_translations.py.
"""

T = {
 "Generator": ("المولّد", "دروستکەر"),
 "Admin resource": ("مورد إدارة", "سەرچاوەی بەڕێوەبردن"),
 "Admin page": ("صفحة إدارة", "پەڕەی بەڕێوەبردن"),
 "Block": ("كتلة", "بلۆک"),
 "Widget": ("أداة", "ویجێت"),
 "Controller": ("متحكّم", "کۆنترۆڵەر"),
 "The admin resource, page, block and widget need the admin panel (`hostkurd/flocms-admin`).":
   ("يحتاج مورد الإدارة وصفحة الإدارة والكتلة والأداة إلى لوحة الإدارة (`hostkurd/flocms-admin`).",
    "سەرچاوە، پەڕە، بلۆک و ویجێتی بەڕێوەبردن پێویستیان بە پانێڵی بەڕێوەبردنە (`hostkurd/flocms-admin`)."),
 "New admin resource": ("مورد إدارة جديد", "سەرچاوەی بەڕێوەبردنی نوێ"),
 "New admin page": ("صفحة إدارة جديدة", "پەڕەی بەڕێوەبردنی نوێ"),
 "New block": ("كتلة جديدة", "بلۆکی نوێ"),
 "New dashboard widget": ("أداة جديدة للوحة المعلومات", "ویجێتی نوێی داشبۆرد"),
 "New controller": ("متحكّم جديد", "کۆنترۆڵەری نوێ"),
 "New migration": ("ترحيل جديد", "مایگرەیشنی نوێ"),
 "What it does": ("ما يفعله", "چی دەکات"),
 "A class name, e.g. `Property` or `BlogPost`.": ("اسم صنف، مثل `Property` أو `BlogPost`.", "ناوی کلاسێک، بۆ نموونە `Property` یان `BlogPost`."),
 "A class name, e.g. `SalesReport`.": ("اسم صنف، مثل `SalesReport`.", "ناوی کلاسێک، بۆ نموونە `SalesReport`."),
 "A class name, e.g. `LatestProjects`.": ("اسم صنف، مثل `LatestProjects`.", "ناوی کلاسێک، بۆ نموونە `LatestProjects`."),
 "A class name, e.g. `PostStats` or `RecentPosts`.": ("اسم صنف، مثل `PostStats` أو `RecentPosts`.", "ناوی کلاسێک، بۆ نموونە `PostStats` یان `RecentPosts`."),
 "E.g. `Blog` or `BlogPost`.": ("مثل `Blog` أو `BlogPost`.", "بۆ نموونە `Blog` یان `BlogPost`."),
 "E.g. `create_posts_table` or `add_slug_to_posts`.": ("مثل `create_posts_table` أو `add_slug_to_posts`.", "بۆ نموونە `create_posts_table` یان `add_slug_to_posts`."),
 "Table": ("الجدول", "خشتە"),
 "Empty: the plural of the name.": ("فارغ: جمع الاسم.", "بەتاڵ: کۆی ناوەکە."),
 "URL": ("الرابط", "بەستەر"),
 "the table": ("الجدول", "خشتەکە"),
 "Sidebar group": ("مجموعة الشريط الجانبي", "گرووپی لاتەنیشت"),
 "E.g. Reports. Empty: no group.": ("مثل Reports. فارغ: بلا مجموعة.", "بۆ نموونە Reports. بەتاڵ: بێ گرووپ."),
 "Create and Edit open as": ("يُفتح الإنشاء والتعديل في", "دروستکردن و دەستکاریکردن دەکرێنەوە وەک"),
 "A full page": ("صفحة كاملة", "پەڕەیەکی تەواو"),
 "A dialog": ("نافذة حوار", "پەنجەرەیەکی گفتوگۆ"),
 "A slide-over panel": ("لوحة منزلقة", "پانێڵێکی خلیسکاو"),
 "Also create a migration for the table": ("أنشئ أيضًا ترحيلًا للجدول", "هەروەها مایگرەیشنێک بۆ خشتەکە دروست بکە"),
 "Editors can drag records into order (a `sort` column)":
   ("يستطيع المحررون سحب السجلات لترتيبها (عمود `sort`)", "دەستکاریکەران دەتوانن تۆمارەکان ڕابکێشن بۆ ڕیزکردن (ستوونی `sort`)"),
 "The resource starts with a title field. Add the other fields in the file.":
   ("يبدأ المورد بحقل عنوان. أضف الحقول الأخرى في الملف.", "سەرچاوەکە بە خانەی ناونیشانێک دەست پێدەکات. خانەکانی تر لە فایلەکەدا زیاد بکە."),
 "The page shows": ("تعرض الصفحة", "پەڕەکە پیشان دەدات"),
 "Content of its own": ("محتوى خاصًّا بها", "ناوەڕۆکی خۆی"),
 "A form with Save": ("نموذجًا مع زر حفظ", "فۆڕمێک لەگەڵ پاشەکەوت"),
 "A database table": ("جدول قاعدة بيانات", "خشتەیەکی داتابەیس"),
 "A resource's records": ("سجلات مورد", "تۆمارەکانی سەرچاوەیەک"),
 "Numbers": ("أرقام", "ژمارەکان"),
 "Latest records": ("أحدث السجلات", "نوێترین تۆمارەکان"),
 "A chart": ("رسم بياني", "هێڵکاری"),
 "Actions": ("الإجراءات", "کردارەکان"),
 "A page": ("صفحة", "پەڕەیەک"),
 "Pages and admin actions (CRUD)": ("صفحات وإجراءات إدارة (CRUD)", "پەڕەکان و کردارەکانی بەڕێوەبردن (CRUD)"),
 "Admin actions": ("إجراءات الإدارة", "کردارەکانی بەڕێوەبردن"),
 "JSON API": ("واجهة JSON API", "JSON API"),
 "Also create its model": ("أنشئ أيضًا نموذجه (model)", "هەروەها مۆدێلەکەی دروست بکە"),
 "Empty": ("فارغ", "بەتاڵ"),
 "Create a table": ("إنشاء جدول", "دروستکردنی خشتە"),
 "Change a table": ("تعديل جدول", "گۆڕینی خشتە"),
 "A name like `create_posts_table` creates the `posts` table.": ("اسم مثل `create_posts_table` يُنشئ الجدول `posts`.", "ناوێکی وەک `create_posts_table` خشتەی `posts` دروست دەکات."),
 "Resource": ("المورد", "سەرچاوە"),
 "Optional: the resource it counts.": ("اختياري: المورد الذي تعدّه.", "ئارەزوومەندانە: ئەو سەرچاوەیەی دەیژمێرێت."),
 "Also write its view, with the panel's parts": ("اكتب أيضًا عرضها (view) بعناصر اللوحة", "هەروەها ڤیوەکەی بە بەشەکانی پانێڵەکە بنووسە"),
 "Overwrite files that are there": ("استبدل الملفات الموجودة", "فایلە هەبووەکان بگۆڕەوە"),
 "Preview": ("معاينة", "پێشبینین"),
 "Create resource": ("إنشاء المورد", "دروستکردنی سەرچاوە"),
 "Create page": ("إنشاء الصفحة", "دروستکردنی پەڕە"),
 "Create block": ("إنشاء الكتلة", "دروستکردنی بلۆک"),
 "Create widget": ("إنشاء الأداة", "دروستکردنی ویجێت"),
 "Create controller": ("إنشاء المتحكّم", "دروستکردنی کۆنترۆڵەر"),
 "Create migration": ("إنشاء الترحيل", "دروستکردنی مایگرەیشن"),
 "Preview: what it will write": ("معاينة: ما سيكتبه", "پێشبینین: ئەوەی دەینووسێت"),
 "Preview runs the generator in a temporary copy of the project and shows the files it writes. Your project changes only with Create.":
   ("تشغّل المعاينة المولّد في نسخة مؤقتة من المشروع وتعرض الملفات التي يكتبها. لا يتغير مشروعك إلا بزر الإنشاء.",
    "پێشبینین دروستکەرەکە لە کۆپییەکی کاتیی پڕۆژەکەدا جێبەجێ دەکات و ئەو فایلانە پیشان دەدات کە دەینووسێت. پڕۆژەکەت تەنها بە دروستکردن دەگۆڕێت."),
 "Copying the project…": ("جارٍ نسخ المشروع…", "پڕۆژەکە کۆپی دەکرێت…"),
 "Running the generator in the copy…": ("جارٍ تشغيل المولّد في النسخة…", "دروستکەرەکە لە کۆپییەکەدا کار دەکات…"),
 "The form changed since this preview. Preview again to see what it writes now.":
   ("تغيّر النموذج بعد هذه المعاينة. عاين مجددًا لترى ما سيكتبه الآن.", "فۆڕمەکە دوای ئەم پێشبینینە گۆڕا. دووبارە پێشبینی بکە بۆ بینینی ئەوەی ئێستا دەینووسێت."),
 "It writes no files.": ("لا يكتب أي ملفات.", "هیچ فایلێک نانووسێت."),
 "Not a text file.": ("ليس ملفًا نصيًا.", "فایلی دەق نییە."),
 "Too large to show here.": ("أكبر من أن يُعرض هنا.", "گەورەترە لەوەی لێرە پیشان بدرێت."),
 "A class name in StudlyCase: a capital first, then letters and digits (`BlogPost`).":
   ("اسم صنف بصيغة StudlyCase: حرف كبير أولًا، ثم حروف وأرقام (`BlogPost`).", "ناوی کلاس بە شێوەی StudlyCase: سەرەتا پیتێکی گەورە، پاشان پیت و ژمارە (`BlogPost`)."),
 "Letters, digits, `-` and `_`, starting with a letter.": ("حروف وأرقام و`-` و`_`، يبدأ بحرف.", "پیت، ژمارە، `-` و `_`، بە پیتێک دەست پێبکات."),
 "Letters, digits and `_`, starting with a letter (`add_slug_to_posts`).": ("حروف وأرقام و`_`، يبدأ بحرف (`add_slug_to_posts`).", "پیت، ژمارە و `_`، بە پیتێک دەست پێبکات (`add_slug_to_posts`)."),
 "A table name: letters, digits and `_`, up to 64.": ("اسم جدول: حروف وأرقام و`_`، حتى 64.", "ناوی خشتە: پیت، ژمارە و `_`، تا 64."),
 "Enter the table.": ("أدخل الجدول.", "خشتەکە بنووسە."),
 "Lower case letters and digits, with `-` between words.": ("حروف صغيرة وأرقام، مع `-` بين الكلمات.", "پیتی بچووک و ژمارە، لەگەڵ `-` لە نێوان وشەکان."),
 "A resource's class name, e.g. `Post`.": ("اسم صنف المورد، مثل `Post`.", "ناوی کلاسی سەرچاوەیەک، بۆ نموونە `Post`."),
 "Enter the resource it lists, e.g. `Post`.": ("أدخل المورد الذي تعرضه، مثل `Post`.", "ئەو سەرچاوەیە بنووسە کە لیستی دەکات، بۆ نموونە `Post`."),
 "The project has no `vendor/` folder yet: run `composer install` in it.":
   ("لا يحتوي المشروع على المجلد `vendor/` بعد: شغّل `composer install` فيه.", "پڕۆژەکە هێشتا فۆڵدەری `vendor/` ـی نییە: `composer install` تێیدا جێبەجێ بکە."),
 "The project is too large to copy for a preview (over 400 MB or 40,000 files, without uploads and logs). Create works without one.":
   ("المشروع أكبر من أن يُنسخ للمعاينة (أكثر من 400 ميغابايت أو 40,000 ملف، دون المرفوعات والسجلات). يعمل الإنشاء دون معاينة.",
    "پڕۆژەکە گەورەترە لەوەی بۆ پێشبینین کۆپی بکرێت (زیاتر لە 400 MB یان 40,000 فایل، بەبێ بارکراوەکان و تۆمارەکان). دروستکردن بەبێ پێشبینین کار دەکات."),
 "Studio could not copy the project for the preview.": ("تعذّر على Studio نسخ المشروع للمعاينة.", "Studio نەیتوانی پڕۆژەکە بۆ پێشبینین کۆپی بکات."),
 "The preview was stopped.": ("أُوقفت المعاينة.", "پێشبینینەکە وەستێنرا."),
 "Studio could not remove the preview's copy. It tries again when it next starts.":
   ("تعذّر على Studio حذف نسخة المعاينة. سيحاول مجددًا عند تشغيله التالي.", "Studio نەیتوانی کۆپیی پێشبینینەکە بسڕێتەوە. لە دەستپێکردنی داهاتوودا دووبارە هەوڵ دەداتەوە."),
}

P = {
 "%Ln file(s)": (
   ["%Ln file", "%Ln files"],
   ["لا ملفات", "ملف واحد", "ملفان", "%Ln ملفات", "%Ln ملفًا", "%Ln ملف"],
   ["%Ln فایل", "%Ln فایل"]),
 "%Ln unchanged line(s)": (
   ["%Ln unchanged line", "%Ln unchanged lines"],
   ["لا أسطر دون تغيير", "سطر واحد دون تغيير", "سطران دون تغيير", "%Ln أسطر دون تغيير", "%Ln سطرًا دون تغيير", "%Ln سطر دون تغيير"],
   ["%Ln هێڵی نەگۆڕاو", "%Ln هێڵی نەگۆڕاو"]),
}
