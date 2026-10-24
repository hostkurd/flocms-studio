import QtQuick
import QtQuick.Layouts
import RtlCheck.Backend

// One Doctor check: status icon, label (may hold `code`), detail at the end.
// ltr: true for raw CLI output lines, which always read left to right.
Rectangle {
    id: row
    property string status: "ok" // ok | warn | fail
    property string label
    property string detail
    property bool detailMono: true
    property bool detailLink: false
    property bool ltr: false
    property bool last: false

    LayoutMirroring.enabled: !ltr && Lang.rtl
    LayoutMirroring.childrenInherit: true

    Layout.fillWidth: true
    implicitHeight: Math.max(40, line.implicitHeight + (ltr ? 16 : 0))
    radius: ltr && status !== "ok" ? 6 : 0
    color: !ltr || status === "ok" ? "transparent"
         : (status === "warn" ? Theme.warnRow : Theme.badRow)

    Rectangle {
        visible: !row.last && !(row.ltr && row.status !== "ok")
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.rule
    }

    RowLayout {
        id: line
        anchors.fill: parent
        anchors.leftMargin: row.ltr ? 8 : 0
        anchors.rightMargin: row.ltr ? 8 : 0
        anchors.topMargin: row.ltr ? 8 : 0
        anchors.bottomMargin: row.ltr ? 8 : 0
        spacing: 10

        Canvas {
            id: icon
            Accessible.role: Accessible.Graphic
            Accessible.name: row.status === "ok" ? qsTr("Passed")
                           : (row.status === "warn" ? qsTr("Warning") : qsTr("Problem"))
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            Layout.alignment: row.ltr ? Qt.AlignTop : Qt.AlignVCenter
            Layout.topMargin: row.ltr ? 1 : 0
            property color tint: row.status === "ok" ? Theme.ok
                               : (row.status === "warn" ? Theme.warn : Theme.bad)
            onTintChanged: requestPaint()
            onPaint: {
                const c = getContext("2d");
                c.reset();
                c.strokeStyle = tint;
                c.lineWidth = 2.2;
                c.lineCap = "round";
                c.lineJoin = "round";
                c.scale(18 / 24, 18 / 24);
                c.beginPath();
                if (row.status === "ok") {
                    c.moveTo(5, 12); c.lineTo(10, 17); c.lineTo(19, 7);
                } else if (row.status === "warn") {
                    c.moveTo(12, 3); c.lineTo(2, 20); c.lineTo(22, 20); c.closePath();
                    c.moveTo(12, 10); c.lineTo(12, 14);
                    c.moveTo(12, 17); c.lineTo(12, 17.1);
                } else {
                    c.moveTo(6, 6); c.lineTo(18, 18);
                    c.moveTo(18, 6); c.lineTo(6, 18);
                }
                c.stroke();
            }
        }

        RichText {
            raw: row.label
            plain: row.ltr
            Layout.fillWidth: true
            Layout.alignment: row.ltr ? Qt.AlignTop : Qt.AlignVCenter
            wrapMode: Text.Wrap
            font.pixelSize: row.ltr ? 13 : 14
        }

        RichText {
            visible: row.detail !== ""
            raw: row.detail
            plain: row.detailMono
            font.family: row.detailMono ? Theme.monoFont : Lang.uiFont
            font.pixelSize: row.detailMono ? 12 : 13
            font.underline: row.detailLink
            color: row.detailLink ? Theme.link
                 : (row.status === "ok" ? Theme.muted : Theme.warnText)
        }
    }
}
