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
    readonly property string dot: "M12 10a2 2 0 1 0 0 4a2 2 0 1 0 0-4"
    readonly property string play: "M8 5v14l11-7z"
    readonly property string stop: "M6 6h12v12H6z"
    readonly property string copy: "M8 8h11v11H8zM5 16V5h11"
    readonly property string file: "M6 3h8l4 4v14H6zM14 3v4h4"
    readonly property string external: "M14 4h6v6M20 4l-9 9M18 14v6H4V6h6"
    readonly property string search: "M11 4a7 7 0 1 0 0 14a7 7 0 1 0 0-14M20 20l-4-4"
    readonly property string arrowDown: "M12 5v14M6 13l6 6l6-6"
    readonly property string download: "M12 4v11M7 10l5 5l5-5M5 20h14"
    readonly property string refresh: "M20 11a8 8 0 1 0-2.3 5.7M20 4v7h-7"
    readonly property string trash: "M4 7h16M9 7V4h6v3M6 7l1 13h10l1-13"
    readonly property string table: "M4 5h16v14H4zM4 10h16M10 10v9"
    readonly property string database: "M12 3c4.4 0 8 1.3 8 3s-3.6 3-8 3s-8-1.3-8-3s3.6-3 8-3M4 6v12c0 1.7 3.6 3 8 3s8-1.3 8-3V6M4 12c0 1.7 3.6 3 8 3s8-1.3 8-3"
    readonly property string bolt: "M13 3L5 13.5h6L10 21l8-10.5h-6z"
    readonly property string shield: "M12 3l8 3v6c0 5-3.5 8-8 9c-4.5-1-8-4-8-9V6z"

    // The path flipped left to right (x → 24 − x), for icons that point a
    // way in right-to-left languages. Drawn flipped rather than transformed:
    // a Scale on the icon was lost when the Shape renders into its layer.
    function mirrored(d) {
        const tokens = d.match(/[A-Za-z]|-?(?:\d*\.\d+|\d+\.?)(?:e-?\d+)?/g) || []
        const sizes = { M: 2, L: 2, T: 2, H: 1, V: 1, C: 6, S: 4, Q: 4, A: 7, Z: 0 }
        let out = []
        let command = ""
        let i = 0
        while (i < tokens.length) {
            if (/[A-Za-z]/.test(tokens[i])) {
                command = tokens[i++]
                out.push(command)
                if (command.toUpperCase() === "Z")
                    continue
            }
            const upper = command.toUpperCase()
            const absolute = command === upper
            const n = sizes[upper]
            const args = tokens.slice(i, i + n).map(Number)
            i += n
            const x = v => absolute ? 24 - v : -v
            if (upper === "A") {
                args[2] = -args[2]          // rotation
                args[4] = 1 - args[4]       // sweep
                args[5] = x(args[5])
            } else if (upper === "H") {
                args[0] = x(args[0])
            } else if (upper !== "V") {
                for (let k = 0; k < n; k += 2)
                    args[k] = x(args[k])
            }
            out.push(args.join(" "))
        }
        return out.join(" ")
    }
}
