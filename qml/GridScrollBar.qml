import QtQuick

// Minimal themed vertical scrollbar that follows a Flickable (GridView /
// ListView). It replaces the per-grid inline scrollbar so the folder grid and
// the media grid share one implementation, and is drawn with enough contrast
// to stay visible on both the dark and light palettes.
Item {
    id: root

    /** The view this bar drives (GridView, ListView, ...). */
    property Flickable flickable: null

    width: 8

    visible: flickable !== null
             && flickable.contentHeight > flickable.height
             && flickable.height > 0

    readonly property real ratio: flickable !== null && flickable.contentHeight > 0
                                  ? flickable.height / flickable.contentHeight : 1

    // ------------------------------------------------------------ track
    Rectangle {
        anchors.fill: parent
        radius: root.width / 2
        color: Theme.dark ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.07)
    }

    // ------------------------------------------------------------ handle
    Rectangle {
        id: handle
        width: parent.width
        radius: root.width / 2
        color: Theme.dark ? Qt.rgba(1, 1, 1, 0.34) : Qt.rgba(0, 0, 0, 0.30)
        height: Math.max(36, root.height * root.ratio)
        y: {
            if (!root.flickable || root.flickable.contentHeight <= root.flickable.height)
                return 0;
            const span = root.height - height;
            const maxContent = root.flickable.contentHeight - root.flickable.height;
            if (span <= 0 || maxContent <= 0)
                return 0;
            return Math.max(0, Math.min(span, span * (root.flickable.contentY / maxContent)));
        }

        Behavior on y {
            // While dragging the handle has to sit under the pointer instead of
            // easing behind it.
            enabled: !mouse.pressed
            NumberAnimation {
                duration: 120
            }
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            /** Where inside the handle the drag started. */
            property real grabOffset: 0

            onPressed: m => grabOffset = m.y
            onPositionChanged: m => {
                if (!pressed || !root.flickable)
                    return;

                const span = root.height - handle.height;
                const maxContent = root.flickable.contentHeight - root.flickable.height;
                if (span <= 0 || maxContent <= 0)
                    return;

                // The handle's y follows contentY, so the pointer position has
                // to be translated back into a scroll offset instead of moving
                // the handle directly.
                const wanted = handle.y + m.y - grabOffset;
                const top = Math.max(0, Math.min(span, wanted));
                root.flickable.contentY = maxContent * (top / span);
            }
        }
    }
}
