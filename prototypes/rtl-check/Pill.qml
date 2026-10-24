import QtQuick

Rectangle {
    property alias text: label.text
    property color fg: Theme.text
    property int size: 13

    implicitWidth: label.implicitWidth + 24
    implicitHeight: label.implicitHeight + 8
    radius: height / 2

    AppText {
        id: label
        anchors.centerIn: parent
        color: parent.fg
        font.pixelSize: parent.size
        font.bold: true
    }
}
