import QtQuick
import RtlCheck.Backend

// Every label in the app. AlignLeft is mirrored to the right in RTL by
// LayoutMirroring, so a Latin word (a project name) in an Arabic or
// Kurdish window still sits at the start of the line, on the right.
Text {
    color: Theme.text
    font.family: Lang.uiFont
    font.pixelSize: 14
    horizontalAlignment: Text.AlignLeft
    verticalAlignment: Text.AlignVCenter
}
