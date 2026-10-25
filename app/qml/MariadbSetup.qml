import QtQuick
import QtQuick.Layouts
import FloStudio.Backend

// MariaDB board 3 (design note D6, D11): after the download, "Set up and
// start" in one go, with its steps and what the commands print.
Rectangle {
    id: box
    readonly property var server: Studio.mariadb
    // The series just downloaded, made the one in use when it starts
    property string series: ""
    readonly property string state: server.state
    readonly property var phases: ["check", "setup", "start", "password"]
    readonly property int phaseIndex: state === "running" ? phases.length : phases.indexOf(server.phase)
    visible: state !== "none"
    implicitHeight: column.implicitHeight + 28
    radius: 10
    color: "transparent"
    border.color: Theme.border

    function stepState(index) {
        if (box.state === "running" || index < box.phaseIndex) return "done"
        if (box.state === "starting" && index === box.phaseIndex) return "running"
        return "waiting"
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            AppText {
                ui: box.state === "running" ? qsTr("Studio's MariaDB %1 is running on port %2.").arg(box.server.series).arg(box.server.port)
                    : box.state === "starting" ? qsTr("Setting up Studio's MariaDB %1").arg(box.server.series)
                    : box.server.initialized ? qsTr("Studio's MariaDB %1 is stopped.").arg(box.server.series)
                    : qsTr("Studio's MariaDB %1 is downloaded.").arg(box.series !== "" ? box.series : box.server.series)
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            AppButton {
                visible: box.state === "stopped" || box.state === "failed"
                primary: true
                text: box.server.initialized ? qsTr("Start") : qsTr("Set up and start")
                iconPath: Icons.play
                iconFilled: true
                onClicked: {
                    if (box.series !== "")
                        box.server.setSeries(box.series)
                    box.server.start()
                }
            }
        }

        // The steps while it starts (and once it runs)
        Repeater {
            model: box.state === "starting" || box.state === "running" ? [
                qsTr("Downloaded and checked against its `SHA-256`"),
                qsTr("Databases folder made, `root` password saved in %1").arg(Texts.secureStoreName()),
                qsTr("Starting on port `%1`").arg(box.server.port),
                qsTr("Password of `root` set")
            ] : []
            delegate: RowLayout {
                required property string modelData
                required property int index
                readonly property string step: box.stepState(index)
                spacing: 10
                Layout.fillWidth: true
                StatusIcon {
                    status: step === "done" ? "ok" : (step === "running" ? "running" : "waiting")
                }
                RichText {
                    raw: modelData
                    color: step === "waiting" ? Theme.muted : Theme.text
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
        }

        MariadbProblem { Layout.fillWidth: true }

        // What the commands print, as a terminal
        Rectangle {
            visible: (box.state === "starting" || box.state === "failed") && box.server.lines.length > 0
            Layout.fillWidth: true
            implicitHeight: out.implicitHeight + 20
            radius: 8
            color: Theme.terminalBg
            LayoutMirroring.enabled: false
            LayoutMirroring.childrenInherit: true
            ColumnLayout {
                id: out
                anchors.fill: parent
                anchors.margins: 10
                spacing: 2
                Repeater {
                    model: box.server.lines.slice(-6)
                    delegate: Mono {
                        required property string modelData
                        text: modelData
                        color: modelData.startsWith("› ") ? Theme.terminalPrompt : Theme.terminalMuted
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }
}
