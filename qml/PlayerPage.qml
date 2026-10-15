import QtQuick
import LkPlayer

// Immersive playback page: video surface, metadata side panel and transport.
Item {
    id: page

    signal backRequested()

    property bool detailsVisible: true

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Theme.dark ? "#0B0C11" : "#F6F7FB"
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
        }
    }

    // ----------------------------------------------------------- stage row
    Row {
        id: stage
        anchors.top: header.bottom
        anchors.topMargin: Theme.spacing
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.spacingLarge
        anchors.rightMargin: Theme.spacingLarge
        anchors.bottom: controls.top
        anchors.bottomMargin: Theme.spacing
        spacing: Theme.spacing

        VideoSurface {
            id: surface
            width: page.detailsVisible ? parent.width - 360 : parent.width
            height: parent.height

            Behavior on width {
                NumberAnimation {
                    duration: Theme.durationNormal
                    easing: Theme.easingStandard
                }
            }
        }

        DetailDrawer {
            width: 344
            height: parent.height
            visible: page.detailsVisible
            opacity: page.detailsVisible ? 1 : 0

            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.durationNormal
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

    Component.onDestruction: App.setVideoVisible(false)
}
