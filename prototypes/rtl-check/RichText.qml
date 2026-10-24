import QtQuick
import RtlCheck.Backend

// A label whose source string may hold `code words` in backticks.
// Lang.rich() turns them into LTR-isolated JetBrains Mono at display time.
// RichText, because StyledText has no way to set a font family.
AppText {
    property string raw
    property bool plain: false // raw CLI output: shown as is, no marks
    textFormat: plain ? Text.PlainText : Text.RichText
    text: plain ? raw : (Lang.code, Lang.rich(raw))
}
