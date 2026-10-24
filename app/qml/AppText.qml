import QtQuick
import FloStudio.Backend

// Every label in the app. AlignLeft is mirrored to the right in RTL by
// LayoutMirroring, so a Latin word (a project name) in an Arabic or
// Kurdish window still sits at the start of the line, on the right.
Text {
    // A translated label: set `ui` instead of `text`. In Arabic and Kurdish
    // it is shown after a right-to-left mark, so a label that starts with a
    // Latin word ("PHP و ئامرازەکان") still reads right to left. The mark is
    // added here only; translation files never hold it (D9). Names, paths
    // and versions keep using `text`.
    property string ui
    text: ui === "" ? "" : (Lang.rtl ? "\u200F" + ui : ui)
    color: Theme.text
    font.family: Lang.uiFont
    font.pixelSize: 14
    horizontalAlignment: Text.AlignLeft
    verticalAlignment: Text.AlignVCenter
    wrapMode: Text.NoWrap
}
