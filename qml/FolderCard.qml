import QtQuick
import LkPlayer

// Entry point of the drill-down mode: represents one sub folder.
Item {
    id: root

    property string folderName: ""
    property string folderPath: ""
    property url coverUrl
    property int videoCount: 0
    property string relativePath: ""

    signal activated()

    readonly property bool hovered: hoverArea.containsMouse
    readonly property int coverHeight: Math.round(width * Theme.cardAspect)

    Item {
        id: coverWrap
        width: parent.width
        height: root.coverHeight
        scale: root.hovered ? 1.035 : 1.0

        Behavior on scale {
            NumberAnimation {
                duration: Theme.durationNormal
                easing: Theme.easingStandard
            }
        }

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
            border.color: root.hovered ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.6) : Theme.border

            PosterImage {
                anchors.fill: parent
                radius: Theme.radiusCard
                source: root.coverUrl
                showPlaceholder: true
                requestedSize: Qt.size(Math.max(1, root.width), Math.max(1, root.coverHeight))
            }

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusCard
                color: Qt.rgba(0, 0, 0, root.hovered ? 0.3 : 0.42)

                Behavior on color {
                    ColorAnimation {
                        duration: Theme.durationNormal
                    }
                }
            }

            // Folder silhouette
            Canvas {
                anchors.centerIn: parent
                width: Math.min(cover.width * 0.42, 96)
                height: width * 0.78
                opacity: 0.95

                onPaint: {
                    const ctx = getContext("2d");
                    ctx.reset();
                    const w = width;
                    const h = height;
                    const r = Math.max(2, w * 0.06);

                    ctx.beginPath();
                    ctx.moveTo(w * 0.06, h * 0.26);
                    ctx.lineTo(w * 0.40, h * 0.26);
                    ctx.lineTo(w * 0.48, h * 0.40);
                    ctx.lineTo(w * 0.94, h * 0.40);
                    ctx.lineTo(w * 0.94, h * 0.90);
                    ctx.lineTo(w * 0.06, h * 0.90);
                    ctx.closePath();

                    const gradient = ctx.createLinearGradient(0, 0, w, h);
                    gradient.addColorStop(0, "#FFFFFF");
                    gradient.addColorStop(1, "#D9D3FF");
                    ctx.fillStyle = gradient;
                    ctx.globalAlpha = 0.92;
                    ctx.fill();
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.margins: 10
                height: 22
                width: countLabel.implicitWidth + 18
                radius: 11
                color: Qt.rgba(0, 0, 0, 0.6)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.16)

                Text {
                    id: countLabel
                    anchors.centerIn: parent
                    text: root.videoCount + " 个视频"
                    color: "#F1F3F8"
                    font.pixelSize: 10
                    font.weight: Font.Medium
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
            text: root.folderName
            color: Theme.textPrimary
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            text: root.relativePath
            color: Theme.textMuted
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideMiddle
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
