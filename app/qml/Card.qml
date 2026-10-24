import QtQuick
import QtQuick.Layouts

Rectangle {
    default property alias content: column.data
    property int gap: 10
    property int padding: 18

    color: Theme.surface
    radius: 12
    border.color: Theme.border
    border.width: 1
    implicitWidth: column.implicitWidth + 2 * padding
    implicitHeight: column.implicitHeight + 2 * padding

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: parent.padding
        spacing: parent.gap
    }
}
