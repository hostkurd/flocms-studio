import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// The Logs tab (stage 4, D10): the server's output and storage/logs/app.log
// as one live list. No approved board: built in the boards' style (the
// Quick commands terminal of board 1). The lines are what PHP and FloCMS
// print, in English and left to right; Studio's own lines are translated.
Item {
    id: tab
    readonly property var logs: Studio.logs
    // Keep showing the newest line while the view is at the end
    property bool follow: true

    function studioText(level, text) {
        Lang.code
        switch (level) {
        case "started": return qsTr("Server running at %1").arg(text)
        case "stopped": return qsTr("Server stopped")
        case "leftover": return qsTr("Stopped a server of this project that was still running from the last time Studio closed unexpectedly.")
        case "command": return "› " + text
        }
        return text
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // --- What to show ---
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Segmented {
                label: qsTr("Log")
                value: tab.logs ? tab.logs.source : "all"
                options: [
                    { value: "all", text: qsTr("All") },
                    { value: "server", text: qsTr("Server") },
                    { value: "app", text: qsTr("App log") }
                ]
                onChosen: (value) => { if (tab.logs) tab.logs.source = value }
            }
            AppCheck {
                text: tab.logs && tab.logs.problems > 0 ? qsTr("Problems only (%1)").arg(Lang.num(tab.logs.problems)) : qsTr("Problems only")
                checked: tab.logs ? tab.logs.problemsOnly : false
                onToggled: if (tab.logs) tab.logs.problemsOnly = checked
            }
            Field {
                id: search
                placeholderText: qsTr("Search")
                Layout.preferredWidth: 220
                Accessible.name: qsTr("Search the log")
                onTextEdited: if (tab.logs) tab.logs.search = text
            }
            // Each project has its own log and search: show the open one's
            Connections {
                target: tab
                function onLogsChanged() { search.text = tab.logs ? tab.logs.search : "" }
            }
            Item { Layout.fillWidth: true }
            AppButton {
                small: true
                text: qsTr("Copy")
                iconPath: Icons.copy
                enabled: !!tab.logs && tab.logs.shown > 0
                onClicked: tab.logs.copy()
            }
            AppButton {
                small: true
                text: qsTr("Clear")
                iconPath: Icons.trash
                enabled: !!tab.logs && tab.logs.total > 0
                onClicked: tab.logs.clear()
            }
            AppButton {
                small: true
                text: qsTr("Open log file")
                iconPath: Icons.file
                onClicked: Studio.openLogFile()
            }
        }

        // --- The lines ---
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 10
            color: Theme.terminalBg
            clip: true
            // Log lines read left to right in every language
            LayoutMirroring.enabled: false
            LayoutMirroring.childrenInherit: true

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: 12
                model: tab.logs
                spacing: 1
                boundsBehavior: Flickable.StopAtBounds
                reuseItems: true
                Accessible.name: qsTr("Log lines")
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                onCountChanged: if (tab.follow) Qt.callLater(list.positionViewAtEnd)
                onMovementEnded: tab.follow = atYEnd
                Component.onCompleted: positionViewAtEnd()

                delegate: Rectangle {
                    id: row
                    required property string text
                    required property string source
                    required property string level
                    required property string time
                    required property bool continuation
                    // Only Studio's own sentences are translated; what flocms-cli
                    // printed (Serving ..., the command, its error) stays as it is
                    readonly property bool translated: source === "studio" && ["started", "stopped", "leftover"].indexOf(level) >= 0
                    width: ListView.view.width - 10
                    implicitHeight: line.implicitHeight + 4
                    color: level === "problem" && !continuation ? Theme.terminalBadBg : "transparent"
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        spacing: 10
                        Mono {
                            text: row.continuation ? "" : row.time
                            color: Theme.terminalMuted
                            font.pixelSize: 12
                            Layout.preferredWidth: 58
                            Layout.alignment: Qt.AlignTop
                        }
                        Mono {
                            text: row.continuation ? "" : (row.source === "app" ? "app" : (row.source === "server" ? "srv" : "›"))
                            color: row.source === "studio" ? Theme.terminalPrompt : Theme.terminalMuted
                            font.pixelSize: 12
                            Layout.preferredWidth: 26
                            Layout.alignment: Qt.AlignTop
                        }
                        AppText {
                            id: line
                            // Studio's own lines are translated and follow the language
                            // (the mark keeps an Arabic or Kurdish one right to left);
                            // PHP's and FloCMS's lines stay as they were printed
                            text: row.translated ? (Lang.rtl ? "\u200F" : "") + tab.studioText(row.level, row.text) : row.text
                            textFormat: Text.PlainText
                            font.family: row.translated ? Lang.uiFont : Theme.monoFont
                            font.pixelSize: row.translated ? 13 : 12
                            wrapMode: Text.WrapAnywhere
                            horizontalAlignment: row.translated && Lang.rtl ? Text.AlignRight : Text.AlignLeft
                            leftPadding: row.continuation ? 18 : 0
                            color: row.level === "problem" ? Theme.terminalBad
                                 : row.level === "warning" ? Theme.terminalWarn
                                 : row.level === "quiet" ? Theme.terminalMuted
                                 : row.level === "started" ? Theme.terminalOk
                                 : row.level === "command" ? Theme.terminalPrompt
                                 : Theme.terminalText
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            // Empty, or everything filtered out
            AppText {
                anchors.centerIn: parent
                width: parent.width - 80
                visible: !tab.logs || tab.logs.shown === 0
                ui: !tab.logs || tab.logs.total === 0
                    ? qsTr("Nothing logged yet. Start the server to see what it prints; FloCMS's own log appears as it is written.")
                    : qsTr("No lines match.")
                color: Theme.terminalMuted
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                LayoutMirroring.enabled: Lang.rtl
            }

            // Scrolled up: back to the newest line
            AppButton {
                visible: !tab.follow && list.count > 0
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 14
                small: true
                text: qsTr("Newest")
                iconPath: Icons.arrowDown
                onClicked: { tab.follow = true; list.positionViewAtEnd() }
            }
        }
    }
}
