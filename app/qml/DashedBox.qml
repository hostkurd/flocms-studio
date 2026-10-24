import QtQuick
import QtQuick.Shapes

// A rounded rectangle with a dashed border ("Open existing folder…").
Shape {
    id: box
    property color color: Theme.borderStrong
    property real radius: 8
    layer.enabled: true
    layer.samples: 4
    ShapePath {
        strokeColor: box.color
        strokeWidth: 1
        fillColor: "transparent"
        strokeStyle: ShapePath.DashLine
        dashPattern: [4, 3]
        PathSvg {
            readonly property real w: box.width - 1
            readonly property real h: box.height - 1
            readonly property real r: box.radius
            path: "M %1 0.5 H %2 A %3 %3 0 0 1 %4 %5 V %6 A %3 %3 0 0 1 %2 %7 H %1 A %3 %3 0 0 1 0.5 %6 V %5 A %3 %3 0 0 1 %1 0.5 Z"
                .arg(r + 0.5).arg(w - r + 0.5).arg(r).arg(w + 0.5).arg(r + 0.5).arg(h - r + 0.5).arg(h + 0.5)
        }
    }
}
