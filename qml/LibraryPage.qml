import QtQuick
import LkPlayer

// Main browsing surface: whole library in one grid, or folder by folder.
Item {
    id: page

    signal settingsRequested()

    property bool sidebarCollapsed: false

    TopBar {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.topBarHeight
        onSettingsRequested: page.settingsRequested()
    }

    // ------------------------------------------------------------- sidebar
    SideBar {
        id: sideBar
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: page.sidebarCollapsed ? 0 : Theme.sidebarWidth
        opacity: page.sidebarCollapsed ? 0.0 : 1.0
        clip: true

        Behavior on width {
            NumberAnimation {
                duration: Theme.durationNormal
                easing: Theme.easingStandard
            }
        }

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.durationFast
            }
        }

        onFolderSelected: path => App.openFolder(path)
    }

    // ------------------------------------------------------------- content
    Item {
        id: content
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: sideBar.right
        anchors.right: parent.right
        anchors.leftMargin: Theme.spacingLarge
        anchors.rightMargin: Theme.spacingLarge
        anchors.topMargin: Theme.spacing
        anchors.bottomMargin: Theme.spacing

        Item {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 44

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                visible: App.viewMode !== "drill"
                text: "全部视频"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontTitle
                font.weight: Font.Bold
            }

            Breadcrumb {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                visible: App.viewMode === "drill"
            }

            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.spacing

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: App.visibleCount + " 个视频"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                }

                Rectangle {
                    width: 32
                    height: 32
                    radius: 16
                    color: collapseArea.containsMouse ? Theme.surfaceHover : Theme.glass
                    border.width: 1
                    border.color: Theme.border

                    Text {
                        anchors.centerIn: parent
                        text: page.sidebarCollapsed ? "❯" : "❮"
                        color: Theme.textSecondary
                        font.pixelSize: 12
                    }

                    MouseArea {
                        id: collapseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: page.sidebarCollapsed = !page.sidebarCollapsed
                    }
                }
            }
        }

        // ----------------------------------------------------- folder strip
        Item {
            id: folderStrip
            anchors.top: header.bottom
            anchors.topMargin: Theme.spacingSmall
            anchors.left: parent.left
            anchors.right: parent.right
            height: visible ? 250 : 0
            visible: App.viewMode === "drill" && App.childFolders.count > 0

            Row {
                anchors.left: parent.left
                anchors.top: parent.top
                spacing: Theme.spacingSmall

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "子文件夹"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                    font.letterSpacing: 1.2
                    font.weight: Font.DemiBold
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: App.childFolders.count + " 个"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                }
            }

            ListView {
                id: folderRow
                anchors.top: parent.top
                anchors.topMargin: 24
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                orientation: ListView.Horizontal
                spacing: Theme.spacing
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: App.childFolders
                cacheBuffer: 600

                delegate: FolderCard {
                    width: 138
                    height: 138 * Theme.cardAspect + 46
                    folderName: model.name
                    folderPath: model.path
                    coverUrl: model.coverUrl
                    videoCount: model.videoCount
                    relativePath: model.relativePath
                    onActivated: App.openFolder(model.path)
                }
            }
        }

        // ------------------------------------------------------- media grid
        Item {
            id: gridArea
            anchors.top: folderStrip.bottom
            anchors.topMargin: folderStrip.visible ? Theme.spacing : 0
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            readonly property int columnCount: Math.max(2, Math.floor(width / 208))

            // The cell is the card plus the gap, so cards keep a margin on the
            // right and bottom instead of butting up against their neighbours.
            readonly property int cardWidth: Math.floor(width / columnCount) - Theme.cardGap

            GridView {
                id: grid
                anchors.fill: parent
                anchors.rightMargin: 12
                model: App.mediaModel
                clip: true
                cellWidth: gridArea.cardWidth + Theme.cardGap
                cellHeight: Math.round(gridArea.cardWidth * Theme.cardAspect) + Theme.cardFooter + Theme.cardGap
                cacheBuffer: 900
                boundsBehavior: Flickable.StopAtBounds
                maximumFlickVelocity: 3200

                delegate: MediaCard {
                    width: gridArea.cardWidth
                    height: grid.cellHeight - Theme.cardGap

                    filePath: model.filePath
                    title: model.title
                    tags: model.tags
                    coverUrl: model.coverUrl
                    durationText: model.durationText
                    hasPoster: model.hasPoster
                    metaText: {
                        const parts = [];
                        if (model.year && model.year.length > 0)
                            parts.push(model.year);
                        if (model.relativeFolder && model.relativeFolder.length > 0)
                            parts.push(model.relativeFolder);
                        else if (model.hasSubtitles)
                            parts.push("含字幕");
                        return parts.join(" · ");
                    }

                    onActivated: App.playItem(model.filePath)
                }
            }

            // Custom scrollbar
            Item {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: 8
                visible: grid.contentHeight > grid.height

                readonly property real ratio: grid.contentHeight > 0 ? grid.height / grid.contentHeight : 1

                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    color: Theme.dark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.05)
                }

                Rectangle {
                    width: parent.width
                    radius: 4
                    color: Theme.dark ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(0, 0, 0, 0.18)
                    height: Math.max(36, parent.height * parent.ratio)
                    y: {
                        if (grid.contentHeight <= grid.height)
                            return 0;
                        const span = parent.height - height;
                        const progress = grid.contentY / (grid.contentHeight - grid.height);
                        return Math.max(0, Math.min(span, span * progress));
                    }

                    Behavior on y {
                        enabled: !scrollHandle.pressed
                        NumberAnimation {
                            duration: 120
                        }
                    }

                    MouseArea {
                        id: scrollHandle
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }

            EmptyState {
                anchors.fill: parent
                visible: App.visibleCount === 0
                icon: App.scanning ? "" : "🎬"
                busy: App.scanning
                title: {
                    if (App.scanning)
                        return "正在扫描媒体库…";
                    if (App.libraryRoot.length === 0)
                        return "先选择一个媒体库目录";
                    if (App.selectedTags.length > 0 || App.searchText.length > 0)
                        return "没有符合筛选条件的视频";
                    if (App.viewMode === "drill")
                        return "这个目录下没有视频";
                    return "没有找到视频文件";
                }
                message: {
                    if (App.scanning)
                        return "已发现 " + App.scannedVideos + " 个视频，正在继续读取目录…";
                    if (App.libraryRoot.length === 0)
                        return "LkPlayer 会递归扫描目录下的所有子文件夹，并自动关联海报、nfo、字幕与剧照。";
                    if (App.selectedTags.length > 0 || App.searchText.length > 0)
                        return "试着清除标签筛选或搜索关键字。";
                    return "支持 mp4 / mkv / avi / mov / ts 等常见格式。";
                }
                actionText: App.libraryRoot.length === 0 ? "选择媒体库目录" : ""
                onActionTriggered: settingsRequested()
            }
        }
    }
}
