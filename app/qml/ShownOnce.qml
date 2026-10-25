import QtQuick
import QtQuick.Layouts

// One page of a StackLayout, made the first time the stack shows it and
// kept after that: switching back is as quick as before and what was
// typed or scrolled stays, while pages never opened cost nothing.
// `index` is this page's place in the stack.
Loader {
    property int index: 0
    readonly property bool shown: parent && parent.currentIndex === index
    property bool wanted: shown
    onShownChanged: if (shown) wanted = true
    active: wanted
    Layout.fillWidth: true
    Layout.fillHeight: true
}
