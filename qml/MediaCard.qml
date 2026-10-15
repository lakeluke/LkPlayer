import QtQuick
import QtQuick.Effects
import LkPlayer

// Poster card shown in the media grid. Sized by the GridView cell.
Item {
    id: root

    property string filePath: ""
    property string title: ""
    property var tags: []
    property url coverUrl
    property string metaText: ""
    property string durationText: ""
    property bool hasPoster: true

    property int reloadTick: 0

    signal activated()

    readonly property bool hovered: hoverArea.containsMouse
    readonly property string coverSource: coverUrl ? coverUrl.toString() : ""

    Connections {
        target: App
        function onThumbnailReady(path) {
            if (path === root.filePath)
                root.reloadTick += 1;
        }
    }

    readonly property int coverHeight: Math.round(width * Theme.cardAspect)

    Item {
        id: coverWrap
        width: parent.width
        height: root.coverHeight
        anchors.top: parent.top
        scale: root.hovered ? 1.035 : 1.0

        Behavior on scale {
            NumberAnimation {
                duration: Theme.durationNormal
                easing: Theme.easingStandard
            }
        }

        // Soft shadow made of two stacked rounded rectangles (cheap and crisp).
        Rectangle {
            anchors.fill: cover
            anchors.topMargin: root.hovered ? 12 : 8
            radius: cover.radius
            color: "#000000"
            opacity: root.hovered ? 0.45 : 0.3
        }

        Rectangle {
            id: cover
            anchors.fill: parent
            radius: Theme.radiusCard
            color: Theme.surfaceElevated
            border.width: 1
            border.color: root.hovered ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.55) : Theme.border

            PosterImage {
                id: coverImage
                anchors.fill: parent
                radius: Theme.radiusCard
                source: root.coverUrl
                reloadTick: root.reloadTick
                requestedSize: Qt.size(Math.max(1, root.width), Math.max(1, root.coverHeight))
            }

            // Bottom scrim so badges and titles stay legible on bright frames.
            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusCard
                visible: coverImage.imageReady
                gradient: Theme.coverScrim
            }

            // Tag badges
            Row {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 10
                spacing: 5

                Repeater {
                    model: Math.min(2, root.tags ? root.tags.length : 0)

                    delegate: Rectangle {
                        required property int index

                        height: 21
                        width: badgeLabel.implicitWidth + 16
                        radius: 10.5
                        color: Qt.rgba(0, 0, 0, 0.55)
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.16)

                        Text {
                            id: badgeLabel
                            anchors.centerIn: parent
                            text: root.tags ? root.tags[index] : ""
                            color: "#F1F3F8"
                            font.pixelSize: 10
                            font.weight: Font.Medium
                        }
                    }
                }
            }

            // Duration badge
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.margins: 10
                visible: root.durationText.length > 0
                height: 21
                width: durationLabel.implicitWidth + 16
                radius: 10.5
                color: Qt.rgba(0, 0, 0, 0.62)

                Text {
                    id: durationLabel
                    anchors.centerIn: parent
                    text: root.durationText
                    color: "#F1F3F8"
                    font.pixelSize: 10
                    font.weight: Font.Medium
                }
            }

            // Play affordance
            Rectangle {
                anchors.centerIn: parent
                width: 58
                height: 58
                radius: 29
                opacity: root.hovered ? 1.0 : 0.0
                scale: root.hovered ? 1.0 : 0.7

                gradient: Theme.accentGradient

                Canvas {
                    anchors.centerIn: parent
                    width: 20
                    height: 20

                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.reset();
                        ctx.fillStyle = "#FFFFFF";
                        ctx.beginPath();
                        ctx.moveTo(width * 0.22, height * 0.08);
                        ctx.lineTo(width * 0.90, height * 0.5);
                        ctx.lineTo(width * 0.22, height * 0.92);
                        ctx.closePath();
                        ctx.fill();
                    }
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: Theme.durationFast
                    }
                }

                Behavior on scale {
                    NumberAnimation {
                        duration: Theme.durationNormal
                        easing: Theme.easingEmphasis
                    }
                }
            }
        }
    }

    Column {
        anchors.top: coverWrap.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 3

        Text {
            width: parent.width
            text: root.title
            color: root.hovered ? Theme.textPrimary : Theme.textPrimary
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            maximumLineCount: 2
            wrapMode: Text.Wrap
            lineHeight: 1.15
            opacity: root.hovered ? 1.0 : 0.92
        }

        Text {
            width: parent.width
            text: root.metaText
            visible: text.length > 0
            color: Theme.textMuted
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: hoverArea
        anchors.fill: coverWrap
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.activated()
    }
}
