import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// A row in the sidebar: a project or a Settings section.
ItemDelegate {
    id: item
    property bool selected: false
    property color dot: "transparent"
    property string trailing: ""
    property color trailingColor: Theme.muted
    property string iconPath: ""

    implicitHeight: 38
    leftPadding: 10
    rightPadding: 10
    hoverEnabled: true
    Accessible.name: text
    Accessible.role: Accessible.Button

    background: Rectangle {
        radius: 8
        color: item.selected ? Theme.selectedBg : (item.hovered ? Theme.rule : "transparent")
        border.color: item.visualFocus ? Theme.accent : "transparent"
        border.width: 2
    }

    contentItem: RowLayout {
        spacing: 10
        Rectangle {
            visible: item.iconPath === "" && item.dot !== Qt.color("transparent")
            width: 8; height: 8; radius: 4
            color: item.dot
        }
        Icon { visible: item.iconPath !== ""; path: item.iconPath; color: Theme.navText }
        AppText {
            ui: item.text
            color: item.selected ? Theme.selectedText : Theme.navText
            font.weight: item.selected ? Font.DemiBold : Font.Normal
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        AppText {
            visible: item.trailing !== ""
            text: item.trailing
            color: item.trailingColor
            font.pixelSize: 12
        }
    }
}
