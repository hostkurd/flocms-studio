pragma Singleton
import QtQuick

// The flocms-admin colours from the approved boards, light and dark.
QtObject {
    property bool dark: false

    readonly property color bg: dark ? "#0E161C" : "#F4F7F9"
    readonly property color surface: dark ? "#15212A" : "#FFFFFF"
    readonly property color sidebar: dark ? "#111C24" : "#FFFFFF"
    readonly property color border: dark ? "#24333E" : "#E1E7EC"
    readonly property color borderStrong: dark ? "#2C3D49" : "#D5DEE5"
    readonly property color rule: dark ? "#1F2C36" : "#EEF2F5"
    readonly property color text: dark ? "#E4ECF1" : "#14212B"
    readonly property color muted: dark ? "#9DB0BC" : "#56656F"
    readonly property color navText: dark ? "#C3D1DA" : "#33434E"
    readonly property color link: dark ? "#7FD1EC" : "#0B6E91"
    readonly property color accent: dark ? "#38BBE0" : "#0B6E91"
    readonly property color accentText: dark ? "#05303F" : "#FFFFFF"
    readonly property color selectedBg: dark ? "#18323F" : "#E7F4F9"
    readonly property color selectedText: dark ? "#E4ECF1" : "#0B4F68"
    readonly property color brand: "#38BBE0"
    readonly property color brandText: "#05303F"
    readonly property color idle: dark ? "#5F7180" : "#9AA7B1"

    readonly property color ok: dark ? "#4CC38A" : "#1E7F4F"
    readonly property color okText: dark ? "#7FDBAA" : "#1E6B45"
    readonly property color okBg: dark ? "#143526" : "#E3F4EA"
    readonly property color warn: dark ? "#F2C46D" : "#9A5B00"
    readonly property color warnText: dark ? "#F2C46D" : "#6B4100"
    readonly property color warnBg: dark ? "#3A2E10" : "#FFF1D6"
    readonly property color warnRow: dark ? "#2A2412" : "#FFF8EA"
    readonly property color bad: dark ? "#FF8A8A" : "#A4262C"
    readonly property color badText: dark ? "#FFB4B4" : "#8A1F24"
    readonly property color badBg: dark ? "#3A1A1D" : "#FDE7E7"
    readonly property color badRow: dark ? "#2B1618" : "#FDF0F0"

    readonly property color terminalBg: "#0F1B24"
    readonly property color terminalText: "#C9D6DF"
    readonly property color terminalPrompt: "#7FD1EC"
    readonly property color terminalOk: "#8BD9AE"

    readonly property string monoFont: "JetBrains Mono"
}
