import QtQuick
import LkPlayer

// Immersive playback page: video surface, metadata side panel and transport.
Item {
    id: page

    signal backRequested()

    property bool detailsVisible: true
    property bool playlistVisible: false
    readonly property string playingPath: App.currentItem && App.currentItem.filePath ? App.currentItem.filePath : ""

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Theme.dark ? Theme.backgroundDeep : "#F6F8FA"
            }
            GradientStop {
                position: 1.0
                color: Theme.background
            }
        }
    }

    // --------------------------------------------------------------- header
    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Theme.spacingLarge
        height: 46

        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.spacing

            Rectangle {
                width: 36
                height: 36
                radius: 18
                anchors.verticalCenter: parent.verticalCenter
                color: backArea.containsMouse ? Theme.surfaceHover : Theme.glass
                border.width: 1
                border.color: Theme.border

                Text {
                    anchors.centerIn: parent
                    text: "‹"
                    color: Theme.textPrimary
                    font.pixelSize: 22
                }

                MouseArea {
                    id: backArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: page.backRequested()
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(page.width * 0.55, 720)
                spacing: 2

                Text {
                    width: parent.width
                    text: App.currentItem && App.currentItem.title ? App.currentItem.title : "播放器"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSubtitle
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: App.currentItem && App.currentItem.filePath ? App.currentItem.filePath : ""
                    color: Theme.textMuted
                    font.pixelSize: 10
                    elide: Text.ElideMiddle
                }
            }
        }

        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.spacingSmall

            Rectangle {
                readonly property string label: App.playModeLabel(App.settings.playMode)

                height: 32
                width: modeText.implicitWidth + 34
                radius: 16
                color: modeArea.containsMouse ? Theme.surfaceHover : Theme.glass
                border.width: 1
                border.color: Theme.border

                Text {
                    id: modeText
                    anchors.centerIn: parent
                    text: parent.label
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                }

                MouseArea {
                    id: modeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: App.setPlayMode(App.nextPlayMode())
                }
            }

            Rectangle {
                height: 32
                width: playlistText.implicitWidth + 34
                radius: 16
                color: page.playlistVisible ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18) : Theme.glass
                border.width: 1
                border.color: page.playlistVisible ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35) : Theme.border

                Text {
                    id: playlistText
                    anchors.centerIn: parent
                    text: "播放列表"
                    color: page.playlistVisible ? Theme.accentSoft : Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: page.playlistVisible = !page.playlistVisible
                }
            }

            Rectangle {
                height: 32
                width: detailsText.implicitWidth + 34
                radius: 16
                color: page.detailsVisible ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18) : Theme.glass
                border.width: 1
                border.color: page.detailsVisible ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35) : Theme.border

                Text {
                    id: detailsText
                    anchors.centerIn: parent
                    text: "详情"
                    color: page.detailsVisible ? Theme.accentSoft : Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: page.detailsVisible = !page.detailsVisible
                }
            }

            Rectangle {
                height: 32
                width: fullscreenText.implicitWidth + 34
                radius: 16
                color: App.fullscreen ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18) : Theme.glass
                border.width: 1
                border.color: App.fullscreen ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35) : Theme.border

                Text {
                    id: fullscreenText
                    anchors.centerIn: parent
                    text: App.fullscreen ? "退出全屏" : "全屏"
                    color: App.fullscreen ? Theme.accentSoft : Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: App.toggleFullscreen()
                }
            }
        }
    }

    // ----------------------------------------------------------- stage row
    // The video surface is anchored to the panel slot instead of computing its
    // width from a magic number: it takes whatever space is left, so hiding the
    // metadata panel always lets it fill the stage.
    Item {
        id: stage
        anchors.top: header.bottom
        anchors.topMargin: Theme.spacing
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.spacingLarge
        anchors.rightMargin: Theme.spacingLarge
        anchors.bottom: controls.top
        anchors.bottomMargin: Theme.spacing

        // Collapses to zero width when hidden, exactly like the details panel:
        // the video surface then expands into the freed space.
        Item {
            id: playlistHolder
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: page.playlistVisible ? 300 + Theme.spacing : 0
            clip: true

            Behavior on width {
                NumberAnimation {
                    duration: Theme.durationNormal
                    easing: Theme.easingStandard
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 300
                radius: Theme.radiusLarge
                color: Theme.dark ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(1, 1, 1, 0.85)
                border.width: 1
                border.color: Theme.border
                clip: true

                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: Theme.spacingSmall

                    Text {
                        width: parent.width
                        text: "播放列表 · " + App.allVideos.count + " 个视频"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    ListView {
                        id: playlistView
                        anchors.top: parent.top
                        anchors.topMargin: 24
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        clip: true
                        spacing: 6
                        model: App.allVideos
                        boundsBehavior: Flickable.StopAtBounds

                        delegate: Rectangle {
                            width: playlistView.width
                            height: 46
                            radius: Theme.radiusMedium
                            color: model.filePath === page.playingPath
                                   ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                                   : (itemMouse.containsMouse ? Theme.surfaceHover : "transparent")

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.right: durationLabel.left
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: model.title
                                color: model.filePath === page.playingPath ? Theme.accentSoft : Theme.textPrimary
                                font.pixelSize: Theme.fontCaption
                                elide: Text.ElideRight
                            }

                            Text {
                                id: durationLabel
                                anchors.right: parent.right
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                text: model.durationText
                                color: Theme.textMuted
                                font.pixelSize: 10
                            }

                            MouseArea {
                                id: itemMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: App.playItem(model.filePath)
                            }
                        }
                    }
                }
            }
        }

        VideoSurface {
            id: surface
            anchors.left: playlistHolder.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: drawerHolder.left
        }

        // Collapses to zero width when the panel is hidden, which is what the
        // video surface expands into.
        Item {
            id: drawerHolder
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: page.detailsVisible ? 344 + Theme.spacing : 0
            clip: true

            Behavior on width {
                NumberAnimation {
                    duration: Theme.durationNormal
                    easing: Theme.easingStandard
                }
            }

            DetailDrawer {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 344
                visible: page.detailsVisible
                opacity: page.detailsVisible ? 1 : 0

                Behavior on opacity {
                    NumberAnimation {
                        duration: Theme.durationNormal
                    }
                }
            }
        }
    }

    // -------------------------------------------------------------- controls
    PlayerControls {
        id: controls
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingLarge
        anchors.topMargin: 0
        height: 78
    }

    // Keyboard shortcuts
    Item {
        anchors.fill: parent
        focus: true
        Keys.onPressed: event => {
            switch (event.key) {
            case Qt.Key_Space:
                App.togglePlayPause();
                event.accepted = true;
                break;
            case Qt.Key_Escape:
                if (App.fullscreen)
                    App.toggleFullscreen();
                else
                    page.backRequested();
                event.accepted = true;
                break;
            case Qt.Key_Left:
                App.seekRelative(-10);
                event.accepted = true;
                break;
            case Qt.Key_Right:
                App.seekRelative(10);
                event.accepted = true;
                break;
            case Qt.Key_Up:
                App.setVolume(Math.min(100, (App.player ? App.player.volume : 100) + 5));
                event.accepted = true;
                break;
            case Qt.Key_Down:
                App.setVolume(Math.max(0, (App.player ? App.player.volume : 100) - 5));
                event.accepted = true;
                break;
            case Qt.Key_M:
                App.toggleMute();
                event.accepted = true;
                break;
            default:
                break;
            }
        }
    }

    // Keep the current video in view whenever the track changes while the
    // playlist is open.
    Connections {
        target: App
        function onCurrentItemChanged() {
            if (!page.playlistVisible || page.playingPath.length === 0)
                return;
            const idx = App.allVideos.indexOfPath(page.playingPath);
            if (idx >= 0)
                playlistView.positionViewAtIndex(idx, ListView.Contain);
        }
    }

    // Playback is not tied to this page: the picture lives in Main.qml and keeps
    // running when the page goes away, continuing in the library's small window.
    Component.onDestruction: App.setVideoVisible(false)
}
