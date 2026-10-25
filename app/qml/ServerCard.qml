import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// Board 1's Server card: is the site running, where, since when.
Card {
    id: card
    readonly property var server: Studio.server
    readonly property string state: server.state || "stopped"
    readonly property bool running: state === "running"
    // Minutes since it started, kept fresh while it runs
    property real now: Date.now()
    Timer { interval: 30000; repeat: true; running: card.running; onTriggered: card.now = Date.now() }
    readonly property int minutes: server.startedAt > 0 ? Math.max(0, Math.floor((now - server.startedAt) / 60000)) : 0

    SectionLabel { ui: qsTr("Server") }
    RowLayout {
        spacing: 8
        Rectangle {
            width: 10; height: 10; radius: 5
            color: card.running ? Theme.ok
                 : (card.state === "failed" ? Theme.bad
                 : (card.state === "starting" || card.state === "stopping" ? Theme.warn : Theme.idle))
        }
        Heading {
            level: 2
            ui: card.running ? qsTr("Running")
                : card.state === "starting" ? qsTr("Starting…")
                : card.state === "stopping" ? qsTr("Stopping…")
                : card.state === "failed" ? qsTr("Not running")
                : qsTr("Stopped")
        }
    }
    Mono {
        visible: card.running && !!card.server.url
        text: card.server.url || ""
        color: Theme.link
        font.underline: linkArea.containsMouse
        Accessible.role: Accessible.Link
        Accessible.name: text
        MouseArea {
            id: linkArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: Studio.openSite()
        }
    }
    AppText {
        visible: card.running
        ui: (card.server.php ? "PHP " + card.server.php + " · " : "")
            + (card.minutes < 1 ? qsTr("started just now") : qsTr("started %Ln min ago", "", card.minutes))
        color: Theme.muted
        font.pixelSize: 13
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    // --host=0.0.0.0: the addresses for a phone or another computer
    ColumnLayout {
        visible: card.running && (card.server.networkUrls || []).length > 0
        spacing: 2
        AppText { ui: qsTr("From other devices on your network:"); color: Theme.muted; font.pixelSize: 13 }
        Repeater {
            model: card.server.networkUrls || []
            delegate: Mono { required property string modelData; text: modelData; font.pixelSize: 12 }
        }
    }
    RichText {
        visible: card.state === "stopped"
        raw: qsTr("Runs the site with `php flo serve` on port %1.").arg(Lang.num(Studio.serverPort))
        color: Theme.muted
        font.pixelSize: 13
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    RichText {
        visible: card.state === "failed" && !!card.server.error
        raw: card.server.error ? Texts.problem(card.server.error) : ""
        color: Theme.badText
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    Mono {
        visible: card.state === "failed" && !!card.server.detail && card.server.detail !== card.server.url
        text: card.server.detail || ""
        color: Theme.badText
        font.pixelSize: 12
        wrapMode: Text.WrapAnywhere
        Layout.fillWidth: true
    }
    // The button sits at the bottom, level with the other tiles' (Overview)
    Item { Layout.fillHeight: true }
    AppButton {
        Layout.topMargin: 4
        text: card.running || card.state === "starting" ? qsTr("Stop server") : qsTr("Start server")
        iconPath: card.running || card.state === "starting" ? Icons.stop : Icons.play
        iconFilled: true
        primary: !card.running && card.state !== "starting" && card.state !== "stopping"
        danger: card.running || card.state === "starting"
        enabled: card.state !== "stopping"
        onClicked: card.running || card.state === "starting" ? Studio.stopServer() : Studio.startServer()
    }
}
