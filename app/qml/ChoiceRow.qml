import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// One radio choice with a path and a note (Settings > PHP to use).
Rectangle {
    id: row
    property var group: null
    property alias checked: radio.checked
    property string path
    property string detail
    signal picked()

    implicitHeight: 48
    radius: 8
    color: radio.checked ? Theme.optionSelectedBg : "transparent"
    border.color: radio.checked ? Theme.accent : Theme.border

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 12
        spacing: 8
        AppRadio {
            id: radio
            ButtonGroup.group: row.group
            Accessible.name: row.path
            onClicked: row.picked()
        }
        Mono { text: row.path; elide: Text.ElideMiddle; Layout.fillWidth: true }
        AppText { text: row.detail; color: Theme.muted; font.pixelSize: 13 }
    }
}
