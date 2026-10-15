import QtQuick
import LkPlayer

// Rectangle that hosts the native ffplay window in embedded mode. It reports
// its geometry (in window coordinates) to C++ so the child window can follow.
Item {
    id: root

    readonly property var player: App.player
    readonly property bool embedded: player ? player.embedded : false
    readonly property bool active: player ? player.active : false
    /** False when the registered system player owns playback. */
    readonly property bool controllable: player ? player.controllable : true

    function reportGeometry() {
        if (width <= 0 || height <= 0)
            return;
        const point = root.mapToItem(null, 0, 0);
        App.setVideoRect(point.x, point.y, root.width, root.height);
    }

    onXChanged: reportGeometry()
    onYChanged: reportGeometry()
    onWidthChanged: reportGeometry()
    onHeightChanged: reportGeometry()
    onVisibleChanged: {
        App.setVideoVisible(visible && embedded && active);
        if (visible)
            reportGeometry();
    }

    Component.onCompleted: {
        reportGeometry();
        App.setVideoVisible(embedded && active);
    }

    Component.onDestruction: App.setVideoVisible(false)

    // A slow heartbeat keeps the native window aligned through layout passes.
    Timer {
        interval: 400
        repeat: true
        running: root.visible && root.embedded
        onTriggered: root.reportGeometry()
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusLarge
        color: "#05060A"
        clip: true

        // Backdrop when nothing is playing yet.
        PosterImage {
            anchors.fill: parent
            radius: Theme.radiusLarge
            visible: !root.active && App.currentItem && App.currentItem.filePath !== undefined
            source: App.currentItem && App.currentItem.fanartPath ? "file:///" + App.currentItem.fanartPath : ""
            showPlaceholder: false
            opacity: 0.5
        }

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusLarge
            color: Qt.rgba(0.02, 0.02, 0.04, 0.72)
            visible: !root.embedded || !root.active
        }

        Column {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.7, 520)
            spacing: Theme.spacing

            visible: !root.embedded || !root.active

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                text: {
                    if (!App.currentItem || App.currentItem.filePath === undefined)
                        return "从媒体库中选择一个视频开始播放";
                    if (!root.active)
                        return App.currentItem.title;
                    if (!root.controllable)
                        return "已在系统播放器中打开";
                    if (!root.embedded)
                        return "正在独立窗口中播放";
                    return "正在加载…";
                }
                color: Theme.textPrimary
                font.pixelSize: Theme.fontDisplay
                font.weight: Font.Bold
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                maximumLineCount: 2
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                text: {
                    if (!root.controllable && root.active)
                        return "已交给 Windows 注册的视频播放器：进度、音量与暂停都在它的窗口里操作，这里不重复控制。";
                    if (!root.embedded && root.active)
                        return "内嵌模式关闭时，ffplay 会打开自己的播放窗口。可在设置中切换为内嵌模式，让画面回到这里。";
                    if (App.currentItem && App.currentItem.filePath !== undefined)
                        return App.currentItem.relativeFolder || "";
                    return "支持 ffmpeg / ffplay 可播放的所有格式，字幕会按同名规则自动加载。";
                }
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                lineHeight: 1.4
            }

            Item {
                width: 1
                height: 6
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.spacing

                Rectangle {
                    width: 118
                    height: 40
                    radius: 20
                    gradient: Theme.accentGradient
                    visible: App.currentItem && App.currentItem.filePath !== undefined
                    // Once the system player owns the file there is nothing to toggle here.
                    enabled: root.controllable || !root.active
                    opacity: enabled ? 1 : 0.4

                    Text {
                        anchors.centerIn: parent
                        text: root.active ? (root.player.playing ? "暂停" : "继续播放") : "开始播放"
                        color: "#FFFFFF"
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: App.togglePlayPause()
                    }
                }

                Rectangle {
                    width: 148
                    height: 40
                    radius: 20
                    visible: App.currentItem && App.currentItem.filePath !== undefined
                    color: Theme.glassStrong
                    border.width: 1
                    border.color: Theme.border

                    Text {
                        anchors.centerIn: parent
                        text: "用系统播放器打开"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (App.currentItem && App.currentItem.filePath)
                                App.openWithSystemPlayer(App.currentItem.filePath);
                        }
                    }
                }
            }
        }
    }
}
