import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// A project: board 1's header and tabs. Stage 3 fills Overview from the
// project's files; the other tabs come with their stages.
Item {
    id: page
    readonly property var info: Studio.current
    property int tab: 0

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        anchors.topMargin: 24
        spacing: 18

        // Header: name and folder, then the actions
        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true
                Heading { level: 1; text: page.info.name || ""; elide: Text.ElideRight; Layout.fillWidth: true }
                Mono { text: page.info.path || ""; color: Theme.muted; elide: Text.ElideMiddle; Layout.fillWidth: true }
            }
            AppButton {
                text: qsTr("Open folder")
                iconPath: Icons.folder
                onClicked: Studio.openFolder(page.info.path)
            }
            AppButton {
                text: qsTr("Open in editor")
                iconPath: Icons.code
                enabled: !page.info.error
                onClicked: Studio.openInEditor(page.info.path)
            }
        }

        // The project is gone or no longer FloCMS
        Card {
            visible: !!page.info.error
            Layout.fillWidth: true
            RichText {
                raw: page.info.error ? Texts.problem(page.info.error) : ""
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            AppButton {
                text: qsTr("Remove from the list")
                iconPath: Icons.trash
                onClicked: Studio.removeProject(page.info.path)
            }
        }

        TabBar {
            id: tabs
            visible: !page.info.error
            Layout.fillWidth: true
            spacing: 22
            currentIndex: page.tab
            onCurrentIndexChanged: page.tab = currentIndex
            background: Rectangle {
                color: "transparent"
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
            }
            Repeater {
                model: [qsTr("Overview"), qsTr("Generate"), qsTr("Database"), qsTr("Logs"), qsTr("Packages"), qsTr("Doctor")]
                delegate: TabButton {
                    id: tabButton
                    required property string modelData
                    required property int index
                    text: modelData
                    width: implicitWidth
                    implicitHeight: 40
                    leftPadding: 4
                    rightPadding: 4
                    font.family: Lang.uiFont
                    font.pixelSize: 14
                    font.weight: tabs.currentIndex === index ? Font.DemiBold : Font.Medium
                    background: Rectangle {
                        color: "transparent"
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 2
                            color: tabs.currentIndex === tabButton.index ? Theme.accent : "transparent"
                        }
                    }
                    contentItem: AppText {
                        ui: tabButton.text
                        font: tabButton.font
                        color: tabs.currentIndex === tabButton.index ? Theme.selectedText : Theme.muted
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }

        StackLayout {
            visible: !page.info.error
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: page.tab
            OverviewTab { info: page.info }
            LaterTab { stage: 7 }
            LaterTab { stage: 5 }
            LaterTab { stage: 4 }
            LaterTab { stage: 8 }
            LaterTab { stage: 5 }
        }
    }
}
