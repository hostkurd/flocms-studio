import QtQuick
import FloStudio.Backend

AppText {
    property int level: 2
    font.family: Lang.displayFont
    font.weight: level === 1 ? Font.Bold : Font.DemiBold
    font.pixelSize: level === 1 ? 28 : (level === 2 ? 17 : 15)
}
