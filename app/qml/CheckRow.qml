import QtQuick
import QtQuick.Layouts
import FloStudio.Backend

// One check of `php flo doctor` (boards 1 and 7): its status, its name in
// the person's language, and FloCMS's detail as it printed it.
RowLayout {
    id: check
    property string status: "ok"
    property string name
    property string detail
    implicitHeight: Math.max(40, label.implicitHeight + 12)
    spacing: 10

    StatusIcon { status: check.status }
    RichText {
        id: label
        raw: check.name
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    Mono {
        text: check.detail
        color: check.status === "ok" ? Theme.muted : (check.status === "fail" ? Theme.badText : Theme.warnText)
        font.pixelSize: 12
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight
        Layout.maximumWidth: 280
    }
}
