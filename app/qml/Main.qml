import QtQuick
import QtQuick.Layouts
// Only the Basic style: importing QtQuick.Controls would link every style
// into the static build (Fusion, Material, Universal, ...).
import QtQuick.Controls.Basic
import FloStudio.App

// Stage 1 placeholder: proves the build, the libraries and Qt Quick
// Controls work in every build type. The real screens come in stage 3.
ApplicationWindow {
    id: root
    width: 720
    height: 440
    minimumWidth: 560
    minimumHeight: 360
    visible: true
    title: "FloCMS Studio"
    color: "#F4F7F9"

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 560)
        spacing: 16

        RowLayout {
            spacing: 12
            Rectangle {
                width: 40; height: 40; radius: 10
                color: "#38BBE0"
                Text { anchors.centerIn: parent; text: "F"; color: "#05303F"; font.pixelSize: 22; font.bold: true }
            }
            Text { text: "FloCMS Studio " + AppInfo.version; color: "#14212B"; font.pixelSize: 24; font.bold: true }
        }

        Text {
            Layout.fillWidth: true
            text: "This is the stage 1 build check. The real screens come in stage 3."
            color: "#56656F"
            font.pixelSize: 14
            wrapMode: Text.Wrap
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: details.implicitHeight + 28
            radius: 8
            color: "#0F1B24"
            Text {
                id: details
                anchors.fill: parent
                anchors.margins: 14
                text: AppInfo.buildText + "\nLibraries: " + AppInfo.libraries.join(", ")
                color: "#C9D6DF"
                font.family: "monospace"
                font.pixelSize: 13
                lineHeight: 1.3
            }
        }

        Button {
            text: "Copy build details"
            onClicked: AppInfo.copyBuildText()
        }
    }
}
