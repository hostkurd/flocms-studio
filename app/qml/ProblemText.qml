import QtQuick
import QtQuick.Layouts
import FloStudio.Backend

// A report that failed (ProjectChecks): what went wrong, the message
// flocms-cli printed, and its full output behind a button. Nothing is
// hidden (D4).
ColumnLayout {
    id: problem
    property var report: ({})
    property bool expanded: false
    visible: report.state === "failed"
    spacing: 6
    Layout.fillWidth: true

    RichText {
        raw: problem.report.error ? Texts.problem(problem.report.error) : ""
        color: Theme.badText
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    Mono {
        visible: !!problem.report.detail
        text: problem.report.detail || ""
        color: Theme.badText
        font.pixelSize: 12
        wrapMode: Text.WrapAnywhere
        Layout.fillWidth: true
    }
    AppButton {
        visible: !!problem.report.output
        small: true
        text: problem.expanded ? qsTr("Hide the output") : qsTr("Show the output")
        onClicked: problem.expanded = !problem.expanded
    }
    CommandOutput {
        visible: problem.expanded
        bare: true
        output: ({ state: "failed", shown: "", command: "", lines: (problem.report.output || "").split("\n"), exitCode: "" })
        Layout.fillWidth: true
    }
}
