pragma Singleton
import QtQuick

// The paths of the icons on the approved boards (24x24, stroked).
QtObject {
    readonly property string plus: "M12 5v14M5 12h14"
    readonly property string gear: "M12 9a3 3 0 1 0 0 6a3 3 0 1 0 0-6M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.1 2.1M17 17l2.1 2.1M4.9 19.1L7 17M17 7l2.1-2.1"
    readonly property string globe: "M12 3a9 9 0 1 0 0 18a9 9 0 1 0 0-18M3 12h18M12 3c3 3 3 15 0 18M12 3c-3 3-3 15 0 18"
    readonly property string code: "M8 7l-5 5l5 5M16 7l5 5l-5 5"
    readonly property string folder: "M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"
    readonly property string check: "M5 12l5 5l9-10"
    readonly property string warning: "M12 3L2 20h20L12 3zM12 10v4M12 17h.01"
    readonly property string cross: "M6 6l12 12M18 6L6 18"
    readonly property string back: "M15 6l-6 6l6 6"
    readonly property string download: "M12 4v11M7 10l5 5l5-5M5 20h14"
    readonly property string refresh: "M20 11a8 8 0 1 0-2.3 5.7M20 4v7h-7"
    readonly property string trash: "M4 7h16M9 7V4h6v3M6 7l1 13h10l1-13"
    readonly property string shield: "M12 3l8 3v6c0 5-3.5 8-8 9c-4.5-1-8-4-8-9V6z"
}
