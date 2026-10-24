import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Templates as T
import FloStudio.Backend

// Boards 7a and 7b (D15): choose what to download, then watch it arrive.
// Only the files in Studio's tool list, each checked against its SHA-256.
Popup {
    id: dialog
    modal: true
    focus: true
    width: Math.min(560, parent ? parent.width - 48 : 560)
    height: Math.min(implicitHeight, parent ? parent.height - 48 : implicitHeight)
    padding: 24
    closePolicy: Studio.downloading ? Popup.NoAutoClose : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)

    readonly property var offer: Studio.downloadOffer
    readonly property var phps: offer.filter(p => p.tool === "php")
    readonly property var composer: offer.find(p => p.tool === "composer")
    readonly property var mariadb: offer.find(p => p.tool === "mariadb")
    readonly property bool choosing: Studio.downloads.length === 0
    readonly property bool finished: !choosing && !Studio.downloading
    property string phpChoice: ""
    property bool wantComposer: true
    property bool wantMariadb: false

    // What this computer has already (found by Studio, not downloaded)
    function found(tool) {
        const row = Studio.tools.find(t => t.tool === tool)
        return row && (row.state === "found" || row.state === "running") ? row : null
    }
    function missing(tool) {
        const row = Studio.tools.find(t => t.tool === tool)
        return !row || row.state === "missing" || row.state === "unsupported" || row.state === "failed"
    }

    onAboutToShow: {
        const firstNew = phps.find(p => !p.installed)
        phpChoice = missing("php") && firstNew ? firstNew.id : ""
        wantComposer = missing("composer") && !!composer && !composer.installed
        wantMariadb = false
    }
    onClosed: Studio.clearDownloads()

    background: Rectangle {
        color: Theme.surface
        radius: 14
        border.color: Theme.border
    }
    T.Overlay.modal: Rectangle { color: Theme.overlay }

    contentItem: ColumnLayout {
        spacing: 14
        LayoutMirroring.enabled: Lang.rtl
        LayoutMirroring.childrenInherit: true

        Heading {
            ui: dialog.choosing ? qsTr("Download the tools Studio needs") : qsTr("Downloading the tools")
            Layout.fillWidth: true
            Accessible.role: Accessible.Heading
        }

        // Scrolls when the window is short (Arabic text is taller); the
        // buttons below always stay in view
        ScrollView {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: inner.implicitHeight
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                id: inner
                width: body.availableWidth
                spacing: 14

                // --- Choose ---
                ColumnLayout {
                    visible: dialog.choosing
                    spacing: 12
                    Layout.fillWidth: true

                    AppText {
                        ui: qsTr("They go into Studio's own folder for your user. Nothing else on the computer changes, and no administrator rights are needed.")
                        color: Theme.muted
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }

                    SectionLabel { text: "PHP" }
                    ButtonGroup { id: phpGroup }
                    Repeater {
                        model: dialog.phps
                        delegate: OptionRow {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            radio: true
                            group: phpGroup
                            checked: dialog.phpChoice === modelData.id
                            // Already downloaded: nothing to download again
                            enabled: !modelData.installed
                            title: "PHP " + modelData.version
                            note: (index === 0 ? qsTr("Newest · from %1").arg(modelData.source) : qsTr("from %1").arg(modelData.source))
                                  + (modelData.installed ? " · " + qsTr("already downloaded") : "")
                            size: modelData.size
                            onToggled: (on) => { if (on) dialog.phpChoice = modelData.id }
                        }
                    }
                    OptionRow {
                        Layout.fillWidth: true
                        radio: true
                        group: phpGroup
                        checked: dialog.phpChoice === ""
                        title: qsTr("No PHP")
                        note: qsTr("Keep the PHP this computer has")
                        onToggled: (on) => { if (on) dialog.phpChoice = "" }
                    }

                    SectionLabel { ui: qsTr("Also download"); Layout.topMargin: 4 }
                    OptionRow {
                        visible: !!dialog.composer
                        Layout.fillWidth: true
                        checked: dialog.wantComposer && enabled
                        enabled: !!dialog.composer && !dialog.composer.installed
                        title: "Composer"
                        note: qsTr("Needed to create projects · from %1").arg(dialog.composer ? dialog.composer.source : "")
                              + (dialog.composer && dialog.composer.installed ? " · " + qsTr("already downloaded")
                                 : (dialog.found("composer") ? " · " + qsTr("this computer has %1").arg(dialog.found("composer").version || "Composer") : ""))
                        size: dialog.composer ? dialog.composer.size : 0
                        onToggled: (on) => dialog.wantComposer = on
                    }
                    OptionRow {
                        visible: !!dialog.mariadb
                        Layout.fillWidth: true
                        checked: dialog.wantMariadb && enabled
                        enabled: !!dialog.mariadb && !dialog.mariadb.installed
                        title: dialog.mariadb ? "MariaDB " + dialog.mariadb.version : ""
                        note: qsTr("Only for projects that use MySQL; SQLite needs nothing · from %1").arg(dialog.mariadb ? dialog.mariadb.source : "")
                              + (dialog.mariadb && dialog.mariadb.installed ? " · " + qsTr("already downloaded")
                                 : (dialog.found("mariadb") ? " · " + qsTr("this computer has %1").arg(dialog.found("mariadb").version || "MariaDB") : ""))
                        size: dialog.mariadb ? dialog.mariadb.size : 0
                        onToggled: (on) => dialog.wantMariadb = on
                    }
                    RichText {
                        visible: !dialog.mariadb && Studio.mariadbFromHomebrew
                        raw: qsTr("MariaDB (only for projects that use MySQL): MariaDB makes no builds for macOS. Install it with Homebrew: `brew install mariadb`.")
                        color: Theme.muted
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        spacing: 8
                        Layout.fillWidth: true
                        Icon { path: Icons.shield; color: Theme.ok; size: 16 }
                        RichText {
                            raw: qsTr("Every file is checked against its `SHA-256` before Studio uses it.")
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                    }
                    RowLayout {
                        spacing: 6
                        Layout.fillWidth: true
                        AppText { ui: qsTr("Into:"); color: Theme.muted }
                        Mono { text: Studio.toolsFolder; color: Theme.muted; elide: Text.ElideMiddle; Layout.fillWidth: true }
                    }
                }

                // --- Downloading ---
                Repeater {
                    model: dialog.choosing ? [] : Studio.downloads
                    delegate: DownloadRow {
                        required property var modelData
                        entry: modelData
                        Layout.fillWidth: true
                    }
                }
                RowLayout {
                    visible: !dialog.choosing
                    spacing: 8
                    Layout.fillWidth: true
                    Icon { path: Icons.shield; color: Theme.ok; size: 16 }
                    RichText {
                        raw: qsTr("Each file is used only after its `SHA-256` check.")
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // --- Buttons ---
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 8
            AppButton {
                visible: dialog.choosing
                text: qsTr("Choose my own PHP…")
                onClicked: { dialog.close(); Studio.showSettings("tools") }
            }
            Item { Layout.fillWidth: true }
            AppButton {
                visible: dialog.choosing || Studio.downloading
                text: qsTr("Cancel")
                onClicked: Studio.downloading ? Studio.cancelDownloads() : dialog.close()
            }
            AppButton {
                visible: dialog.choosing
                primary: true
                text: qsTr("Download")
                iconPath: Icons.download
                enabled: dialog.phpChoice !== "" || (dialog.wantComposer && !!dialog.composer && !dialog.composer.installed)
                         || (dialog.wantMariadb && !!dialog.mariadb && !dialog.mariadb.installed)
                onClicked: {
                    const ids = []
                    if (dialog.phpChoice !== "") ids.push(dialog.phpChoice)
                    if (dialog.wantComposer && dialog.composer && !dialog.composer.installed) ids.push(dialog.composer.id)
                    if (dialog.wantMariadb && dialog.mariadb && !dialog.mariadb.installed) ids.push(dialog.mariadb.id)
                    Studio.startDownloads(ids)
                }
            }
            AppButton {
                visible: dialog.finished
                primary: true
                text: qsTr("Close")
                onClicked: dialog.close()
            }
        }
    }
}
