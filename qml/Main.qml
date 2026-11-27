import QtQuick
import QtQuick.Controls
import LkPlayer

Window {
    id: window

    width: 1480
    height: 920
    minimumWidth: 1024
    minimumHeight: 660
    visible: true
    title: "LkPlayer · 本地媒体库播放器"
    color: Theme.background

    Component.onCompleted: {
        Theme.mode = App.settings.themeMode;
        Qt.callLater(function () {
            App.attachWindow(window);
        });
    }

    Connections {
        target: App.settings
        function onThemeModeChanged() {
            Theme.mode = App.settings.themeMode;
        }
    }

    Connections {
        target: App
        function onPlaybackRequested(filePath) {
            pageHost.navigateTo("player");
        }
    }

    Connections {
        target: App
        function onActorBrowseRequested() {
            pageHost.navigateTo("library");
        }
    }

    // ---------------------------------------------------------- background
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Theme.dark ? Theme.surface : "#FFFFFF"
            }
            GradientStop {
                position: 0.42
                color: Theme.background
            }
            GradientStop {
                position: 1.0
                color: Theme.backgroundDeep
            }
        }
    }

    // Ambient accent glow keeps the dark shell from feeling flat.
    Rectangle {
        width: Math.max(parent.width, parent.height) * 0.55
        height: width
        radius: width / 2
        x: -width * 0.22
        y: -height * 0.72
        color: Theme.accent
        opacity: Theme.dark ? 0.13 : 0.08
        antialiasing: true
    }

    Item {
        id: pageHost
        anchors.fill: parent

        property string currentPage: "library"

        function navigateTo(name) {
            if (name === currentPage) {
                loader.opacity = 1;
                return;
            }
            currentPage = name;
            transition.restart();
        }

        Loader {
            id: loader
            anchors.fill: parent
        }

        SequentialAnimation {
            id: transition

            NumberAnimation {
                target: loader
                property: "opacity"
                to: 0
                duration: 110
                easing: Easing.OutQuad
            }

            ScriptAction {
                script: loader.sourceComponent = pageHost.componentFor(pageHost.currentPage)
            }

            ParallelAnimation {
                NumberAnimation {
                    target: loader
                    property: "opacity"
                    to: 1
                    duration: 200
                    easing: Easing.OutQuad
                }
                NumberAnimation {
                    target: loader
                    property: "scale"
                    from: 0.985
                    to: 1
                    duration: 240
                    easing: Easing.OutCubic
                }
            }
        }

        function componentFor(name) {
            if (name === "player")
                return playerComponent;
            if (name === "settings")
                return settingsComponent;
            return libraryComponent;
        }

        Component.onCompleted: loader.sourceComponent = libraryComponent
    }

    // ------------------------------------------------------------ video layer
    // One picture item for the whole window, above the pages. A page-owned item
    // would be destroyed together with the page, which also tears down mpv's
    // render context and aborts playback; here the pages only publish the
    // rectangle the picture should fill, so the player page and the
    // picture-in-picture window hand it back and forth without a reload.
    MpvItem {
        id: videoLayer

        readonly property bool showing: App.videoVisible
                                        && App.videoRect.width > 1
                                        && App.videoRect.height > 1

        // Parked off-screen instead of hidden: an invisible item is dropped from
        // the scene graph, which would destroy the render context as well.
        x: showing ? App.videoRect.x : -20000
        y: showing ? App.videoRect.y : -20000
        width: Math.max(1, App.videoRect.width)
        height: Math.max(1, App.videoRect.height)
        z: 500
        player: App.player

        Behavior on x {
            enabled: videoLayer.showing
            NumberAnimation {
                duration: 180
                easing: Easing.OutCubic
            }
        }
        Behavior on y {
            enabled: videoLayer.showing
            NumberAnimation {
                duration: 180
                easing: Easing.OutCubic
            }
        }
        Behavior on width {
            enabled: videoLayer.showing
            NumberAnimation {
                duration: 180
                easing: Easing.OutCubic
            }
        }
        Behavior on height {
            enabled: videoLayer.showing
            NumberAnimation {
                duration: 180
                easing: Easing.OutCubic
            }
        }

        // Double-click toggles fullscreen — but only on the player page. On the
        // library page the picture lives inside the mini player, whose own mouse
        // area handles dragging and the double-click-to-expand instead.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            enabled: pageHost.currentPage === "player"
            onDoubleClicked: App.toggleFullscreen()
        }
    }

    // ------------------------------------------------ floating fold handles
    // The playlist / details fold handles float over the video's left and right
    // edges, above the video layer, so they never take up video space.
    Rectangle {
        id: playlistHandle
        x: App.videoRect.x + 4
        y: App.videoRect.y + App.videoRect.height / 2 - height / 2
        width: 18
        height: 46
        radius: 9
        z: 600
        visible: pageHost.currentPage === "player" && !App.fullscreen && App.videoRect.height > 160
        color: playlistHandleArea.containsMouse ? Theme.surfaceHover : Theme.glass
        border.width: 1
        border.color: App.playlistVisible ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.4) : Theme.border

        Text {
            anchors.centerIn: parent
            text: App.playlistVisible ? "‹" : "›"
            color: App.playlistVisible ? Theme.accentSoft : Theme.textSecondary
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: playlistHandleArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: App.setPlaylistVisible(!App.playlistVisible)
        }
    }

    Rectangle {
        id: detailsHandle
        x: App.videoRect.x + App.videoRect.width - width - 4
        y: App.videoRect.y + App.videoRect.height / 2 - height / 2
        width: 18
        height: 46
        radius: 9
        z: 600
        visible: pageHost.currentPage === "player" && !App.fullscreen && App.videoRect.height > 160
        color: detailsHandleArea.containsMouse ? Theme.surfaceHover : Theme.glass
        border.width: 1
        border.color: App.detailsVisible ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.4) : Theme.border

        Text {
            anchors.centerIn: parent
            text: App.detailsVisible ? "›" : "‹"
            color: App.detailsVisible ? Theme.accentSoft : Theme.textSecondary
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: detailsHandleArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: App.setDetailsVisible(!App.detailsVisible)
        }
    }

    // ------------------------------------------------------ window transport
    // The transport bar and its fullscreen auto-hide live at window level so
    // they can float above the video layer. The page only reserves space for
    // them with a bottom margin and no longer anchors to the controls.
    property bool controlsVisible: true

    // Fullscreen auto-hide: bring the transport bar back and restart the timer.
    function revealControls() {
        if (!App.fullscreen)
            return;
        window.controlsVisible = true;
        hideControlsTimer.restart();
    }

    // Fullscreen title. Pure text (no panel) with an outline + drop shadow so it
    // stays legible over any frame; it rides the same reveal/hide as the bar.
    Item {
        id: fullscreenTitle
        anchors.top: parent.top
        anchors.topMargin: Theme.spacingLarge
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 2 * Theme.spacingLarge
        height: 34
        z: 650
        visible: pageHost.currentPage === "player" && App.fullscreen
        opacity: window.controlsVisible ? 1 : 0

        Behavior on opacity {
            NumberAnimation {
                duration: 180
            }
        }

        readonly property string title: App.currentItem
                                            ? (App.currentItem.title || App.currentItem.fileName || "")
                                            : ""

        // Drop shadow: a dark copy nudged down-right, drawn under the main text.
        Text {
            anchors.fill: parent
            anchors.leftMargin: 2
            anchors.topMargin: 2
            text: fullscreenTitle.title
            color: Qt.rgba(0, 0, 0, 0.5)
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
            elide: Text.ElideMiddle
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Text {
            anchors.fill: parent
            text: fullscreenTitle.title
            color: "#FFFFFF"
            style: Text.Outline
            styleColor: Qt.rgba(0, 0, 0, 0.85)
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
            elide: Text.ElideMiddle
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    PlayerControls {
        id: controls
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        // Fullscreen keeps the floating rounded bar; windowed mode docks it to
        // the bottom edge so it spans the full width with no gap underneath.
        anchors.leftMargin: App.fullscreen ? Theme.spacingLarge : 0
        anchors.rightMargin: App.fullscreen ? Theme.spacingLarge : 0
        anchors.bottomMargin: App.fullscreen ? Theme.spacingLarge : 0
        anchors.topMargin: 0
        height: 58
        z: 600
        visible: pageHost.currentPage === "player"
        opacity: (App.fullscreen && !window.controlsVisible) ? 0 : 1

        Behavior on opacity {
            NumberAnimation {
                duration: 180
            }
        }
    }

    Timer {
        id: hideControlsTimer
        interval: 3000
        repeat: false
        onTriggered: {
            if (App.fullscreen)
                window.controlsVisible = false;
        }
    }

    Connections {
        target: App
        function onFullscreenChanged() {
            window.controlsVisible = true;
            if (App.fullscreen)
                hideControlsTimer.restart();
        }
    }

    // Detects mouse movement in fullscreen to bring the bar back. Hover events
    // pass through the window-level video item, so this still fires above it.
    MouseArea {
        id: fullscreenWakeArea
        anchors.fill: parent
        z: 700
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        enabled: App.fullscreen && pageHost.currentPage === "player"
        onPositionChanged: window.revealControls()
    }

    // Scan prompt: shown when the library has never been scanned.
    Connections {
        target: App
        function onNeedsScanChanged() {
            if (App.needsScan && App.libraryRoot.length > 0)
                scanPrompt.open();
        }
    }

    Popup {
        id: scanPrompt
        anchors.centerIn: parent
        width: 400
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        padding: 0

        background: Rectangle {
            radius: Theme.radiusLarge
            color: Theme.surface
            border.width: 1
            border.color: Theme.border
        }

        contentItem: Column {
            spacing: Theme.spacing
            leftPadding: Theme.spacingLarge
            rightPadding: Theme.spacingLarge
            topPadding: Theme.spacingLarge
            bottomPadding: Theme.spacingLarge

            Text {
                width: parent.width
                text: "尚未扫描媒体库"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
            }

            Text {
                width: parent.width
                text: "当前媒体库目录还没有扫描记录，浏览列表会是空的。是否现在扫描？"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                wrapMode: Text.WordWrap
            }

            Row {
                anchors.right: parent.right
                spacing: Theme.spacingSmall
                topPadding: Theme.spacing

                Rectangle {
                    width: 90
                    height: 34
                    radius: 17
                    color: cancelScanArea.containsMouse ? Theme.surfaceHover : Theme.glass
                    border.width: 1
                    border.color: Theme.border

                    Text {
                        anchors.centerIn: parent
                        text: "稍后"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }

                    MouseArea {
                        id: cancelScanArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: scanPrompt.close()
                    }
                }

                Rectangle {
                    width: 90
                    height: 34
                    radius: 17
                    gradient: Theme.accentGradient

                    Text {
                        anchors.centerIn: parent
                        text: "扫描"
                        color: "#FFFFFF"
                        font.pixelSize: Theme.fontCaption
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            scanPrompt.close();
                            App.rescan();
                        }
                    }
                }
            }
        }
    }

    Component {
        id: libraryComponent

        LibraryPage {
            onSettingsRequested: pageHost.navigateTo("settings")
            onPlayRequested: pageHost.navigateTo("player")
        }
    }

    Component {
        id: playerComponent

        PlayerPage {
            onBackRequested: pageHost.navigateTo("library")
        }
    }

    Component {
        id: settingsComponent

        SettingsPage {
            onBackRequested: pageHost.navigateTo("library")
        }
    }
}
