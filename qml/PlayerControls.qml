import QtQuick
import LkPlayer

// Transport bar. Kept below the video area on purpose: a native child window
// would otherwise cover floating QML controls in embedded mode.
Item {
    id: root

    readonly property var player: App.player
    readonly property bool active: player ? player.active : false
    readonly property real duration: player ? player.duration : 0
    readonly property real position: player ? player.position : 0
    /** False when the system player owns playback: nothing here applies. */
    readonly property bool controllable: player ? player.controllable : true
    /** Fullscreen uses a dark glass surface over the picture in both themes. */
    readonly property bool fullscreen: App.fullscreen
    readonly property color textPrimaryColor: root.fullscreen ? "#E6EDF3" : Theme.textPrimary
    readonly property color textSecondaryColor: root.fullscreen ? "#8B949E" : Theme.textSecondary
    readonly property color textMutedColor: root.fullscreen ? "#6E7681" : Theme.textMuted

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusLarge
        color: root.fullscreen ? Qt.rgba(8, 10, 16, 0.58)
               : (Theme.dark ? Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.5)
                             : Qt.rgba(1, 1, 1, 0.8))
        border.width: 1
        border.color: root.fullscreen ? Qt.rgba(1, 1, 1, 0.12) : Theme.border
    }

    Row {
        id: transport
        anchors.left: parent.left
        anchors.leftMargin: Theme.spacing
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.spacingSmall

        // Play / pause
        Rectangle {
            width: 48
            height: 48
            radius: 24
            gradient: Theme.accentGradient
            opacity: !root.controllable ? 0.3 : (root.active ? 1.0 : 0.45)
            enabled: root.controllable

            scale: playArea.pressed ? 0.94 : (playArea.containsMouse ? 1.06 : 1.0)

            Behavior on scale {
                NumberAnimation {
                    duration: Theme.durationFast
                    easing: Theme.easingStandard
                }
            }

            Canvas {
                id: playGlyph
                anchors.centerIn: parent
                width: 20
                height: 20

                readonly property bool showPause: root.player ? root.player.playing : false

                onShowPauseChanged: requestPaint()

                onPaint: {
                    const ctx = getContext("2d");
                    ctx.reset();
                    ctx.fillStyle = "#FFFFFF";
                    if (showPause) {
                        ctx.fillRect(width * 0.20, height * 0.16, width * 0.22, height * 0.68);
                        ctx.fillRect(width * 0.58, height * 0.16, width * 0.22, height * 0.68);
                    } else {
                        ctx.beginPath();
                        ctx.moveTo(width * 0.26, height * 0.12);
                        ctx.lineTo(width * 0.86, height * 0.5);
                        ctx.lineTo(width * 0.26, height * 0.88);
                        ctx.closePath();
                        ctx.fill();
                    }
                }
            }

            MouseArea {
                id: playArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: App.togglePlayPause()
            }
        }

        // Stop
        Rectangle {
            width: 40
            height: 40
            radius: 20
            color: stopArea.containsMouse ? Theme.surfaceHover : Theme.glass
            border.width: 1
            border.color: Theme.border
            opacity: !root.controllable ? 0.3 : (root.active ? 1.0 : 0.5)
            enabled: root.controllable

            Rectangle {
                anchors.centerIn: parent
                width: 12
                height: 12
                radius: 2
                color: root.textSecondaryColor
            }

            MouseArea {
                id: stopArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                enabled: root.active
                onClicked: App.stopPlayback()
            }
        }

        // Skip back / forward
        Repeater {
            model: [-10, 10]

            delegate: Rectangle {
                required property var modelData

                width: 40
                height: 40
                radius: 20
                color: skipArea.containsMouse ? Theme.surfaceHover : "transparent"
                opacity: !root.controllable ? 0.3 : (root.active ? 1.0 : 0.4)
                enabled: root.controllable

                Text {
                    anchors.centerIn: parent
                    text: modelData < 0 ? "⏪" : "⏩"
                    color: root.textSecondaryColor
                    font.pixelSize: 15
                }

                MouseArea {
                    id: skipArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    enabled: root.active
                    onClicked: App.seekRelative(modelData)
                }
            }
        }

        // Playback speed: cycles through the presets on each click.
        Item {
            id: speedControl
            anchors.verticalCenter: parent.verticalCenter
            width: speedChip.width
            height: 40

            readonly property var steps: [0.5, 0.75, 1.0, 1.25, 1.5, 2.0]

            function nextSpeed() {
                const current = root.player ? root.player.speed : 1.0;
                let idx = steps.findIndex(s => Math.abs(s - current) < 0.001);
                if (idx < 0)
                    idx = steps.indexOf(1.0);
                return steps[(idx + 1) % steps.length];
            }

            Rectangle {
                id: speedChip
                anchors.verticalCenter: parent.verticalCenter
                width: 46
                height: 40
                radius: 20
                color: speedArea.containsMouse ? Theme.surfaceHover : Theme.glass
                border.width: 1
                border.color: Theme.border
                opacity: !root.controllable ? 0.3 : (root.active ? 1.0 : 0.5)
                enabled: root.controllable

                Text {
                    anchors.centerIn: parent
                    text: {
                        const s = root.player ? root.player.speed : 1.0;
                        return (Math.round(s * 100) / 100) + "×";
                    }
                    color: root.textSecondaryColor
                    font.pixelSize: Theme.fontCaption
                    font.weight: Font.DemiBold
                }
            }

            MouseArea {
                id: speedArea
                anchors.fill: speedChip
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: App.setSpeed(speedControl.nextSpeed())
            }
        }
    }

    // ------------------------------------------------------------- progress
    Item {
        id: seekBar
        anchors.left: transport.right
        anchors.leftMargin: Theme.spacingLarge
        anchors.right: timeLabels.left
        anchors.rightMargin: Theme.spacing
        anchors.verticalCenter: parent.verticalCenter
        height: 26

        property bool dragging: false
        property real dragRatio: 0

        readonly property real ratio: {
            if (dragging)
                return dragRatio;
            if (root.duration <= 0)
                return 0;
            return Math.max(0, Math.min(1, root.position / root.duration));
        }

        Rectangle {
            id: seekTrack
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: seekArea.containsMouse || seekBar.dragging ? 7 : 5
            radius: height / 2
            color: Theme.dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.1)

            Behavior on height {
                NumberAnimation {
                    duration: Theme.durationFast
                }
            }
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: seekTrack.width * seekBar.ratio
            height: seekTrack.height
            radius: height / 2
            gradient: Theme.accentGradient
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            x: seekTrack.width * seekBar.ratio - width / 2
            width: 14
            height: 14
            radius: 7
            color: "#FFFFFF"
            border.width: 2
            border.color: Theme.accent
            opacity: seekArea.containsMouse || seekBar.dragging ? 1 : 0

            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.durationFast
                }
            }
        }

        MouseArea {
            id: seekArea
            anchors.fill: parent
            anchors.topMargin: -8
            anchors.bottomMargin: -8
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            enabled: root.active && root.duration > 0

            function ratioAt(mouseX) {
                if (seekTrack.width <= 0)
                    return 0;
                return Math.max(0, Math.min(1, mouseX / seekTrack.width));
            }

            onPressed: mouse => {
                seekBar.dragging = true;
                seekBar.dragRatio = ratioAt(mouse.x);
            }
            onPositionChanged: mouse => {
                if (!seekBar.dragging)
                    return;
                seekBar.dragRatio = ratioAt(mouse.x);
            }
            onReleased: mouse => {
                const ratio = ratioAt(mouse.x);
                seekBar.dragging = false;
                App.seekTo(ratio * root.duration);
            }
        }
    }

    // ---------------------------------------------------------- time labels
    Row {
        id: timeLabels
        anchors.right: volumeSection.left
        anchors.rightMargin: Theme.spacingLarge
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: App.formatDuration(root.position)
            color: root.textPrimaryColor
            font.pixelSize: Theme.fontCaption
            font.weight: Font.DemiBold
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "/"
            color: root.textMutedColor
            font.pixelSize: Theme.fontCaption
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: App.formatDuration(root.duration)
            color: root.textMutedColor
            font.pixelSize: Theme.fontCaption
        }
    }

    // ---------------------------------------------------------------- volume
    Row {
        id: volumeSection
        anchors.right: modeChip.left
        anchors.rightMargin: Theme.spacingLarge
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.spacingSmall
        enabled: root.controllable
        opacity: root.controllable ? 1 : 0.35

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.durationFast
            }
        }

        Item {
            anchors.verticalCenter: parent.verticalCenter
            width: 26
            height: 26

            Text {
                anchors.centerIn: parent
                text: root.player ? (root.player.muted ? "🔇" : "🔊") : "🔊"
                font.pixelSize: 15
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: App.toggleMute()
            }
        }

        Item {
            id: volumeBar
            anchors.verticalCenter: parent.verticalCenter
            width: 90
            height: 22

            readonly property real ratio: root.player ? root.player.volume / 100 : 1

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 4
                radius: 2
                color: Theme.dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.1)
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width * volumeBar.ratio
                height: 4
                radius: 2
                color: Theme.accentSoft
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                x: parent.width * volumeBar.ratio - width / 2
                width: 11
                height: 11
                radius: 5.5
                color: "#FFFFFF"
                border.width: 2
                border.color: Theme.accentSoft
                opacity: volumeArea.containsMouse ? 1 : 0
            }

            MouseArea {
                id: volumeArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                function apply(mouseX) {
                    App.setVolume(Math.round(Math.max(0, Math.min(1, mouseX / width)) * 100));
                }

                onPressed: mouse => apply(mouse.x)
                onPositionChanged: mouse => {
                    if (pressed)
                        apply(mouse.x);
                }
            }
        }
    }

    // ------------------------------------------------------------ mode chip
    Rectangle {
        id: modeChip
        anchors.right: fullscreenChip.left
        anchors.rightMargin: Theme.spacingSmall
        anchors.verticalCenter: parent.verticalCenter
        height: 28
        width: modeLabel.implicitWidth + 22
        radius: 14
        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
        border.width: 1
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.32)

        Text {
            id: modeLabel
            anchors.centerIn: parent
            text: App.playModeLabel(App.settings.playMode)
            color: Theme.accentSoft
            font.pixelSize: 10
            font.weight: Font.DemiBold
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: App.setPlayMode(App.nextPlayMode())
        }
    }

    // ---------------------------------------------------- fullscreen toggle
    Rectangle {
        id: fullscreenChip
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacing
        anchors.verticalCenter: parent.verticalCenter
        width: 32
        height: 32
        radius: 16
        color: fullscreenArea.containsMouse ? Theme.surfaceHover : Theme.glass
        border.width: 1
        border.color: App.fullscreen ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35) : Theme.border

        Canvas {
            id: fullscreenGlyph
            anchors.centerIn: parent
            width: 16
            height: 16
            property color strokeColor: App.fullscreen ? Theme.accentSoft : root.textSecondaryColor
            onStrokeColorChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                ctx.strokeStyle = strokeColor;
                ctx.lineWidth = 1.6;
                ctx.lineCap = "round";
                ctx.lineJoin = "round";
                const c = 5;
                ctx.beginPath();
                ctx.moveTo(0, c);
                ctx.lineTo(0, 0);
                ctx.lineTo(c, 0);
                ctx.moveTo(width - c, 0);
                ctx.lineTo(width, 0);
                ctx.lineTo(width, c);
                ctx.moveTo(width, height - c);
                ctx.lineTo(width, height);
                ctx.lineTo(width - c, height);
                ctx.moveTo(c, height);
                ctx.lineTo(0, height);
                ctx.lineTo(0, height - c);
                ctx.stroke();
            }
        }

        MouseArea {
            id: fullscreenArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: App.toggleFullscreen()
        }
    }
}
