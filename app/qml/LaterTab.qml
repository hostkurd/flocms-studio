import QtQuick
import QtQuick.Layouts
import FloStudio.Backend

// A tab whose screen comes in a later stage of the native app.
Item {
    property int stage: 4
    ColumnLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 8
        AppText {
            ui: qsTr("This tab comes in a later version of the native Studio.")
            color: Theme.muted
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }
}
