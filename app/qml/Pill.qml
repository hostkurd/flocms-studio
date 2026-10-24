import QtQuick

// A rounded chip: a status ("Missing"), a language ("ar"), a version.
Rectangle {
    property alias text: label.ui
    property color fg: Theme.text
    property bool mono: false

    implicitWidth: label.implicitWidth + 18
    implicitHeight: 22
    radius: height / 2

    AppText {
        id: label
        anchors.centerIn: parent
        color: parent.fg
        font.pixelSize: 12
        font.weight: Font.DemiBold
        font.family: parent.mono ? Theme.monoFont : font.family
    }
}
