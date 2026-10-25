import QtQuick
import QtQuick.Window
import FloStudio.Backend

// Shown the moment Studio starts, until the main window has drawn its
// first frame (main.cpp closes it). Small and quick to make: the logo,
// the name and "Starting…", in the person's theme and language.
Window {
    id: splash
    width: 360
    height: 200
    x: Math.round((Screen.width - width) / 2)
    y: Math.round((Screen.height - height) / 2)
    flags: Qt.SplashScreen | Qt.FramelessWindowHint
    // Solid: a see-through window shows black corners where there is no
    // compositor (some Linux desktops)
    color: Theme.surface
    title: "FloCMS Studio"
    visible: true

    Rectangle {
        anchors.fill: parent
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
        LayoutMirroring.enabled: Lang.rtl
        LayoutMirroring.childrenInherit: true

        Column {
            anchors.centerIn: parent
            spacing: 14
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12
                LayoutMirroring.enabled: false // the wordmark reads the same in every language
                Image {
                    source: "qrc:/icons/logo-128.png"
                    width: 44
                    height: 44
                    sourceSize: Qt.size(88, 88)
                    smooth: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "FloCMS Studio"
                    font.family: "Bricolage Grotesque"
                    font.weight: Font.Bold
                    font.pixelSize: 26
                    color: Theme.text
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            AppText {
                anchors.horizontalCenter: parent.horizontalCenter
                ui: qsTr("Starting…")
                color: Theme.muted
                font.pixelSize: 13
            }
        }
    }
}
