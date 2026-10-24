import QtQuick
import QtQuick.Layouts
import FloStudio.Backend

// A labelled field of the wizard (board 6): the label, the control, a hint
// or, once touched, what is wrong with it.
ColumnLayout {
    id: field
    property string label
    property string hint // may hold `code` words
    property string problem // a code from Core's checkNewProject
    default property alias content: holder.data
    spacing: 6

    AppText { ui: field.label; font.weight: Font.DemiBold }
    ColumnLayout { id: holder; Layout.fillWidth: true; spacing: 0 }
    RichText {
        visible: field.problem !== "" || field.hint !== ""
        raw: field.problem !== "" ? Texts.fieldProblem(field.problem) : field.hint
        color: field.problem !== "" ? Theme.badText : Theme.muted
        font.pixelSize: 13
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
}
