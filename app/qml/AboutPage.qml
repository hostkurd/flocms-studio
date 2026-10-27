import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// About FloCMS Studio: the version and build (copied for bug reports), the
// project's pages, and the licenses that ship with this build.
ScrollView {
    id: page
    contentWidth: availableWidth
    clip: true

    readonly property string repo: "https://github.com/hostkurd/flocms-studio"
    // The shipped file when this build has it, else the same text on GitHub
    function openShipped(file, online) {
        if (file !== "")
            AppInfo.openFile(file)
        else
            Qt.openUrlExternally(page.repo + online)
    }

    ColumnLayout {
        width: Math.min(page.availableWidth - 56, 680)
        x: (page.availableWidth - width) / 2
        spacing: 16

        Item { implicitHeight: 12 }

        // --- Name and version ---
        RowLayout {
            spacing: 16
            Layout.fillWidth: true
            Image {
                source: "qrc:/icons/logo-128.png"
                sourceSize: Qt.size(64, 64)
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
                Accessible.ignored: true
            }
            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                Heading { level: 1; text: "FloCMS Studio"; Layout.fillWidth: true }
                AppText { ui: qsTr("Version %1").arg(AppInfo.version); color: Theme.muted }
            }
        }
        RichText {
            raw: qsTr("Create, run and change FloCMS projects. Every action runs a real `php flo` or `composer` command in the project and shows it.")
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        // --- This build ---
        Card {
            Layout.fillWidth: true
            SectionLabel { ui: qsTr("This build") }
            // English lines, left to right in every language
            Mono {
                LayoutMirroring.enabled: false
                horizontalAlignment: Text.AlignLeft
                text: AppInfo.buildText
                color: Theme.navText
                font.pixelSize: 12
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
            }
            RowLayout {
                spacing: 10
                AppButton {
                    id: copy
                    text: qsTr("Copy version info")
                    iconPath: Icons.copy
                    onClicked: { AppInfo.copyBuildText(); copied.restart() }
                }
                AppText {
                    visible: copied.running
                    ui: qsTr("Copied. Paste it into a bug report.")
                    color: Theme.okText
                    font.pixelSize: 13
                }
                Timer { id: copied; interval: 3000 }
            }
        }

        // --- The project ---
        Card {
            Layout.fillWidth: true
            SectionLabel { ui: qsTr("Project") }
            Flow {
                spacing: 8
                Layout.fillWidth: true
                AppButton { text: qsTr("Project page"); iconPath: Icons.external; onClicked: Qt.openUrlExternally(page.repo) }
                AppButton { text: qsTr("Releases"); iconPath: Icons.download; onClicked: Qt.openUrlExternally(page.repo + "/releases") }
                AppButton { text: qsTr("Report a problem"); iconPath: Icons.warning; onClicked: Qt.openUrlExternally(page.repo + "/issues") }
                AppButton { text: qsTr("Code signing policy"); iconPath: Icons.shield; onClicked: Qt.openUrlExternally(page.repo + "/blob/main/CODE_SIGNING.md") }
            }
        }

        // --- Licenses ---
        Card {
            Layout.fillWidth: true
            SectionLabel { ui: qsTr("License") }
            AppText {
                ui: qsTr("FloCMS Studio is free software under the MIT License. It uses Qt under the GNU LGPL v3, QtKeychain, and fonts under the SIL Open Font License; the third-party notices list them all, with their source code.")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Flow {
                spacing: 8
                Layout.fillWidth: true
                AppButton { text: qsTr("License"); iconPath: Icons.file; onClicked: page.openShipped(AppInfo.licenseFile, "/blob/main/LICENSE") }
                AppButton { text: qsTr("Third-party notices"); iconPath: Icons.file; onClicked: page.openShipped(AppInfo.noticesFile, "/tree/main/licenses") }
            }
        }

        AppText {
            text: "© 2026 HostKurd"
            color: Theme.muted
            font.pixelSize: 13
            Layout.bottomMargin: 24
        }
    }
}
