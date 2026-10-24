import QtQuick
import QtQuick.Layouts
import RtlCheck.Backend

// Board 7 of the approved mockups: the Doctor tab.
ColumnLayout {
    spacing: 16

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        Pill { text: qsTr("%Ln problem(s)", "", 1); color: Theme.badBg; fg: Theme.badText; size: 14 }
        Pill { text: qsTr("%Ln warning(s)", "", 3); color: Theme.warnBg; fg: Theme.warnText; size: 14 }
        Pill { text: qsTr("%Ln OK", "", 22); color: Theme.okBg; fg: Theme.okText; size: 14 }
        AppText { text: qsTr("Last run: %Ln minute(s) ago", "", 2); color: Theme.muted; font.pixelSize: 13 }
        Item { Layout.fillWidth: true }
        AppButton { text: qsTr("Run again") }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 14

        Card {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            gap: 2
            RowLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: 6
                AppText { text: qsTr("Framework checks"); font.pixelSize: 17; font.bold: true; Layout.fillWidth: true }
                AppText { text: "php flo doctor"; font.family: Theme.monoFont; font.pixelSize: 13; color: Theme.muted }
            }
            CheckRow { label: qsTr("`PHP` version"); detail: "8.4" }
            CheckRow { label: qsTr("The `pdo_mysql` extension"); detail: "loaded" }
            CheckRow { status: "warn"; label: qsTr("The `storage/cache/` folder"); detail: "missing" }
            CheckRow { label: qsTr("The `.env` file"); detail: "present" }
            CheckRow { label: "`APP_KEY`"; detail: "set" }
            CheckRow { label: qsTr("Database"); detail: "connected (mysql)" }
            CheckRow { status: "warn"; label: qsTr("Migrations"); detail: "2 pending: run php flo migrate" }
            CheckRow { label: qsTr("Debug mode"); detail: "off"; last: true }
            AppText { text: qsTr("%Ln more check(s), all OK", "", 15); color: Theme.muted; font.pixelSize: 13; Layout.topMargin: 6 }
        }

        Card {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            gap: 6
            RowLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: 4
                AppText { text: qsTr("Admin panel checks"); font.pixelSize: 17; font.bold: true; Layout.fillWidth: true }
                AppText { text: "php flo admin:doctor"; font.family: Theme.monoFont; font.pixelSize: 13; color: Theme.muted }
            }
            // Raw output of admin:doctor: English, always left to right.
            CheckRow { ltr: true; label: "Languages: en, ar, ku (default en)" }
            CheckRow { ltr: true; label: "Panel pages: /admin/dashboard, /admin/pages, /admin/media" }
            CheckRow { ltr: true; status: "warn"; label: "PropertyResource: table() has no defaultSort(): rows may change places between pages; sort by a unique column such as id" }
            CheckRow { ltr: true; status: "fail"; label: "Your site has a PropertiesController, which always wins: the panel's /admin/properties pages are not shown."; last: true }
            AppText {
                LayoutMirroring.enabled: false
                Layout.fillWidth: true
                text: "1 problem(s), 1 warning(s)."
                font.family: Theme.monoFont
                font.pixelSize: 12
                color: Theme.muted
            }
        }
    }

    Item { Layout.fillHeight: true }
}
