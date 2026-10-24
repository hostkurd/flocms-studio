import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// A choice in the downloads dialog: a radio button (PHP) or a checkbox.
Rectangle {
    id: row
    property bool radio: false
    property var group: null
    property bool checked: false
    property string title
    property string note
    property real size: 0
    signal toggled(bool on)

    implicitHeight: content.implicitHeight + 20
    radius: 10
    color: row.checked ? Theme.optionSelectedBg : "transparent"
    border.color: row.checked ? Theme.accent : Theme.border

    RowLayout {
        id: content
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 14
        spacing: 6
        AppRadio {
            visible: row.radio
            checked: row.checked
            ButtonGroup.group: row.radio ? row.group : null
            Accessible.name: row.title
            onToggled: row.toggled(checked)
        }
        AppCheck {
            visible: !row.radio
            implicitWidth: 40
            checked: row.checked
            Accessible.name: row.title
            onToggled: row.toggled(checked)
        }
        ColumnLayout {
            spacing: 2
            Layout.fillWidth: true
            AppText { ui: row.title; font.weight: Font.DemiBold }
            AppText { ui: row.note; color: Theme.muted; font.pixelSize: 13; wrapMode: Text.Wrap; Layout.fillWidth: true }
        }
        AppText {
            visible: row.size > 0
            text: Lang.size(row.size)
            color: Theme.muted
            font.pixelSize: 13
        }
    }
}
