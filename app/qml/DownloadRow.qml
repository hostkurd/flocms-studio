import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// One download in progress (board 7b): percent, bar, and the three steps.
Rectangle {
    id: row
    property var entry: ({})
    readonly property string step: entry.state || "waiting"
    readonly property real fraction: entry.total > 0 ? Math.min(1, entry.received / entry.total) : 0

    implicitHeight: column.implicitHeight + 24
    radius: 10
    color: "transparent"
    border.color: row.step === "failed" ? Theme.bad : Theme.border

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                AppText { ui: Texts.toolName(row.entry.tool) + " " + (row.entry.version || ""); font.weight: Font.DemiBold; Layout.fillWidth: true }
                AppText { ui: qsTr("from %1").arg(row.entry.source || ""); color: Theme.muted; font.pixelSize: 13; Layout.fillWidth: true }
            }
            AppText {
                ui: row.step === "downloading" ? Lang.num(Math.round(row.fraction * 100)) + (Lang.rtl ? "٪" : "%")
                    : row.step === "waiting" ? qsTr("Waiting")
                    : row.step === "installing" ? qsTr("Checking and installing…")
                    : row.step === "done" ? qsTr("Ready")
                    : row.step === "cancelled" ? qsTr("Cancelled")
                    : qsTr("Failed")
                color: row.step === "done" ? Theme.okText : (row.step === "failed" ? Theme.badText : Theme.muted)
                font.weight: Font.DemiBold
            }
        }

        ProgressBar {
            visible: row.step === "downloading" || row.step === "installing"
            Layout.fillWidth: true
            from: 0
            to: 1
            value: row.fraction
            indeterminate: row.step === "installing"
            Accessible.name: qsTr("Downloading %1").arg(Texts.toolName(row.entry.tool))
            background: Rectangle { implicitHeight: 6; radius: 3; color: Theme.rule }
            contentItem: Item {
                implicitHeight: 6
                Rectangle {
                    width: parent.width * row.fraction
                    height: parent.height
                    radius: 3
                    color: Theme.accent
                }
            }
        }

        // Download → SHA-256 check → install
        RowLayout {
            visible: row.step !== "waiting" && row.step !== "failed" && row.step !== "cancelled"
            spacing: 14
            Repeater {
                model: [
                    { text: qsTr("Download"), done: row.step === "installing" || row.step === "done", now: row.step === "downloading" },
                    { text: qsTr("Check `SHA-256`"), done: row.step === "done", now: row.step === "installing" },
                    { text: qsTr("Install"), done: row.step === "done", now: false }
                ]
                delegate: RowLayout {
                    required property var modelData
                    spacing: 6
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: modelData.done ? Theme.ok : (modelData.now ? Theme.accent : Theme.idle)
                    }
                    RichText {
                        raw: modelData.text
                        font.pixelSize: 13
                        color: modelData.done || modelData.now ? Theme.text : Theme.muted
                    }
                }
            }
        }

        RichText {
            visible: row.step === "failed"
            raw: row.entry.error ? Texts.problem(row.entry.error) : ""
            color: Theme.badText
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Mono {
            visible: row.step === "failed" && !!row.entry.detail
            text: row.entry.detail || ""
            color: Theme.badText
            font.pixelSize: 12
            wrapMode: Text.WrapAnywhere
            Layout.fillWidth: true
        }
    }
}
