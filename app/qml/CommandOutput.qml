import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// What a button's command printed (board 1's Quick commands panel): the
// command, its lines as they come, and how it ended. Left to right in
// every language, as a terminal.
Rectangle {
    id: panel
    property var output: ({})
    property bool bare: false // only the lines (a report's raw output)
    readonly property string state: output.state || ""
    visible: state !== ""
    radius: 8
    color: Theme.terminalBg
    implicitHeight: Math.min(column.implicitHeight + 24, 260)
    clip: true
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: 12
        contentHeight: column.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        onContentHeightChanged: if (panel.state === "running") contentY = Math.max(0, contentHeight - height)
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: column
            width: flick.width - 8
            spacing: 3
            Mono {
                visible: !panel.bare
                text: "› " + (panel.output.shown || panel.output.command || "")
                color: Theme.terminalPrompt
                font.pixelSize: 12
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
            }
            Repeater {
                model: panel.output.lines || []
                delegate: Mono {
                    required property string modelData
                    text: modelData
                    color: Theme.terminalText
                    font.pixelSize: 12
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                }
            }
            AppText {
                visible: panel.state !== "" && !panel.bare
                text: (Lang.rtl ? "‏" : "")
                      + (panel.state === "running" ? qsTr("Running…")
                         : panel.state === "done" ? qsTr("Done")
                         : (panel.output.error ? Texts.problemPlain(panel.output.error) : qsTr("Failed (exit code %1)").arg(panel.output.exitCode)))
                horizontalAlignment: Lang.rtl ? Text.AlignRight : Text.AlignLeft
                color: panel.state === "failed" ? Theme.terminalBad : (panel.state === "done" ? Theme.terminalOk : Theme.terminalMuted)
                font.pixelSize: 12
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
    }
}
