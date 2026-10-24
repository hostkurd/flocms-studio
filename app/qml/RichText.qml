import QtQuick
import FloStudio.Backend

// A label whose source string may hold `code words` in backticks: Lang.rich()
// turns them into left-to-right isolated JetBrains Mono when shown (D9).
// RichText, because StyledText can't set a font family.
AppText {
    property string raw
    textFormat: Text.RichText
    text: (Lang.code, Lang.rich(raw))
}
