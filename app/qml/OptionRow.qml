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
    // A state chip at the end (Running, Downloaded)
    property string badge
    property color badgeColor: Theme.border
    property color badgeText: Theme.muted
    signal toggled(bool on)

    implicitHeight: content.implicitHeight + 20
    radius: 10
    // The whole row is the control: it shows the pointer over it, and a
    // click anywhere on it chooses it. Not enabled: greyed, nothing happens
    color: row.checked ? Theme.optionSelectedBg : (hover.hovered ? Theme.hoverBg : "transparent")
    border.color: row.checked ? Theme.accent : (hover.hovered ? Theme.controlBorderHover : Theme.border)
    opacity: row.enabled ? 1 : 0.55
    HoverHandler { id: hover; enabled: row.enabled; cursorShape: Qt.PointingHandCursor }
    TapHandler {
        enabled: row.enabled
        onTapped: {
            if (row.radio) {
                if (!row.checked) row.toggled(true)
            } else {
                row.toggled(!row.checked)
            }
        }
    }

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
        Pill {
            visible: row.badge !== ""
            text: row.badge
            color: row.badgeColor
            fg: row.badgeText
        }
        AppText {
            visible: row.size > 0
            text: Lang.size(row.size)
            color: Theme.muted
            font.pixelSize: 13
        }
    }
}
