import QtQuick
import LkPlayer

// Immersive playback page: video surface, metadata side panel and transport.
Item {
    id: page

    signal backRequested()

    readonly property string playingPath: App.currentItem && App.currentItem.filePath ? App.currentItem.filePath : ""

    // Playlist sort order. "scan" is the default traversal order; the others
    // reorder App.allVideos (the flat playlist) in place.
    readonly property var sortFields: ["scan", "title", "name", "path", "duration", "modified", "release"]
    readonly property var sortLabels: ["扫描顺序", "标题", "文件名", "路径", "时长", "修改时间", "发行时间"]
    property int sortIndex: 0

    // Playback mode dropdown. The embedded backend is skipped where the build
    // or platform cannot host it.
    readonly property var modeKeys: App.embeddedSupported
        ? ["standalone", "embedded", "system"]
        : ["standalone", "system"]
    readonly property var modeLabels: App.embeddedSupported
        ? ["独立窗口", "内嵌模式", "系统播放器"]
        : ["独立窗口", "系统播放器"]

    function modeIndex() {
        const i = modeKeys.indexOf(App.settings.playMode);
        return i >= 0 ? i : 0;
    }

    function isTimeSort(field) {
        return field === "duration" || field === "modified" || field === "release";
    }

    function applySort(index) {
        sortIndex = index;
        const field = sortFields[sortIndex];
        // Time-like fields read better newest / longest first.
        const ascending = !page.isTimeSort(field);
        App.allVideos.sortBy(field, ascending);
        // Keep the playing item visible after the reorder.
        if (page.playingPath.length > 0) {
            const idx = App.allVideos.indexOfPath(page.playingPath);
            if (idx >= 0)
                Qt.callLater(() => playlistView.positionViewAtIndex(idx, ListView.Contain));
        }
    }

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
        visible: !App.fullscreen

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

            Dropdown {
                height: 32
                width: 120
                model: page.modeLabels
                currentIndex: page.modeIndex()
                onActivated: index => App.setPlayMode(page.modeKeys[index])
            }

        }
    }

    // ----------------------------------------------------------- stage row
    // The video surface is anchored to the panel slot instead of computing its
    // width from a magic number: it takes whatever space is left, so hiding the
    // metadata panel always lets it fill the stage.
    Item {
        id: stage
        anchors.top: App.fullscreen ? parent.top : header.bottom
        anchors.topMargin: App.fullscreen ? 0 : Theme.spacing
        anchors.left: parent.left
        anchors.right: parent.right
        // With both side panels folded there is nothing to breathe around, so
        // the stage hugs the edges and lets the video fill the full width.
        anchors.leftMargin: (App.fullscreen || (!App.playlistVisible && !App.detailsVisible)) ? 0 : Theme.spacingLarge
        anchors.rightMargin: (App.fullscreen || (!App.playlistVisible && !App.detailsVisible)) ? 0 : Theme.spacingLarge
        anchors.bottom: parent.bottom
        anchors.bottomMargin: App.fullscreen ? 0 : (58 + Theme.spacing)

        Behavior on anchors.leftMargin {
            NumberAnimation {
                duration: Theme.durationNormal
                easing: Theme.easingStandard
            }
        }
        Behavior on anchors.rightMargin {
            NumberAnimation {
                duration: Theme.durationNormal
                easing: Theme.easingStandard
            }
        }

        // The playlist panel slides in from the left edge. Its fold handle lives
        // in Main.qml, floating over the video, so this slot only holds the panel.
        Item {
            id: playlistHolder
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: App.fullscreen ? 0 : (App.playlistVisible ? 300 + Theme.spacing : 0)
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

                    Item {
                        width: parent.width
                        height: 24

                        Text {
                            anchors.left: parent.left
                            anchors.right: sortChip.left
                            anchors.rightMargin: Theme.spacingSmall
                            anchors.verticalCenter: parent.verticalCenter
                            text: "播放列表 · " + App.allVideos.count + " 个视频"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Dropdown {
                            id: sortChip
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            height: 24
                            width: 100
                            model: page.sortLabels
                            currentIndex: page.sortIndex
                            onActivated: index => page.applySort(index)
                        }
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

        // The details panel mirrors the playlist; its fold handle also floats
        // over the video in Main.qml.
        Item {
            id: drawerHolder
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: App.fullscreen ? 0 : (App.detailsVisible ? 344 + Theme.spacing : 0)
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
                visible: App.detailsVisible
                opacity: App.detailsVisible ? 1 : 0

                Behavior on opacity {
                    NumberAnimation {
                        duration: Theme.durationNormal
                    }
                }
            }
        }
    }

    // Keyboard shortcuts. Shortcut is window-scoped by default, so these keep
    // working no matter which child control currently has focus.
    Shortcut {
        sequence: "Space"
        onActivated: App.togglePlayPause()
    }
    Shortcut {
        sequence: "Esc"
        onActivated: {
            if (App.fullscreen)
                App.toggleFullscreen();
            else
                page.backRequested();
        }
    }
    Shortcut {
        sequence: "F11"
        onActivated: App.toggleFullscreen()
    }
    Shortcut {
        sequence: "Return"
        onActivated: App.toggleFullscreen()
    }
    Shortcut {
        sequence: "Enter"
        onActivated: App.toggleFullscreen()
    }
    Shortcut {
        sequence: "Left"
        onActivated: App.seekRelative(-10)
    }
    Shortcut {
        sequence: "Right"
        onActivated: App.seekRelative(10)
    }
    Shortcut {
        sequence: "Up"
        onActivated: App.setVolume(Math.min(100, (App.player ? App.player.volume : 100) + 5))
    }
    Shortcut {
        sequence: "Down"
        onActivated: App.setVolume(Math.max(0, (App.player ? App.player.volume : 100) - 5))
    }
    Shortcut {
        sequence: "M"
        onActivated: App.toggleMute()
    }

    // Keep the current video in view whenever the track changes while the
    // playlist is open.
    Connections {
        target: App
        function onCurrentItemChanged() {
            if (!App.playlistVisible || page.playingPath.length === 0)
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
