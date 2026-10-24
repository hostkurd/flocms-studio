import QtQuick
import FloStudio.Backend

// The small capitals above a group ("PROJECTS"). Arabic script has no
// capitals and no letter spacing: there it is just small and bold.
AppText {
    font.pixelSize: 12
    font.weight: Font.DemiBold
    font.capitalization: Lang.rtl ? Font.MixedCase : Font.AllUppercase
    font.letterSpacing: Lang.rtl ? 0 : 0.5
    color: Theme.muted
}
