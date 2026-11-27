import QtQuick
import QtQuick.Dialogs
import LkPlayer

// Preferences: library root, external tool directory, playback mode, theme and cache.
Item {
    id: page

    signal backRequested()

    property string pendingToolError: ""

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

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "设置"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontTitle
                font.weight: Font.Bold
            }
        }
    }

    Flickable {
        anchors.top: header.bottom
        anchors.topMargin: Theme.spacing
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.spacingLarge
        anchors.rightMargin: Theme.spacingLarge
        anchors.bottomMargin: Theme.spacingLarge
        contentWidth: width
        contentHeight: column.implicitHeight + 20
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column
            width: Math.min(parent.width - 12, 900)
            spacing: Theme.spacingLarge

            // --------------------------------------------------- media library
            Column {
                width: parent.width
                spacing: Theme.spacingSmall

                Text {
                    text: "媒体库"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                    font.letterSpacing: 1.2
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    width: parent.width
                    implicitHeight: libraryColumn.implicitHeight + 32
                    radius: Theme.radiusLarge
                    color: Theme.dark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(1, 1, 1, 0.85)
                    border.width: 1
                    border.color: Theme.border

                    Column {
                        id: libraryColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: 16
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        spacing: Theme.spacingSmall

                        Text {
                            width: parent.width
                            text: "根目录"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                            font.weight: Font.DemiBold
                        }

                        Text {
                            width: parent.width
                            text: App.libraryRoot.length > 0 ? App.libraryRoot : "尚未设置 — 选择一个文件夹后会自动开始扫描"
                            color: App.libraryRoot.length > 0 ? Theme.textSecondary : Theme.textMuted
                            font.pixelSize: Theme.fontCaption
                            wrapMode: Text.WrapAnywhere
                        }

                        Row {
                            spacing: Theme.spacingSmall
                            topPadding: 4

                            Rectangle {
                                width: 116
                                height: 36
                                radius: 18
                                gradient: Theme.accentGradient

                                Text {
                                    anchors.centerIn: parent
                                    text: "选择目录"
                                    color: "#FFFFFF"
                                    font.pixelSize: Theme.fontCaption
                                    font.weight: Font.DemiBold
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: libraryDialog.open()
                                }
                            }

                            Rectangle {
                                width: 116
                                height: 36
                                radius: 18
                                color: rescanArea.containsMouse ? Theme.surfaceHover : Theme.glass
                                border.width: 1
                                border.color: Theme.border

                                Text {
                                    anchors.centerIn: parent
                                    text: App.scanning ? "扫描中…" : "重新扫描"
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontCaption
                                }

                                MouseArea {
                                    id: rescanArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    enabled: !App.scanning && App.libraryRoot.length > 0
                                    onClicked: App.rescan()
                                }
                            }

                            Rectangle {
                                width: 116
                                height: 36
                                radius: 18
                                color: fullRescanArea.containsMouse ? Theme.surfaceHover : Theme.glass
                                border.width: 1
                                border.color: Theme.border

                                Text {
                                    anchors.centerIn: parent
                                    text: "忽略缓存重扫"
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontCaption
                                }

                                MouseArea {
                                    id: fullRescanArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    enabled: !App.scanning && App.libraryRoot.length > 0
                                    onClicked: App.rescanFull()
                                }
                            }

                            Text {
                                width: parent.width
                                topPadding: 4
                                text: App.lastScanTime.length > 0
                                      ? "上次扫描：" + App.lastScanTime
                                      : "尚未扫描过媒体库"
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontCaption
                            }
                        }
                    }
                }
            }

            // -------------------------------------------------------- tools
            Column {
                width: parent.width
                spacing: Theme.spacingSmall

                Text {
                    text: "程序目录（ffmpeg / ffplay）"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                    font.letterSpacing: 1.2
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    width: parent.width
                    implicitHeight: toolColumn.implicitHeight + 32
                    radius: Theme.radiusLarge
                    color: Theme.dark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(1, 1, 1, 0.85)
                    border.width: 1
                    border.color: Theme.border

                    Column {
                        id: toolColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        anchors.margins: 16
                        spacing: Theme.spacingSmall

                        Text {
                            width: parent.width
                            text: "封面由外部 ffmpeg 抽取；内嵌播放由内置的 libmpv 完成，不需要 ffplay。指向包含 ffmpeg / ffplay 的目录即可（仅用内嵌模式时可以不配）。"
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                            wrapMode: Text.WordWrap
                            lineHeight: 1.4
                        }

                        Rectangle {
                            width: parent.width
                            height: 42
                            radius: Theme.radiusMedium
                            color: Theme.dark ? Qt.rgba(0, 0, 0, 0.25) : Qt.rgba(0, 0, 0, 0.04)
                            border.width: 1
                            border.color: App.settings.toolsValid
                                          ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.4)
                                          : Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.4)

                            Row {
                                anchors.left: parent.left
                                anchors.leftMargin: 14
                                anchors.right: statusBadge.left
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 8

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: App.settings.toolsValid ? "✓" : "!"
                                    color: App.settings.toolsValid ? Theme.success : Theme.danger
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 24
                                    text: App.settings.ffmpegDir.length > 0 ? App.settings.ffmpegDir : "尚未设置"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontCaption
                                    elide: Text.ElideMiddle
                                }
                            }

                            Rectangle {
                                id: statusBadge
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                height: 26
                                width: badgeText.implicitWidth + 20
                                radius: 13
                                color: App.settings.toolsValid
                                       ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.16)
                                       : Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.16)

                                Text {
                                    id: badgeText
                                    anchors.centerIn: parent
                                    text: App.settings.toolsValid ? "可用" : "不可用"
                                    color: App.settings.toolsValid ? Theme.success : Theme.danger
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }
                            }
                        }

                        Text {
                            width: parent.width
                            visible: !App.settings.toolsValid
                            text: page.pendingToolError.length > 0 ? page.pendingToolError : App.settings.toolsStatus
                            color: Theme.danger
                            font.pixelSize: Theme.fontCaption
                            wrapMode: Text.WordWrap
                        }

                        Row {
                            spacing: Theme.spacingSmall
                            topPadding: 4

                            Rectangle {
                                width: 148
                                height: 36
                                radius: 18
                                gradient: Theme.accentGradient

                                Text {
                                    anchors.centerIn: parent
                                    text: "选择程序目录"
                                    color: "#FFFFFF"
                                    font.pixelSize: Theme.fontCaption
                                    font.weight: Font.DemiBold
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: toolDialog.open()
                                }
                            }

                            Rectangle {
                                width: 148
                                height: 36
                                radius: 18
                                color: autoArea.containsMouse ? Theme.surfaceHover : Theme.glass
                                border.width: 1
                                border.color: Theme.border

                                Text {
                                    anchors.centerIn: parent
                                    text: "自动检测 PATH"
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontCaption
                                }

                                MouseArea {
                                    id: autoArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        const detected = App.detectToolDir();
                                        if (detected.length > 0)
                                            App.setToolDirectory(detected);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // -------------------------------------------------- playback mode
            Column {
                width: parent.width
                spacing: Theme.spacingSmall

                Text {
                    text: "播放模式"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                    font.letterSpacing: 1.2
                    font.weight: Font.DemiBold
                }

                Row {
                    width: parent.width
                    spacing: Theme.spacing

                    Repeater {
                        model: [
                            {
                                key: "standalone",
                                title: "独立窗口",
                                desc: "ffplay 打开自己的窗口，最稳定，推荐默认使用。"
                            },
                            {
                                key: "embedded",
                                title: "内嵌到界面",
                                desc: App.embeddedSupported
                                      ? "画面直接渲染在播放页里（libmpv + 硬件解码），与界面同步刷新，不依赖 ffplay。"
                                      : "当前构建未包含 libmpv，选择后仍会用 ffplay 的独立窗口播放。"
                            },
                            {
                                key: "system",
                                title: "系统播放器",
                                desc: "交给系统注册的播放器打开（Windows 关联程序 / Linux xdg-open），不需要 ffmpeg / ffplay。"
                            }
                        ]

                        delegate: Rectangle {
                            required property var modelData

                            readonly property bool active: App.settings.playMode === modelData.key
                            // Embedding needs a window system we can drive; the card
                            // stays visible but inert where that is unavailable.
                            readonly property bool available: modelData.key !== "embedded" || App.embeddedSupported

                            width: (column.width - Theme.spacing * 2) / 3
                            implicitHeight: modeColumn.implicitHeight + 32
                            radius: Theme.radiusLarge
                            enabled: available
                            opacity: available ? 1 : 0.45
                            color: active
                                   ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                                   : (Theme.dark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(1, 1, 1, 0.85))
                            border.width: 1
                            border.color: active ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.55) : Theme.border

                            Behavior on color {
                                ColorAnimation {
                                    duration: Theme.durationFast
                                }
                            }

                            Column {
                                id: modeColumn
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 18
                                anchors.rightMargin: 18
                                spacing: 6

                                Row {
                                    spacing: 8

                                    Rectangle {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 16
                                        height: 16
                                        radius: 8
                                        color: active ? Theme.accent : "transparent"
                                        border.width: 2
                                        border.color: active ? Theme.accent : Theme.borderStrong

                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 6
                                            height: 6
                                            radius: 3
                                            color: "#FFFFFF"
                                            visible: active
                                        }
                                    }

                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.title
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontBody
                                        font.weight: Font.DemiBold
                                    }
                                }

                                Text {
                                    width: parent.width
                                    text: modelData.desc
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontCaption
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.4
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: App.setPlayMode(modelData.key)
                            }
                        }
                    }
                }

                // Where the ini actually lives differs between the portable
                // archive and an installed package, so show it.
                Text {
                    width: parent.width
                    text: (App.portable ? "便携模式 · " : "配置 · ") + App.settingsPath
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                    elide: Text.ElideMiddle
                }
            }

            // ------------------------------------------------------- appearance
            Column {
                width: parent.width
                spacing: Theme.spacingSmall

                Text {
                    text: "外观与缓存"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                    font.letterSpacing: 1.2
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    width: parent.width
                    implicitHeight: appearanceColumn.implicitHeight + 32
                    radius: Theme.radiusLarge
                    color: Theme.dark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(1, 1, 1, 0.85)
                    border.width: 1
                    border.color: Theme.border

                    Column {
                        id: appearanceColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: 16
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        spacing: Theme.spacing

                        Row {
                            width: parent.width
                            spacing: Theme.spacing

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 120
                                text: "主题"
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontBody
                            }

                            Repeater {
                                model: [
                                    {
                                        key: "dark",
                                        label: "深色"
                                    },
                                    {
                                        key: "light",
                                        label: "浅色"
                                    }
                                ]

                                delegate: Rectangle {
                                    required property var modelData

                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 96
                                    height: 34
                                    radius: 17
                                    color: App.settings.themeMode === modelData.key
                                           ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.9)
                                           : Theme.glass
                                    border.width: 1
                                    border.color: App.settings.themeMode === modelData.key ? "transparent" : Theme.border

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        color: App.settings.themeMode === modelData.key ? "#FFFFFF" : Theme.textSecondary
                                        font.pixelSize: Theme.fontCaption
                                        font.weight: Font.DemiBold
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: App.setThemeMode(modelData.key)
                                    }
                                }
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: 1
                            color: Theme.border
                        }

                        Row {
                            width: parent.width
                            spacing: Theme.spacing

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 120
                                text: "封面缓存"
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontBody
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: App.thumbnails.cacheCount() + " 个文件 · " + (App.thumbnails.cacheBytes() / 1048576).toFixed(1) + " MB"
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                            }

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 96
                                height: 34
                                radius: 17
                                color: clearArea.containsMouse ? Theme.surfaceHover : Theme.glass
                                border.width: 1
                                border.color: Theme.border

                                Text {
                                    anchors.centerIn: parent
                                    text: "清理缓存"
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontCaption
                                }

                                MouseArea {
                                    id: clearArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: App.clearThumbnailCache()
                                }
                            }

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 110
                                height: 34
                                radius: 17
                                color: genCoversArea.containsMouse ? Theme.surfaceHover : Theme.glass
                                border.width: 1
                                border.color: Theme.border
                                opacity: App.settings.generateCovers ? 1 : 0.5

                                Text {
                                    anchors.centerIn: parent
                                    text: "生成缺失封面"
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontCaption
                                }

                                MouseArea {
                                    id: genCoversArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    enabled: App.settings.generateCovers
                                    onClicked: App.generateAllCovers()
                                }
                            }
                        }

                        Row {
                            width: parent.width
                            spacing: Theme.spacing

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 120
                                text: "自动生成封面"
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontBody
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: App.settings.generateCovers
                                      ? "扫描后按需抽取视频画面"
                                      : "已关闭 — 仅显示海报与已缓存封面"
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                            }

                            Rectangle {
                                id: coverToggle
                                anchors.verticalCenter: parent.verticalCenter
                                width: 44
                                height: 24
                                radius: 12
                                color: App.settings.generateCovers ? Theme.accent : Theme.border

                                Behavior on color {
                                    ColorAnimation {
                                        duration: Theme.durationFast
                                    }
                                }

                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 18
                                    height: 18
                                    radius: 9
                                    color: "#FFFFFF"
                                    x: App.settings.generateCovers ? parent.width - width - 3 : 3

                                    Behavior on x {
                                        NumberAnimation {
                                            duration: Theme.durationFast
                                            easing: Easing.OutCubic
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: App.settings.generateCovers = !App.settings.generateCovers
                                }
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: 1
                            color: Theme.border
                        }

                        Column {
                            width: parent.width
                            spacing: 4

                            Text {
                                text: "配置文件"
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontBody
                            }

                            Text {
                                width: parent.width
                                text: App.settings.configFilePath
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontCaption
                                wrapMode: Text.WrapAnywhere
                            }
                        }
                    }
                }
            }
        }
    }

    FolderDialog {
        id: libraryDialog
        title: "选择媒体库根目录"
        onAccepted: App.setLibraryRoot(App.toLocalPath(selectedFolder.toString()))
    }

    FolderDialog {
        id: toolDialog
        title: "选择包含 ffmpeg 与 ffplay 的目录"
        onAccepted: {
            const path = App.toLocalPath(selectedFolder.toString());
            page.pendingToolError = App.checkToolDirectory(path);
            App.setToolDirectory(path);
        }
    }
}
