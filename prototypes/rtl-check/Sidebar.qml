import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import RtlCheck.Backend

Rectangle {
    id: sidebar
    property bool dark: false
    signal darkToggled(bool on)

    color: Theme.sidebar

    // The border sits on the end side: right in LTR, left in RTL.
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 20
        anchors.bottomMargin: 16
        anchors.leftMargin: 14
        anchors.rightMargin: 15
        spacing: 6

        RowLayout {
            spacing: 10
            Layout.leftMargin: 6
            Layout.bottomMargin: 14
            Rectangle {
                width: 32; height: 32; radius: 8
                color: Theme.brand
                AppText { anchors.centerIn: parent; text: "F"; color: Theme.brandText; font.bold: true; font.pixelSize: 17 }
            }
            AppText { text: "FloCMS Studio"; font.bold: true; font.pixelSize: 17 }
        }

        AppText {
            text: qsTr("Projects")
            color: Theme.muted
            font.pixelSize: 13
            font.bold: true
            Layout.leftMargin: 10
        }

        TextField {
            id: search
            Layout.fillWidth: true
            Layout.bottomMargin: 4
            implicitHeight: 38
            placeholderText: qsTr("Search projects")
            placeholderTextColor: Theme.muted
            color: Theme.text
            font.family: Lang.uiFont
            font.pixelSize: 14
            horizontalAlignment: TextInput.AlignLeft
            selectionColor: Theme.accent
            selectedTextColor: Theme.accentText
            background: Rectangle {
                radius: 8
                color: Theme.bg
                border.color: search.activeFocus ? Theme.accent : Theme.borderStrong
            }
        }

        Repeater {
            model: [
                { name: "fectumhellas", running: true },
                { name: "flocms-website", running: false },
                { name: "admin-playground", running: false },
                { name: "ar-lighting-backend", running: false }
            ]
            delegate: Rectangle {
                required property var modelData
                visible: search.text === "" || modelData.name.indexOf(search.text) >= 0
                Layout.fillWidth: true
                implicitHeight: 38
                radius: 8
                color: modelData.running ? Theme.selectedBg : "transparent"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 10
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: modelData.running ? Theme.ok : Theme.idle
                    }
                    AppText {
                        text: modelData.name
                        color: modelData.running ? Theme.selectedText : Theme.navText
                        font.bold: modelData.running
                        Layout.fillWidth: true
                    }
                    AppText {
                        visible: modelData.running
                        text: qsTr("Running")
                        color: Theme.okText
                        font.pixelSize: 12
                    }
                }
            }
        }

        AppButton {
            text: qsTr("New project")
            Layout.fillWidth: true
            Layout.topMargin: 6
        }

        Item { Layout.fillHeight: true }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border; Layout.bottomMargin: 8 }

        AppText {
            text: qsTr("This computer")
            color: Theme.muted
            font.pixelSize: 13
            font.bold: true
            Layout.leftMargin: 6
        }

        GridLayout {
            columns: 2
            columnSpacing: 12
            rowSpacing: 6
            Layout.fillWidth: true
            Layout.leftMargin: 6
            Layout.rightMargin: 6

            AppText { text: "PHP"; Layout.fillWidth: true }
            AppText { text: "8.4 ✓"; font.family: Theme.monoFont; font.pixelSize: 13; color: Theme.okText }
            AppText { text: "MariaDB"; Layout.fillWidth: true }
            RichText { raw: qsTr("Running · `%1`").arg("11.8"); color: Theme.okText; font.pixelSize: 13 }
            AppText { text: "Composer"; Layout.fillWidth: true }
            AppText { text: "2.8 ✓"; font.family: Theme.monoFont; font.pixelSize: 13; color: Theme.okText }
        }

        // Language and theme switches, to test switching while the app runs.
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 10
            spacing: 6
            Repeater {
                model: [
                    { code: "en", label: "English" },
                    { code: "ar", label: "العربية" },
                    { code: "ckb", label: "کوردی" }
                ]
                delegate: AppButton {
                    required property var modelData
                    small: true
                    checkable: true
                    checked: Lang.code === modelData.code
                    text: modelData.label
                    font.family: modelData.code === "en" ? "Noto Sans" : "Noto Sans Arabic"
                    Layout.fillWidth: true
                    onClicked: Lang.set(modelData.code)
                }
            }
        }

        AppButton {
            small: true
            checkable: true
            checked: sidebar.dark
            text: qsTr("Dark theme")
            Layout.fillWidth: true
            onToggled: sidebar.darkToggled(checked)
        }

        // A directional icon: the chevron points to the end side, so it
        // must flip in RTL.
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 6
            Layout.topMargin: 6
            spacing: 8
            AppText { text: qsTr("Settings"); color: Theme.navText; Layout.fillWidth: true }
            Canvas {
                width: 16; height: 16
                property color tint: Theme.navText
                onTintChanged: requestPaint()
                transform: Scale { origin.x: 8; xScale: Lang.rtl ? -1 : 1 }
                onPaint: {
                    const c = getContext("2d");
                    c.reset();
                    c.strokeStyle = tint;
                    c.lineWidth = 2;
                    c.lineCap = "round";
                    c.lineJoin = "round";
                    c.beginPath();
                    c.moveTo(6, 3); c.lineTo(11, 8); c.lineTo(6, 13);
                    c.stroke();
                }
            }
        }
    }
}
