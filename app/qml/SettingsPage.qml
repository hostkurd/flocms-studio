import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import FloStudio.Backend

// Board 6: Settings, one card per section; the sidebar jumps to them.
// Saved passwords come with the security library (stage 6), Updates with
// the first release (stage 9).
ScrollView {
    id: page
    signal downloadRequested()
    contentWidth: availableWidth
    clip: true

    readonly property var sections: ({ floserver: floserver, general: general, tools: tools, database: database, server: server, editor: editor, folders: folders })
    // Where showSection last put the view: anything else is the person
    // scrolling (a wheel, keys), which ends the settling below
    property real shownY: -1
    function showSection() {
        const card = page.sections[Studio.settingsSection]
        if (card && Studio.page === "settings") {
            page.contentItem.contentY = Math.max(0, Math.min(card.y - 24, page.contentHeight - page.height))
            page.shownY = page.contentItem.contentY
        }
    }
    Connections {
        target: Studio
        function onPageChanged() { page.showSection() }
    }
    // The page is made when first shown (ShownOnce), after the page change
    Component.onCompleted: Qt.callLater(page.showSection)
    // Opened on a section: kept there while the cards take their places,
    // for a moment (a demo: 1.5 s) or until the person scrolls
    property bool settling: true
    property int settleTicks: 0
    Timer {
        interval: 100
        repeat: true
        running: page.settling
        onTriggered: {
            if (page.shownY >= 0 && Math.abs(page.contentItem.contentY - page.shownY) > 1) {
                page.settling = false
                return
            }
            page.showSection()
            if (++page.settleTicks >= (Studio.demo ? 15 : 5)) page.settling = false
        }
    }
    Connections {
        target: page.contentItem
        function onMovementStarted() { page.settling = false }
    }
    Connections {
        target: page.ScrollBar.vertical
        function onPressedChanged() { page.settling = false }
    }

    FileDialog {
        id: pickPhp
        title: qsTr("Choose PHP")
        onAccepted: Studio.setPhpPath(Studio.localPath(selectedFile))
    }
    FileDialog {
        id: pickComposer
        title: qsTr("Choose Composer")
        nameFilters: [qsTr("Composer (composer composer.phar composer.bat)"), qsTr("All files (*)")]
        onAccepted: Studio.setComposerPath(Studio.localPath(selectedFile))
    }
    FolderDialog {
        id: addFolder
        title: qsTr("The folder your FloCMS projects are in")
        onAccepted: Studio.addProjectFolder(selectedFolder)
    }

    ColumnLayout {
        width: Math.min(page.availableWidth, 920)
        spacing: 16

        Heading { level: 1; ui: qsTr("Settings"); Layout.topMargin: 24; Layout.leftMargin: 28 }

        // --- FloServer (design D14): its PHP and Composer, per project ---
        Card {
            id: floserver
            readonly property bool found: !!Studio.floServer.found
            Layout.fillWidth: true
            Layout.leftMargin: 28
            Layout.rightMargin: 28
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Heading { text: "FloServer"; Layout.fillWidth: true }
                Pill {
                    visible: floserver.found
                    text: qsTr("FloServer %1 found").arg(Studio.floServer.version || "")
                    color: Theme.okBg
                    fg: Theme.okText
                }
            }
            AppSwitch {
                // Can be turned on only while FloServer is there, and off always
                // (FloServer gone: Studio uses the PHP and Composer of before)
                checked: Studio.useFloServer
                enabled: floserver.found || Studio.useFloServer
                text: qsTr("Use FloServer's PHP and Composer")
                Layout.fillWidth: true
                onToggled: Studio.setUseFloServer(checked)
            }
            AppText {
                visible: floserver.found
                ui: qsTr("Turning this off brings back the PHP and Composer you had before.")
                color: Theme.muted
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            AppText {
                visible: !floserver.found && Studio.useFloServer
                ui: qsTr("FloServer can't be found now, so Studio uses the PHP and Composer below until it is back.")
                color: Theme.warnText
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            AppText {
                visible: !floserver.found
                ui: qsTr("FloServer isn't installed on this computer.")
                color: Theme.muted
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        // --- General ---
        Card {
            id: general
            Layout.fillWidth: true
            Layout.leftMargin: 28
            Layout.rightMargin: 28
            Heading { ui: qsTr("General") }
            GridLayout {
                columns: page.availableWidth > 760 ? 2 : 1
                columnSpacing: 24
                rowSpacing: 18
                Layout.fillWidth: true
                ColumnLayout {
                    spacing: 8
                    AppText { ui: qsTr("Language"); font.weight: Font.DemiBold }
                    Segmented {
                        label: qsTr("Language")
                        value: Studio.language
                        options: [
                            { value: "en", text: "English" },
                            { value: "ar", text: "العربية", lang: "ar" },
                            { value: "ckb", text: "کوردی", lang: "ckb" },
                            { value: "system", text: qsTr("System") }
                        ]
                        onChosen: (value) => Studio.setLanguage(value)
                    }
                    AppText {
                        ui: qsTr("Changes right away. Arabic and Kurdish turn the whole window right to left.")
                        color: Theme.muted
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }
                ColumnLayout {
                    spacing: 8
                    AppText { ui: qsTr("Theme"); font.weight: Font.DemiBold }
                    Segmented {
                        label: qsTr("Theme")
                        value: Studio.theme
                        options: [
                            { value: "light", text: qsTr("Light") },
                            { value: "dark", text: qsTr("Dark") },
                            { value: "system", text: qsTr("System") }
                        ]
                        onChosen: (value) => Studio.setTheme(value)
                    }
                    AppText {
                        ui: qsTr("System follows the computer's dark mode.")
                        color: Theme.muted
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // --- PHP and tools ---
        Card {
            id: tools
            Layout.fillWidth: true
            Layout.leftMargin: 28
            Layout.rightMargin: 28
            // FloServer's in use (D14): each project's PHP from its floserver.json
            readonly property bool floServer: Studio.useFloServer && !!Studio.floServer.found
            Heading { ui: qsTr("PHP and tools") }
            AppText { ui: qsTr("PHP to use"); font.weight: Font.DemiBold }
            ColumnLayout {
                visible: tools.floServer
                spacing: 6
                Layout.fillWidth: true
                AppText {
                    ui: qsTr("FloServer, per project (the version in each project's floserver.json)")
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Flow {
                    visible: (Studio.floServer.php || []).length > 0
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: Studio.floServer.php || []
                        delegate: Pill {
                            required property string modelData
                            text: "PHP " + modelData
                            mono: true
                            color: Theme.rule
                            fg: Theme.navText
                        }
                    }
                }
                AppText {
                    visible: (Studio.floServer.php || []).length === 0
                    ui: qsTr("FloServer has no PHP installed yet.")
                    color: Theme.warnText
                    font.pixelSize: 13
                }
            }
            ButtonGroup { id: phpGroup }
            Repeater {
                model: tools.floServer ? [] : Studio.phpChoices
                delegate: ChoiceRow {
                    required property var modelData
                    Layout.fillWidth: true
                    group: phpGroup
                    path: modelData.path
                    checked: Studio.phpPath === "" ? modelData.origin === "system" : Studio.phpPath === modelData.path
                    detail: modelData.origin === "studio" ? qsTr("PHP %1 · downloaded by Studio").arg(modelData.version)
                          : modelData.origin === "system" ? qsTr("PHP %1 · found on this computer").arg(modelData.version)
                          : qsTr("PHP %1 · chosen by you").arg(modelData.version)
                    onPicked: Studio.setPhpPath(modelData.origin === "system" ? "" : modelData.path)
                }
            }
            AppText {
                visible: !tools.floServer && Studio.phpChoices.length === 0 && !Studio.checkingTools
                ui: qsTr("No PHP found on this computer.")
                color: Theme.badText
            }
            // FloServer installs its own PHP and Composer: only MariaDB is
            // still downloaded here then (Studio's database server)
            Flow {
                Layout.fillWidth: true
                spacing: 8
                AppButton { visible: !tools.floServer; text: qsTr("Choose another…"); onClicked: pickPhp.open() }
                AppButton { visible: !tools.floServer; text: qsTr("Search again"); iconPath: Icons.refresh; enabled: !Studio.checkingTools; onClicked: Studio.refreshTools() }
                AppButton {
                    visible: !tools.floServer || !Studio.mariadbFromHomebrew
                    text: tools.floServer ? qsTr("Download MariaDB…") : qsTr("Download PHP, Composer or MariaDB…")
                    iconPath: Icons.download
                    onClicked: page.downloadRequested()
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.rule; Layout.topMargin: 6; Layout.bottomMargin: 6 }
            AppText { ui: qsTr("Composer"); font.weight: Font.DemiBold }
            AppText {
                visible: tools.floServer
                ui: Studio.floServer.composer ? qsTr("FloServer's Composer %1").arg(Studio.floServer.composer)
                                              : qsTr("FloServer has no Composer installed yet.")
                color: Studio.floServer.composer ? Theme.text : Theme.warnText
                Layout.fillWidth: true
            }
            RowLayout {
                visible: !tools.floServer
                Layout.fillWidth: true
                spacing: 8
                Mono {
                    readonly property var row: Studio.tools.find(t => t.tool === "composer")
                    // A path, or "Not found" (translated: ui adds the RTL mark)
                    ui: Studio.composerPath !== "" ? Studio.composerPath : (row && row.path ? row.path : qsTr("Not found"))
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
                AppButton { text: qsTr("Choose…"); small: true; onClicked: pickComposer.open() }
                AppButton { visible: Studio.composerPath !== ""; text: qsTr("Use the one found"); small: true; onClicked: Studio.setComposerPath("") }
            }
            RichText {
                raw: qsTr("Downloads go into `%1`, for your user only. No administrator rights are needed.").arg(Studio.toolsFolder)
                color: Theme.muted
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        // --- Database server (MariaDB design note D9) ---
        DatabaseSettings {
            id: database
            Layout.fillWidth: true
            Layout.leftMargin: 28
            Layout.rightMargin: 28
            onDownloadRequested: page.downloadRequested()
        }

        // --- Server ---
        Card {
            id: server
            Layout.fillWidth: true
            Layout.leftMargin: 28
            Layout.rightMargin: 28
            Heading { ui: qsTr("Server") }
            RowLayout {
                spacing: 12
                AppText { ui: qsTr("First port to try"); font.weight: Font.DemiBold }
                Field {
                    id: port
                    mono: true
                    implicitWidth: 110
                    text: String(Studio.serverPort) // a port, not a number: no 8,000
                    validator: IntValidator { bottom: 1024; top: 65535 }
                    inputMethodHints: Qt.ImhDigitsOnly
                    Accessible.name: qsTr("First port to try")
                    onEditingFinished: if (acceptableInput) Studio.setServerPort(Number(text))
                }
            }
            AppCheck {
                checked: Studio.serverNetwork
                text: qsTr("Let other devices on the network open the site (phones for testing)")
                Layout.fillWidth: true
                onToggled: Studio.setServerNetwork(checked)
            }
            RichText {
                raw: qsTr("`php flo serve` takes the next free port when this one is in use. On the network, the site listens on `0.0.0.0`: only do this on networks you trust.")
                color: Theme.muted
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        // --- Editor ---
        Card {
            id: editor
            Layout.fillWidth: true
            Layout.leftMargin: 28
            Layout.rightMargin: 28
            Heading { ui: qsTr("Editor") }
            Field {
                Layout.fillWidth: true
                mono: true
                text: Studio.editorCommand
                Accessible.name: qsTr("Editor command")
                onEditingFinished: Studio.setEditorCommand(text)
            }
            RichText {
                raw: qsTr("`{folder}` becomes the project's folder; quotes group words. For example `code \"{folder}\"` or `phpstorm {folder}`.")
                color: Theme.muted
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            // Open terminal (design note of 30 Oct, B-D2)
            Heading { ui: qsTr("Terminal"); Layout.topMargin: 10 }
            Field {
                Layout.fillWidth: true
                mono: true
                text: Studio.terminalCommand
                placeholderText: Qt.platform.os === "windows" ? qsTr("Automatic: Windows Terminal, else PowerShell")
                               : (Qt.platform.os === "osx" ? qsTr("Automatic: Terminal") : qsTr("Automatic: this system's terminal"))
                Accessible.name: qsTr("Terminal command")
                onEditingFinished: Studio.setTerminalCommand(text)
            }
            RichText {
                raw: qsTr("Empty: Studio picks the terminal and puts its PHP and Composer first in it. Or a command with `{folder}`, for example `wezterm start --cwd \"{folder}\"`.")
                color: Theme.muted
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        // --- Project folders ---
        Card {
            id: folders
            Layout.fillWidth: true
            Layout.leftMargin: 28
            Layout.rightMargin: 28
            Layout.bottomMargin: 28
            Heading { ui: qsTr("Project folders") }
            AppText {
                ui: qsTr("Studio lists every FloCMS project inside these folders.")
                color: Theme.muted
                font.pixelSize: 13
            }
            Repeater {
                model: Studio.projectFolders
                delegate: RowLayout {
                    required property string modelData
                    Layout.fillWidth: true
                    Mono { text: modelData; elide: Text.ElideMiddle; Layout.fillWidth: true }
                    AppButton { text: qsTr("Remove"); small: true; onClicked: Studio.removeProjectFolder(modelData) }
                }
            }
            AppButton { text: qsTr("Add a folder…"); iconPath: Icons.plus; onClicked: addFolder.open() }
        }
    }
}
