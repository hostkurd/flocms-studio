import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// The Overview's Doctor tile (redesign boards 1-3): one line for the
// result, the first two checks that need a look, when it ran. The whole
// list is on the Doctor tab.
Card {
    id: card
    property var openTab: function(name) {}
    readonly property var doctor: Studio.checks ? Studio.checks.doctor : ({})
    readonly property int failed: doctor.failed || 0
    readonly property int warnings: doctor.warnings || 0
    readonly property bool checking: doctor.state === "running" && !doctor.checks
    readonly property var needLook: (doctor.checks || []).filter(c => c.status !== "ok")
    property real now: Date.now()
    Timer { interval: 30000; repeat: true; running: card.visible; onTriggered: card.now = Date.now() }
    readonly property int minutes: doctor.checkedAt ? Math.max(0, Math.floor((now - Date.parse(doctor.checkedAt)) / 60000)) : -1

    SectionLabel { ui: qsTr("Doctor") }
    RowLayout {
        spacing: 8
        Layout.fillWidth: true
        StatusIcon {
            visible: !card.checking && !!card.doctor.checks
            status: card.failed > 0 ? "fail" : (card.warnings > 0 ? "warn" : "ok")
        }
        Heading {
            level: 2
            ui: card.checking ? qsTr("Checking…")
                : card.failed > 0 ? qsTr("%Ln problem(s)", "", card.failed)
                : card.warnings > 0 ? qsTr("%Ln warning(s)", "", card.warnings)
                : card.doctor.checks ? qsTr("All checks pass") : qsTr("Not checked yet")
            color: card.checking || !card.doctor.checks ? Theme.text
                 : card.failed > 0 ? Theme.badText : (card.warnings > 0 ? Theme.warnText : Theme.okText)
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }
    ProblemText { report: card.doctor }
    // What needs a look first: its name and what FloCMS said
    Repeater {
        model: card.needLook.slice(0, 2)
        delegate: RichText {
            required property var modelData
            // FloCMS's detail is English: an isolated left-to-right run (an
            // override inside, as Lang.rich does for code words), so it
            // reads the same inside an Arabic or Kurdish line
            raw: Texts.checkName(modelData.check) + (modelData.detail ? " · \u2066\u202D" + modelData.detail + "\u202C\u2069" : "")
            color: Theme.navText
            font.pixelSize: 13
            wrapMode: Text.Wrap // rich text can't be cut with "…"
            Layout.fillWidth: true
        }
    }
    AppText {
        visible: card.needLook.length > 2
        ui: qsTr("and %Ln more", "", card.needLook.length - 2)
        color: Theme.muted
        font.pixelSize: 13
    }
    AppText {
        visible: card.minutes >= 0
        ui: card.minutes < 1 ? qsTr("Last checked: just now") : qsTr("Last checked: %Ln min ago", "", card.minutes)
        color: Theme.muted
        font.pixelSize: 13
    }
    // The buttons sit at the bottom, level with the other tiles'
    Item { Layout.fillHeight: true }
    Flow {
        spacing: 8
        Layout.fillWidth: true
        Layout.topMargin: 4
        AppButton {
            text: qsTr("Run again")
            enabled: !!Studio.checks && card.doctor.state !== "running"
            onClicked: Studio.checks.runDoctor()
        }
        AppButton {
            text: qsTr("All checks")
            onClicked: card.openTab("doctor")
        }
    }
}
