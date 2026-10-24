import QtQuick
import QtQuick.Layouts
import FloStudio.Backend

// The last problem (Studio.problem), until dismissed.
Rectangle {
    id: bar
    visible: Studio.problem.code !== undefined
    implicitHeight: row.implicitHeight + 20
    radius: 10
    color: Theme.badBg
    border.color: Theme.bad
    border.width: 1
    Accessible.role: Accessible.AlertMessage

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: 10
        anchors.leftMargin: 14
        spacing: 12
        Icon { path: Icons.warning; color: Theme.bad; size: 18; Layout.alignment: Qt.AlignTop }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            RichText {
                raw: Studio.problem.code !== undefined ? Texts.problem(Studio.problem.code) : ""
                color: Theme.badText
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Mono {
                visible: (Studio.problem.detail || "") !== ""
                text: Studio.problem.detail || ""
                color: Theme.badText
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
            }
        }
        AppButton { text: qsTr("Dismiss"); small: true; onClicked: Studio.dismissProblem() }
    }
}
