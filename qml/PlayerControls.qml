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

    // Inverted text: pure black/white bars in windowed mode, bright text over
    // the dark glass in fullscreen. Deliberately not Theme.textSecondary (grey)
    // so the controls stay high-contrast.
    readonly property color textPrimaryColor: (root.fullscreen || Theme.dark) ? "#FFFFFF" : "#111111"
    readonly property color textSecondaryColor: (root.fullscreen || Theme.dark) ? "#D8DEE4" : "#3A3F47"
    readonly property color textMutedColor: (root.fullscreen || Theme.dark) ? "#8B949E" : "#6A737D"

    // Button fill. Fullscreen floats over arbitrary video frames, so the fill
    // goes dark to stay visible on bright scenes; windowed mode inverts against
    // the solid bar.
    readonly property color controlBg: root.fullscreen ? Qt.rgba(0, 0, 0, 0.40)
        : (Theme.dark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.08))
    readonly property color controlBgHover: root.fullscreen ? Qt.rgba(0, 0, 0, 0.56)
        : (Theme.dark ? Qt.rgba(1, 1, 1, 0.18) : Qt.rgba(0, 0, 0, 0.14))

    Rectangle {
        id: barBackground
        anchors.fill: parent
        radius: root.fullscreen ? Theme.radiusLarge : 0
        // Windowed: solid bar matching the theme (black/white). Fullscreen: a
        // thinner dark glass so the picture shows through around the buttons.
        color: root.fullscreen ? Qt.rgba(8, 10, 16, 0.38)
               : Theme.background
        border.width: 1
        border.color: root.fullscreen ? Qt.rgba(1, 1, 1, 0.12)
                       : (Theme.dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.08))

        // Top hairline highlight in windowed mode (black bar only; a white bar
        // already reads as a solid surface).
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: Qt.rgba(1, 1, 1, 0.18)
            visible: !root.fullscreen && Theme.dark
        }
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
            color: stopArea.containsMouse ? root.controlBgHover : root.controlBg
            border.width: 1
            border.color: Theme.border
            opacity: !root.controllable ? 0.3 : (root.active ? 1.0 : 0.5)
            enabled: root.controllable

            Rectangle {
                anchors.centerIn: parent
                width: 12
                height: 12
                radius: 2
                color: root.textPrimaryColor
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
                color: skipArea.containsMouse ? root.controlBgHover : "transparent"
                opacity: !root.controllable ? 0.3 : (root.active ? 1.0 : 0.4)
                enabled: root.controllable

                Text {
                    anchors.centerIn: parent
                    text: modelData < 0 ? "⏪" : "⏩"
                    color: root.textPrimaryColor
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

        // Playback speed: an editable spinbox with up/down step buttons.
        Item {
            id: speedControl
            anchors.verticalCenter: parent.verticalCenter
            width: 92
            height: 40
            opacity: !root.controllable ? 0.3 : (root.active ? 1.0 : 0.5)
            enabled: root.controllable

            readonly property real value: root.player ? root.player.speed : 1.0
            readonly property real minSpeed: 0.1
            readonly property real maxSpeed: 10.0

            // Snap to the 0.1 unit and clamp to the allowed range.
            function clampSpeed(v) {
                return Math.max(minSpeed, Math.min(maxSpeed, Math.round(v * 10) / 10));
            }

            // Display text: "0.1x", "0.5x", "1x", "1.5x", "10x".
            function formatSpeed(v) {
                const n = Math.round(v * 10) / 10;
                return (n % 1 === 0 ? String(Math.round(n)) : n.toFixed(1)) + "x";
            }

            function stepUp() {
                const step = value >= 1.0 ? 1.0 : 0.1;
                App.setSpeed(clampSpeed(value + step));
            }

            function stepDown() {
                const step = value <= 1.0 ? 0.1 : 1.0;
                App.setSpeed(clampSpeed(value - step));
            }

            function commit() {
                const raw = speedInput.text.trim().replace(/[x×X]$/, "").trim();
                const n = Number(raw);
                if (raw !== "" && isFinite(n)) {
                    const target = clampSpeed(n);
                    App.setSpeed(target);
                    speedInput.text = formatSpeed(target);
                } else {
                    speedInput.text = formatSpeed(value);
                }
            }

            Connections {
                target: speedControl
                function onValueChanged() {
                    if (!speedInput.activeFocus)
                        speedInput.text = speedControl.formatSpeed(speedControl.value);
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: parent.height / 2
                color: root.controlBg
                border.width: 1
                border.color: Theme.border

                TextInput {
                    id: speedInput
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.right: spinColumn.left
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    text: speedControl.formatSpeed(speedControl.value)
                    color: root.textPrimaryColor
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.DemiBold
                    clip: true
                    selectByMouse: true
                    maximumLength: 6

                    onActiveFocusChanged: {
                        if (activeFocus)
                            selectAll();
                    }

                    onEditingFinished: speedControl.commit()

                    Keys.onEscapePressed: {
                        text = speedControl.formatSpeed(speedControl.value);
                        focus = false;
                    }
                }

                Column {
                    id: spinColumn
                    anchors.right: parent.right
                    anchors.rightMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                    width: 22
                    height: parent.height - 6

                    Rectangle {
                        width: parent.width
                        height: parent.height / 2
                        radius: Theme.radiusSmall / 2
                        color: upArea.containsMouse ? root.controlBgHover : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "▴"
                            color: root.textSecondaryColor
                            font.pixelSize: 9
                        }

                        MouseArea {
                            id: upArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: speedControl.stepUp()
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: parent.height / 2
                        radius: Theme.radiusSmall / 2
                        color: downArea.containsMouse ? root.controlBgHover : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "▾"
                            color: root.textSecondaryColor
                            font.pixelSize: 9
                        }

                        MouseArea {
                            id: downArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: speedControl.stepDown()
                        }
                    }
                }
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
            color: root.fullscreen ? Qt.rgba(1, 1, 1, 0.24)
                   : (Theme.dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.10))

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
            color: root.textSecondaryColor
            font.pixelSize: Theme.fontCaption
        }
    }

    // ---------------------------------------------------------------- volume
    Row {
        id: volumeSection
        anchors.right: fullscreenChip.left
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

    // ---------------------------------------------------- fullscreen toggle
    Rectangle {
        id: fullscreenChip
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacing
        anchors.verticalCenter: parent.verticalCenter
        width: 32
        height: 32
        radius: 16
        color: fullscreenArea.containsMouse ? root.controlBgHover : root.controlBg
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
