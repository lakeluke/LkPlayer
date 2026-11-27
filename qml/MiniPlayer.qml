import QtQuick
import LkPlayer

// Picture-in-picture window shown over the library once the player page is
// left. It owns no video item of its own: the picture belongs to the window and
// is simply moved onto the rectangle reported here, which is what keeps playback
// running across the page switch.
Item {
    id: mini

    signal expandRequested()

    readonly property var player: App.player
    readonly property bool active: player ? player.active : false
    /** Height of the picture; the control strip sits below it. */
    readonly property int videoHeight: Math.round(width * 9 / 16)
    readonly property int stripHeight: 40

    width: 336
    height: videoHeight + stripHeight

    visible: active && player && player.filePath.length > 0

    function reportGeometry() {
        if (!visible || width <= 0 || videoHeight <= 0)
            return;
        const point = mini.mapToItem(null, 0, 0);
        App.setVideoRect(point.x, point.y, width, videoHeight);
    }

    function syncVideo() {
        App.setVideoVisible(visible);
    }

    // Keep the floating window inside the page while dragging or resizing.
    function clampPosition() {
        if (!parent)
            return;
        const m = Theme.spacingLarge;
        x = Math.max(m, Math.min(x, parent.width - width - m));
        y = Math.max(m, Math.min(y, parent.height - height - m));
    }

    // Follow page resizes so the mini player never ends up off-screen.
    Connections {
        target: mini.parent
        enabled: mini.parent !== null
        function onWidthChanged() {
            mini.clampPosition();
        }
        function onHeightChanged() {
            mini.clampPosition();
        }
    }

    onXChanged: reportGeometry()
    onYChanged: reportGeometry()
    onWidthChanged: reportGeometry()
    onHeightChanged: reportGeometry()
    onVisibleChanged: {
        syncVideo();
        if (visible)
            reportGeometry();
    }

    Component.onCompleted: {
        reportGeometry();
        syncVideo();
    }

    Component.onDestruction: App.setVideoVisible(false)

    // The picture is drawn by the window-level item underneath; this only draws
    // the frame around it and the control strip, which extends below the video
    // area so it is never covered.
    Rectangle {
        anchors.fill: parent
        anchors.topMargin: mini.videoHeight
        radius: Theme.radiusSmall
        color: Theme.dark ? Qt.rgba(0.08, 0.085, 0.1, 0.96) : Qt.rgba(1, 1, 1, 0.96)
        border.width: 1
        border.color: Theme.border
    }

    Row {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.spacingSmall
        anchors.rightMargin: Theme.spacingSmall
        anchors.bottomMargin: 6
        height: mini.stripHeight - 12
        spacing: Theme.spacingSmall

        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - controls.width - Theme.spacingSmall
            text: mini.player ? mini.player.title : ""
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideRight
        }

        Row {
            id: controls
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            Rectangle {
                width: 28
                height: 28
                radius: 14
                color: playArea.containsMouse ? Theme.surfaceHover : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: mini.player && mini.player.playing ? "⏸" : "▶"
                    color: Theme.textPrimary
                    font.pixelSize: 13
                }

                MouseArea {
                    id: playArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: App.togglePlayPause()
                }
            }

            Rectangle {
                width: 28
                height: 28
                radius: 14
                color: expandArea.containsMouse ? Theme.surfaceHover : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "⤢"
                    color: Theme.textPrimary
                    font.pixelSize: 14
                }

                MouseArea {
                    id: expandArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: mini.expandRequested()
                }
            }

            Rectangle {
                width: 28
                height: 28
                radius: 14
                color: closeArea.containsMouse ? Theme.surfaceHover : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    color: Theme.textSecondary
                    font.pixelSize: 12
                }

                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: App.stopPlayback()
                }
            }
        }
    }

    // Dragging the picture moves the mini player; a double-click returns to the
    // full player page.
    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: mini.videoHeight
        cursorShape: Qt.PointingHandCursor

        property real startX: 0
        property real startY: 0
        property real startGlobalX: 0
        property real startGlobalY: 0

        onPressed: mouse => {
            startX = mini.x;
            startY = mini.y;
            const p = mini.mapToItem(null, mouse.x, mouse.y);
            startGlobalX = p.x;
            startGlobalY = p.y;
        }
        onPositionChanged: mouse => {
            if (!pressed)
                return;
            // mouse.x/y are relative to this MouseArea, which moves with the
            // mini player; converting to scene coordinates avoids the feedback
            // that otherwise halves the drag distance.
            const p = mini.mapToItem(null, mouse.x, mouse.y);
            mini.x = startX + p.x - startGlobalX;
            mini.y = startY + p.y - startGlobalY;
            mini.clampPosition();
        }
        onDoubleClicked: mini.expandRequested()
    }
}
