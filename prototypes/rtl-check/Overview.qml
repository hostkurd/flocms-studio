import QtQuick
import QtQuick.Layouts
import RtlCheck.Backend

// Board 4 of the approved mockups: the project overview.
ColumnLayout {
    spacing: 16

    GridLayout {
        columns: 3
        columnSpacing: 14
        rowSpacing: 14
        Layout.fillWidth: true

        Card {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            AppText { text: qsTr("Server"); color: Theme.muted; font.pixelSize: 13; font.bold: true }
            RowLayout {
                spacing: 8
                Rectangle { width: 10; height: 10; radius: 5; color: Theme.ok }
                AppText { text: qsTr("Running"); font.pixelSize: 18; font.bold: true }
            }
            AppText { text: "http://127.0.0.1:8000"; font.family: Theme.monoFont; font.pixelSize: 13; color: Theme.link }
            AppText { text: qsTr("Started %Ln minute(s) ago", "", 14); color: Theme.muted; font.pixelSize: 13 }
        }

        Card {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            AppText { text: qsTr("Database"); color: Theme.muted; font.pixelSize: 13; font.bold: true }
            AppText { text: "MariaDB · fectum"; font.pixelSize: 18; font.bold: true }
            RowLayout {
                spacing: 6
                AppText { text: qsTr("%Ln table(s)", "", 24) }
                AppText { text: "·"; color: Theme.muted }
                AppText { text: qsTr("%Ln migration(s) waiting", "", 2); color: Theme.warnText; font.bold: true }
            }
            AppButton { text: qsTr("Run migrations"); primary: true }
        }

        Card {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            AppText { text: qsTr("Packages"); color: Theme.muted; font.pixelSize: 13; font.bold: true }
            GridLayout {
                columns: 2
                rowSpacing: 8
                Layout.fillWidth: true
                AppText { text: "flocms-core"; Layout.fillWidth: true }
                AppText { text: "2.8.0"; font.family: Theme.monoFont; font.pixelSize: 13 }
                AppText { text: "flocms-admin"; Layout.fillWidth: true }
                RowLayout {
                    spacing: 6
                    Pill { text: qsTr("Update"); color: Theme.warnBg; fg: Theme.warnText; size: 12 }
                    AppText { text: "0.39.2"; font.family: Theme.monoFont; font.pixelSize: 13 }
                }
                AppText { text: "flocms-cli"; Layout.fillWidth: true }
                AppText { text: "2.0.1"; font.family: Theme.monoFont; font.pixelSize: 13 }
            }
        }
    }

    Card {
        Layout.fillWidth: true
        gap: 4
        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 6
            AppText { text: qsTr("Doctor"); font.pixelSize: 17; font.bold: true; Layout.fillWidth: true }
            AppButton { text: qsTr("Run again"); small: true }
        }
        CheckRow { label: qsTr("The `.env` file and the app key"); detail: qsTr("OK"); detailMono: false }
        CheckRow { label: qsTr("Storage folders are writable"); detail: qsTr("OK"); detailMono: false }
        CheckRow {
            status: "warn"
            label: qsTr("The slug `properties` clashes with `PropertiesController`.")
            detail: qsTr("How to fix"); detailMono: false; detailLink: true
        }
        CheckRow {
            status: "warn"
            label: qsTr("Mail settings are missing, so password reset won't work. Set `MAIL_HOST` in `.env`.")
            detail: qsTr("Open `.env`"); detailMono: false; detailLink: true
            last: true
        }
    }

    Rectangle {
        // Command output is always left to right, in every language.
        LayoutMirroring.enabled: false
        LayoutMirroring.childrenInherit: true
        Layout.fillWidth: true
        implicitHeight: terminal.implicitHeight + 24
        radius: 8
        color: Theme.terminalBg
        Column {
            id: terminal
            anchors.fill: parent
            anchors.margins: 12
            spacing: 4
            AppText { text: "› php flo optimize --no-interaction --no-ansi"; font.family: Theme.monoFont; font.pixelSize: 13; color: Theme.terminalPrompt }
            AppText { text: "Config cached. Routes cached. Views compiled."; font.family: Theme.monoFont; font.pixelSize: 13; color: Theme.terminalText }
            AppText { text: "Done in 0.8 s"; font.family: Theme.monoFont; font.pixelSize: 13; color: Theme.terminalOk }
        }
    }

    Item { Layout.fillHeight: true }
}
