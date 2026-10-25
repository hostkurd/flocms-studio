import QtQuick
import QtQuick.Layouts
import FloStudio.Backend

// Why Studio's MariaDB didn't start (MariaDB boards 2 and 6), with what to
// do: another port, a library to install, or the end of its log.
Rectangle {
    id: box
    readonly property var server: Studio.mariadb
    readonly property string code: server.state === "failed" ? server.error : ""
    readonly property bool portBusy: code === "db_port_busy"
    readonly property bool libaio: code === "db_library" && server.detail === "libaio.so.1"
    visible: code !== ""
    implicitHeight: row.implicitHeight + 24
    radius: 10
    color: portBusy ? Theme.warnBg : Theme.badBg
    border.color: portBusy ? Theme.warn : Theme.bad
    Accessible.role: Accessible.AlertMessage

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: 12
        anchors.leftMargin: 14
        spacing: 12
        Icon { path: Icons.warning; color: box.portBusy ? Theme.warn : Theme.bad; size: 18; Layout.alignment: Qt.AlignTop }
        // A Column, not a ColumnLayout: wrapped text there gets the height
        // it had at its first, narrow width
        Column {
            id: column
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 8
            RichText {
                raw: box.portBusy
                     ? (box.server.freePort > 0
                        ? qsTr("Port `%1` is in use by another program. Studio can start its MariaDB on port `%2`.").arg(box.server.port).arg(box.server.freePort)
                        : qsTr("Port `%1` is in use by another program. Choose another port below.").arg(box.server.port))
                     : (box.code !== "" ? Texts.problem(box.code) : "")
                color: box.portBusy ? Theme.warnText : Theme.badText
                wrapMode: Text.Wrap
                width: column.width
            }
            // A library: the command that installs it, to copy
            RichText {
                visible: box.code === "db_library"
                raw: box.libaio
                     ? qsTr("It can't start without `libaio.so.1`. Install it once, then try again. Studio doesn't change system folders itself.")
                     : qsTr("It can't start without `%1`. Install it with your package manager, then try again.").arg(box.server.detail)
                color: Theme.badText
                wrapMode: Text.Wrap
                width: column.width
            }
            Rectangle {
                visible: box.libaio
                width: column.width
                implicitHeight: 40
                radius: 8
                color: Theme.surface
                border.color: Theme.border
                LayoutMirroring.enabled: false
                LayoutMirroring.childrenInherit: true
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 6
                    Mono { text: "sudo apt install libaio1t64"; Layout.fillWidth: true }
                    AppButton { text: qsTr("Copy"); small: true; onClicked: box.server.copyText("sudo apt install libaio1t64") }
                }
            }
            RichText {
                visible: box.libaio
                raw: qsTr("Ubuntu 24.04 and newer call the file `libaio.so.1t64`; Studio points its own copy of MariaDB at it, inside Studio's folder. Other systems: the `libaio` package.")
                color: Theme.badText
                font.pixelSize: 13
                wrapMode: Text.Wrap
                width: column.width
            }
            Mono {
                visible: !box.portBusy && box.code !== "db_library" && box.server.detail !== ""
                text: box.server.detail
                color: Theme.badText
                font.pixelSize: 12
                wrapMode: Text.WrapAnywhere
                width: column.width
                LayoutMirroring.enabled: false
            }
        }
        AppButton {
            visible: box.portBusy && box.server.freePort > 0
            primary: true
            text: qsTr("Start on `%1`").arg(box.server.freePort)
            Layout.alignment: Qt.AlignTop
            onClicked: box.server.startOnFreePort()
        }
        AppButton {
            visible: !box.portBusy
            text: qsTr("Try again")
            Layout.alignment: Qt.AlignTop
            onClicked: box.server.start()
        }
    }
}
