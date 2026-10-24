import QtQuick
import QtQuick.Shapes
import FloStudio.Backend

// A 24x24 stroke icon from an SVG path. Icons that point a way (back,
// forward) set `directional` and are drawn flipped in right-to-left
// languages (Icons.mirrored).
Item {
    id: icon
    property string path
    property color color: Theme.text
    property real stroke: 2
    property int size: 16
    property bool filled: false
    property bool directional: false

    implicitWidth: size
    implicitHeight: size

    Shape {
        width: 24
        height: 24
        scale: icon.size / 24
        anchors.centerIn: parent
        layer.enabled: true
        layer.samples: 4
        ShapePath {
            strokeColor: icon.filled ? "transparent" : icon.color
            fillColor: icon.filled ? icon.color : "transparent"
            strokeWidth: icon.stroke
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: icon.directional && Lang.rtl ? Icons.mirrored(icon.path) : icon.path }
        }
    }
}
