import QtQuick
import FloStudio.Backend

// A check's status as on boards 1 and 7: a tick, a warning sign, a cross.
Icon {
    property string status: "ok" // ok, warn, fail, note, running
    path: status === "ok" ? Icons.check : (status === "warn" ? Icons.warning : (status === "fail" ? Icons.cross : (status === "running" ? Icons.play : Icons.dot)))
    color: status === "ok" ? Theme.ok : (status === "warn" ? Theme.warn : (status === "fail" ? Theme.bad : (status === "running" ? Theme.accent : Theme.idle)))
    filled: status === "running"
    stroke: status === "ok" || status === "fail" ? 2.4 : 2.2
    size: 18
    Accessible.role: Accessible.Graphic
    Accessible.name: status === "ok" ? qsTr("OK") : (status === "warn" ? qsTr("Warning") : (status === "fail" ? qsTr("Problem") : (status === "running" ? qsTr("Running…") : qsTr("Note"))))
}
