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
    // MariaDB: the newest LTS first (the default), then another version (D2)
    readonly property var mariadbs: offer.filter(p => p.tool === "mariadb")
    readonly property bool choosing: Studio.downloads.length === 0
    readonly property bool finished: !choosing && !Studio.downloading
    property string phpChoice: ""
    property bool wantComposer: true
    property string mariadbChoice: ""
    property string tab: "php"
    // What can be chosen: never what Studio has already, nor a Composer
    // older than (or as new as) the computer's own
    readonly property bool composerAvailable: !!composer && !composer.installed
        && !(found("composer") && !newer(composer.version, found("composer").version || ""))
    readonly property bool composerChosen: wantComposer && composerAvailable
    readonly property bool mariadbChosen: mariadbChoice !== ""
    // A MariaDB this run downloaded: its series, for "Set up and start"
    readonly property string downloadedSeries: {
        const done = Studio.downloads.find(d => d.tool === "mariadb" && d.state === "done")
        return done ? String(done.version).split(".").slice(0, 2).join(".") : ""
    }

    // Whether version a is newer than b (2.10.3 > 2.9.1)
    function newer(a, b) {
        const x = String(a).split(/[^0-9]+/).filter(p => p !== "").map(Number)
        const y = String(b).split(/[^0-9]+/).filter(p => p !== "").map(Number)
        for (let i = 0; i < Math.max(x.length, y.length); ++i) {
            if ((x[i] || 0) !== (y[i] || 0)) return (x[i] || 0) > (y[i] || 0)
        }
        return false
    }
    function toolName(tool, row) {
        return tool === "php" ? "PHP" : tool === "composer" ? "Composer" : (row && row.product ? row.product : "MariaDB")
    }
    // Where a tool this computer has comes from
    function sourceOf(path) {
        const p = String(path).toLowerCase().replace(/\\/g, "/")
        const own = String(Studio.toolsFolder).toLowerCase().replace(/\\/g, "/")
        if (own !== "" && p.startsWith(own)) return qsTr("Downloaded by Studio")
        if (p.indexOf("xampp") >= 0 || p.indexOf("lampp") >= 0) return qsTr("From XAMPP")
        if (p.indexOf("laragon") >= 0) return qsTr("From Laragon")
        if (p.indexOf("herd") >= 0) return qsTr("From Herd")
        return qsTr("Installed on this computer")
    }

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
        mariadbChoice = ""
        tab = missing("php") ? "php" : (missing("composer") ? "composer" : "php")
        if (["php", "composer", "mariadb"].indexOf(Studio.demoTab) >= 0) tab = Studio.demoTab
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

                    // One tab per tool: what this computer has, then what
                    // Studio can download (nothing it has already)
                    Segmented {
                        label: qsTr("Tool")
                        value: dialog.tab
                        Layout.fillWidth: true
                        options: [
                            { value: "php", text: "PHP" + (dialog.phpChoice !== "" ? " •" : "") },
                            { value: "composer", text: "Composer" + (dialog.composerChosen ? " •" : "") },
                            { value: "mariadb", text: "MariaDB" + (dialog.mariadbChosen ? " •" : "") }
                        ]
                        onChosen: (value) => dialog.tab = value
                    }

                    SectionLabel { ui: qsTr("On this computer"); Layout.topMargin: 4 }
                    Rectangle {
                        id: haveBox
                        readonly property var row: dialog.found(dialog.tab)
                        Layout.fillWidth: true
                        implicitHeight: here.implicitHeight + 20
                        radius: 10
                        color: Theme.rule
                        ColumnLayout {
                            id: here
                            anchors.fill: parent
                            anchors.margins: 10
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 2
                            RowLayout {
                                spacing: 8
                                Layout.fillWidth: true
                                AppText {
                                    text: haveBox.row
                                          ? dialog.toolName(dialog.tab, haveBox.row) + " " + (haveBox.row.version || "")
                                          : ""
                                    visible: !!haveBox.row
                                    font.weight: Font.DemiBold
                                }
                                AppText {
                                    visible: !haveBox.row
                                    ui: qsTr("Not found on this computer")
                                    color: Theme.muted
                                }
                                Item { Layout.fillWidth: true }
                                Pill {
                                    visible: dialog.tab === "mariadb" && !!haveBox.row
                                    text: haveBox.row && haveBox.row.state === "running" ? qsTr("Running") : qsTr("Not running")
                                    color: haveBox.row && haveBox.row.state === "running" ? Theme.okBg : Theme.border
                                    fg: haveBox.row && haveBox.row.state === "running" ? Theme.okText : Theme.muted
                                }
                            }
                            AppText {
                                visible: !!haveBox.row
                                ui: haveBox.row ? dialog.sourceOf(haveBox.row.path) : ""
                                color: Theme.muted
                                font.pixelSize: 13
                            }
                            Mono {
                                visible: !!haveBox.row
                                text: haveBox.row ? haveBox.row.path : ""
                                color: Theme.muted
                                font.pixelSize: 12
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                        }
                    }

                    SectionLabel { ui: qsTr("Download"); Layout.topMargin: 4 }

                    // PHP: a version, or none
                    ButtonGroup { id: phpGroup }
                    Repeater {
                        model: dialog.tab === "php" ? dialog.phps : []
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
                        visible: dialog.tab === "php"
                        Layout.fillWidth: true
                        radio: true
                        group: phpGroup
                        checked: dialog.phpChoice === ""
                        title: qsTr("No PHP")
                        note: qsTr("Keep the PHP this computer has")
                        onToggled: (on) => { if (on) dialog.phpChoice = "" }
                    }

                    // Composer: Studio's own copy; an upgrade when this
                    // computer's is older
                    OptionRow {
                        readonly property var have: dialog.found("composer")
                        readonly property bool older: !!have && dialog.newer(dialog.composer ? dialog.composer.version : "", have.version || "")
                        visible: dialog.tab === "composer" && !!dialog.composer
                        Layout.fillWidth: true
                        checked: dialog.composerChosen
                        enabled: dialog.composerAvailable
                        title: dialog.composer ? (older ? qsTr("Upgrade to Composer %1").arg(dialog.composer.version) : "Composer " + dialog.composer.version) : ""
                        note: !dialog.composer ? ""
                              : dialog.composer.installed ? qsTr("Already downloaded")
                              : (!!have && !older) ? qsTr("This computer has the same or a newer version")
                              : older ? qsTr("Studio's own copy, used instead of %1; that one stays as it is · from %2").arg(have.version).arg(dialog.composer.source)
                              : qsTr("Needed to create projects · from %1").arg(dialog.composer.source)
                        size: dialog.composer ? dialog.composer.size : 0
                        onToggled: (on) => dialog.wantComposer = on
                    }

                    // MariaDB: Studio's own server, the newest LTS or another
                    // version; the one this computer has stays as it is
                    ButtonGroup { id: mariadbGroup }
                    Repeater {
                        model: dialog.tab === "mariadb" ? dialog.mariadbs : []
                        delegate: OptionRow {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            radio: true
                            group: mariadbGroup
                            checked: dialog.mariadbChoice === modelData.id
                            enabled: !modelData.installed
                            title: index === 0 ? qsTr("MariaDB %1 LTS").arg(modelData.version.split(".").slice(0, 2).join("."))
                                               : qsTr("Another version: MariaDB %1 LTS").arg(modelData.version.split(".").slice(0, 2).join("."))
                            note: (index === 0 ? qsTr("Newest · Studio starts and stops it · from %1").arg(modelData.source)
                                               : qsTr("from %1").arg(modelData.source))
                            badge: modelData.installed ? qsTr("Downloaded") : ""
                            badgeColor: Theme.okBg
                            badgeText: Theme.okText
                            size: modelData.installed ? 0 : modelData.size
                            onToggled: (on) => { if (on) dialog.mariadbChoice = modelData.id }
                        }
                    }
                    OptionRow {
                        visible: dialog.tab === "mariadb" && dialog.mariadbs.length > 0
                        Layout.fillWidth: true
                        radio: true
                        group: mariadbGroup
                        checked: dialog.mariadbChoice === ""
                        title: qsTr("No MariaDB")
                        note: qsTr("Only projects that use MySQL need one; SQLite needs nothing")
                        onToggled: (on) => { if (on) dialog.mariadbChoice = "" }
                    }
                    MariadbSetup {
                        visible: dialog.tab === "mariadb" && Studio.mariadb.state !== "none"
                        Layout.fillWidth: true
                    }
                    RichText {
                        visible: dialog.tab === "mariadb" && dialog.mariadbs.length === 0
                        raw: Studio.mariadbFromHomebrew
                             ? qsTr("MariaDB makes no builds for macOS. Install it with Homebrew: `brew install mariadb`.")
                             : qsTr("Studio can't download MariaDB for this system yet. Install it with your package manager, for example `sudo apt install mariadb-server`.")
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
                // A MariaDB arrived: set it up and start it from here
                MariadbSetup {
                    visible: dialog.finished && dialog.downloadedSeries !== "" && Studio.mariadb.state !== "none"
                    series: dialog.downloadedSeries
                    Layout.fillWidth: true
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
                enabled: dialog.phpChoice !== "" || dialog.composerChosen || dialog.mariadbChosen
                onClicked: {
                    const ids = []
                    if (dialog.phpChoice !== "") ids.push(dialog.phpChoice)
                    if (dialog.composerChosen) ids.push(dialog.composer.id)
                    if (dialog.mariadbChosen) ids.push(dialog.mariadbChoice)
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
