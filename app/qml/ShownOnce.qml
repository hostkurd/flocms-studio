import QtQuick
import QtQuick.Layouts
import FloStudio.Backend

// One page of a StackLayout, made the first time the stack shows it (lazy
// loading) and kept while it is used. Hidden for a while (Studio's
// releaseIdle, every minute), it is freed again, least recently used
// first, and made anew the next time it is shown. What it shows lives in
// C++ (forms, results, logs), so it comes back as it was; only its scroll
// position starts over. A page with something only it holds says so with
// `keepLoaded: true` and is kept. `index` is this page's place in the
// stack, matching its StackLayout's order.
Loader {
    id: loader
    property int index: 0
    readonly property bool shown: parent && parent.currentIndex === index
    property bool wanted: shown
    // When it was hidden (Date.now())
    property real hiddenSince: 0
    readonly property bool keep: !!item && item.keepLoaded === true
    onShownChanged: {
        if (shown)
            wanted = true
        else
            hiddenSince = Date.now()
    }
    active: wanted
    Layout.fillWidth: true
    Layout.fillHeight: true

    Connections {
        target: Studio
        function onReleaseIdle(now, screenIdleMs) {
            if (!loader.shown && loader.wanted && !loader.keep && (now || Date.now() - loader.hiddenSince >= screenIdleMs)) {
                loader.wanted = false
                Studio.screenReleased()
            }
        }
    }
}
