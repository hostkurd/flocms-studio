import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import RtlCheck.Backend

ApplicationWindow {
    id: root
    property string page: "overview" // overview | doctor
    property bool dark: false

    width: 1280
    height: 800
    minimumWidth: 1100
    minimumHeight: 700
    visible: true
    title: "FloCMS Studio"
    color: Theme.bg

    Binding { target: Theme; property: "dark"; value: root.dark }

    // One switch mirrors the whole window for Arabic and Kurdish.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Lang.rtl
        LayoutMirroring.childrenInherit: true

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Sidebar {
                Layout.fillHeight: true
                Layout.preferredWidth: 240
                dark: root.dark
                onDarkToggled: (on) => root.dark = on
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 24
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    ColumnLayout {
                        spacing: 2
                        AppText { text: "fectumhellas"; font.pixelSize: 28; font.bold: true }
                        AppText { text: "C:\\Sites\\fectumhellas"; font.family: Theme.monoFont; font.pixelSize: 13; color: Theme.muted }
                    }
                    Item { Layout.fillWidth: true }
                    AppButton { text: qsTr("Open site") }
                    AppButton { text: qsTr("Open admin") }
                    AppButton { text: qsTr("Open in `VS Code`") }
                    AppButton { text: qsTr("Stop server"); danger: true }
                }

                // Section tabs. Overview and Doctor work in this check.
                Item {
                    Layout.fillWidth: true
                    implicitHeight: 41
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
                    Row {
                        spacing: 22
                        Repeater {
                            model: [
                                { key: "overview", label: qsTr("Overview") },
                                { key: "generators", label: qsTr("Generators") },
                                { key: "database", label: qsTr("Database") },
                                { key: "logs", label: qsTr("Logs") },
                                { key: "packages", label: qsTr("Packages") },
                                { key: "doctor", label: qsTr("Doctor") }
                            ]
                            delegate: AbstractButton {
                                id: tab
                                required property var modelData
                                readonly property bool current: root.page === modelData.key
                                implicitHeight: 41
                                implicitWidth: tabLabel.implicitWidth + 8
                                enabled: modelData.key === "overview" || modelData.key === "doctor"
                                onClicked: root.page = modelData.key
                                contentItem: AppText {
                                    id: tabLabel
                                    text: tab.modelData.label
                                    horizontalAlignment: Text.AlignHCenter
                                    color: tab.current ? Theme.selectedText : Theme.muted
                                    font.bold: tab.current
                                }
                                background: Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: 2
                                    color: tab.current ? Theme.accent : "transparent"
                                }
                            }
                        }
                    }
                }

                Loader {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    source: root.page === "doctor" ? "Doctor.qml" : "Overview.qml"
                }
            }
        }
    }
}
