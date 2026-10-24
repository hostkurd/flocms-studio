import QtQuick

// A path, a command, a version: JetBrains Mono, as it is.
AppText {
    font.family: Theme.monoFont
    font.pixelSize: 13
    textFormat: Text.PlainText
    // Code shows its characters, not JetBrains Mono's ligatures (=> as an
    // arrow): font.features is in Qt 6.6 and later
    Component.onCompleted: {
        if (font.features !== undefined)
            font.features = { "calt": 0, "liga": 0 }
    }
}
