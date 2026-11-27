import QtQuick
import QtQuick.Controls
import LkPlayer

// Main browsing surface: whole library in one grid, or folder by folder.
Item {
    id: page

    signal settingsRequested()
    /** The picture-in-picture window asks for the full player page. */
    signal playRequested()

    property bool sidebarCollapsed: false
    property string contextFilePath: ""

    // Scroll position persistence: the grid reports its contentY as the user
    // scrolls, the controller stamps it onto the current scope right before the
    // scope changes, and we restore it here once the grid is rebuilt.
    readonly property string gridScrollKey: App.viewMode === "drill" && App.currentFolder.length > 0
                                            ? App.currentFolder : "flat"

    property bool scrollRestorePending: false
    property int scrollRestoreTarget: 0

    onGridScrollKeyChanged: requestScrollRestore()

    Component.onCompleted: requestScrollRestore()
    Component.onDestruction: App.commitGridScroll()

    function requestScrollRestore() {
        scrollRestoreTarget = App.gridScrollForCurrentFolder();
        scrollRestorePending = scrollRestoreTarget > 0;
        attemptScrollRestore();
    }

    function attemptScrollRestore() {
        if (!scrollRestorePending)
            return;
        const maxY = grid.contentHeight - grid.height;
        if (maxY <= 0)
            return;  // model still laying out; retried on contentHeight change
        grid.contentY = Math.min(scrollRestoreTarget, maxY);
        scrollRestorePending = false;
    }

    /**
     * The current folder has subfolders but not a single video of its own, and
     * no filter is narrowing the grid. The two zones share the page, so the one
     * that has nothing to show steps aside: the folder strip then takes the
     * height the video grid would have used instead of leaving a blank block.
     */
    readonly property bool foldersOnly: App.viewMode === "drill"
            && App.libraryRoot.length > 0
            && !App.scanning
            && App.visibleCount === 0
            && App.childFolders.count > 0
            && App.selectedTags.length === 0
            && App.searchText.length === 0

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
        //
        // Wrapping grid instead of a horizontal strip: a single row only holds
        // part of the folders and the rest sit behind a scrollbar that is easy
        // to miss. The strip hugs its content, and once it would take more than
        // visibleRows rows away from the video grid below it starts scrolling.
        Item {
            id: folderStrip
            anchors.top: header.bottom
            anchors.topMargin: Theme.spacingSmall
            anchors.left: parent.left
            anchors.right: parent.right
            readonly property int stripHeaderHeight: 24
            readonly property int visibleRows: 2
            readonly property real naturalHeight: stripHeaderHeight
                    + Math.min(folderGrid.contentHeight, visibleRows * folderGrid.cellHeight)
            // With videos underneath the strip keeps its two rows; without any,
            // it grows into the space the media grid would have occupied.
            height: !visible ? 0 : (page.foldersOnly ? Math.max(0, content.height - y) : naturalHeight)
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

            GridView {
                id: folderGrid
                anchors.top: parent.top
                anchors.topMargin: 24
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.bottom: parent.bottom
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: App.childFolders
                cacheBuffer: 600

                // The cell is the card plus the gap, mirroring the media grid
                // below so both keep the same rhythm.
                readonly property int cardWidth: 138
                cellWidth: cardWidth + Theme.cardGap
                cellHeight: Math.round(cardWidth * Theme.cardAspect) + 46 + Theme.cardGap

                delegate: FolderCard {
                    width: folderGrid.cardWidth
                    height: folderGrid.cellHeight - Theme.cardGap
                    folderName: model.name
                    folderPath: model.path
                    coverUrl: model.coverUrl
                    videoCount: model.videoCount
                    relativePath: model.relativePath
                    onActivated: App.openFolder(model.path)
                }
            }

            GridScrollBar {
                anchors.top: folderGrid.top
                anchors.bottom: folderGrid.bottom
                anchors.right: parent.right
                flickable: folderGrid
            }

            // Only drawn when there really are two zones: with no videos below,
            // the strip owns the whole height and there is nothing to divide.
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.border
                visible: App.visibleCount > 0
            }
        }

        // ------------------------------------------------------- media grid
        Item {
            id: gridArea
            anchors.top: folderStrip.bottom
            anchors.topMargin: folderStrip.visible && App.visibleCount > 0 ? Theme.spacing : 0
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

                onContentYChanged: App.reportGridScroll(grid.contentY)
                onContentHeightChanged: page.attemptScrollRestore()

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
                    onContextMenuRequested: {
                        page.contextFilePath = model.filePath;
                        cardMenu.popup();
                    }
                }
            }

            GridScrollBar {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                flickable: grid
            }

            EmptyState {
                anchors.fill: parent
                // A folder that only holds subfolders has had them take the page,
                // so the notice would cover them for no reason. Filters still get
                // their own message: they narrow the videos, not the folders.
                visible: App.visibleCount === 0 && !page.foldersOnly
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

    // Picture-in-picture: the session keeps playing here after leaving the
    // player page. It draws only its frame and controls; the picture itself is
    // owned by the window, which is what lets it survive the page switch.
    MiniPlayer {
        id: miniPlayer
        onExpandRequested: page.playRequested()

        Component.onCompleted: {
            // Start in the bottom-right corner; the user can then drag it around.
            miniPlayer.x = parent.width - miniPlayer.width - Theme.spacingLarge;
            miniPlayer.y = parent.height - miniPlayer.height - Theme.spacingLarge;
            miniPlayer.clampPosition();
        }
    }

    // ------------------------------------------------------------ context menu
    Menu {
        id: cardMenu

        MenuItem {
            text: "在资源管理器中打开"
            onTriggered: App.revealInFileManager(page.contextFilePath)
        }

        MenuItem {
            text: "删除"
            onTriggered: deleteDialog.openFor(page.contextFilePath)
        }
    }

    // -------------------------------------------------- delete confirmation
    Popup {
        id: deleteDialog
        anchors.centerIn: parent
        width: 480
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: Theme.spacingLarge

        property bool deleteFolder: false
        property var targets: []
        property string path: ""

        function openFor(filePath) {
            const preview = App.deletePreview(filePath);
            deleteFolder = preview.deleteFolder;
            targets = preview.targets;
            path = filePath;
            open();
        }

        background: Rectangle {
            radius: Theme.radiusLarge
            color: Theme.surfaceElevated
            border.width: 1
            border.color: Theme.borderStrong
        }

        contentItem: Column {
            spacing: Theme.spacing

            Text {
                width: parent.width
                text: "确认删除"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSubtitle
                font.weight: Font.DemiBold
            }

            Text {
                width: parent.width
                text: deleteDialog.deleteFolder
                      ? "该文件夹只包含这个视频及其关联信息（海报、nfo、字幕、剧照），将删除整个文件夹："
                      : "将删除以下视频文件："
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                wrapMode: Text.WordWrap
                lineHeight: 1.4
            }

            Rectangle {
                width: parent.width
                height: Math.max(48, Math.min(150, deleteTargetsColumn.implicitHeight + 20))
                radius: Theme.radiusMedium
                color: Theme.dark ? Qt.rgba(0, 0, 0, 0.25) : Qt.rgba(0, 0, 0, 0.04)
                border.width: 1
                border.color: Theme.border
                clip: true

                Column {
                    id: deleteTargetsColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 10
                    spacing: 5

                    Repeater {
                        model: deleteDialog.targets

                        delegate: Text {
                            width: deleteTargetsColumn.width
                            text: "• " + modelData
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideMiddle
                        }
                    }
                }
            }

            Text {
                width: parent.width
                text: "此操作无法撤销。"
                color: Theme.danger
                font.pixelSize: Theme.fontCaption
            }

            Row {
                anchors.right: parent.right
                spacing: Theme.spacingSmall

                Rectangle {
                    width: 88
                    height: 36
                    radius: 18
                    color: Theme.glass
                    border.width: 1
                    border.color: Theme.border

                    Text {
                        anchors.centerIn: parent
                        text: "取消"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontBody
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: deleteDialog.close()
                    }
                }

                Rectangle {
                    width: 88
                    height: 36
                    radius: 18
                    color: Theme.danger

                    Text {
                        anchors.centerIn: parent
                        text: "删除"
                        color: "#FFFFFF"
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            deleteDialog.close();
                            App.deleteItem(deleteDialog.path);
                        }
                    }
                }
            }
        }
    }
}
