import QtQuick
import QtQuick.Layouts

Rectangle {
    default property alias content: column.data
    property int gap: 10

    color: Theme.surface
    radius: 12
    border.color: Theme.border
    border.width: 1
    implicitWidth: column.implicitWidth + 36
    implicitHeight: column.implicitHeight + 36

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 18
        spacing: parent.gap
    }
}
