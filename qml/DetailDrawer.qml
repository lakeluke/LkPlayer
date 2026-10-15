import QtQuick
import LkPlayer

// Metadata column fed by the parsed nfo of the selected video.
Item {
    id: root

    readonly property var item: App.currentItem
    readonly property bool hasItem: item && item.filePath !== undefined && item.filePath.length > 0

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusLarge
        color: Theme.dark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(1, 1, 1, 0.85)
        border.width: 1
        border.color: Theme.border
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: Theme.spacing
        contentWidth: width
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column
            width: parent.width
            spacing: Theme.spacing

            Text {
                width: parent.width
                text: root.hasItem ? root.item.title : "未选择视频"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSubtitle
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }

            Flow {
                width: parent.width
                spacing: 6
                visible: root.hasItem && root.item.tags && root.item.tags.length > 0

                Repeater {
                    model: root.hasItem && root.item.tags ? root.item.tags : []

                    delegate: Rectangle {
                        required property var modelData

                        height: 24
                        width: tagText.implicitWidth + 18
                        radius: 12
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                        border.width: 1
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.3)

                        Text {
                            id: tagText
                            anchors.centerIn: parent
                            text: modelData
                            color: Theme.accentSoft
                            font.pixelSize: 10
                            font.weight: Font.Medium
                        }
                    }
                }
            }

            Text {
                width: parent.width
                visible: root.hasItem && root.item.plot.length > 0
                text: root.hasItem ? root.item.plot : ""
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                wrapMode: Text.WordWrap
                lineHeight: 1.45
                clip: true
                maximumLineCount: 14
                elide: Text.ElideRight
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.border
                visible: root.hasItem
            }

            Column {
                width: parent.width
                spacing: 7
                visible: root.hasItem

                Repeater {
                    model: root.hasItem ? [
                        {
                            label: "年份",
                            value: root.item.year
                        },
                        {
                            label: "时长",
                            value: root.item.durationText
                        },
                        {
                            label: "大小",
                            value: root.item.sizeText
                        },
                        {
                            label: "字幕",
                            value: root.item.subtitles && root.item.subtitles.length > 0 ? root.item.subtitles.length + " 个" : "无"
                        },
                        {
                            label: "剧照",
                            value: root.item.stillImages && root.item.stillImages.length > 0 ? root.item.stillImages.length + " 张" : "无"
                        },
                        {
                            label: "目录",
                            value: root.item.relativeFolder ? root.item.relativeFolder : "根目录"
                        }
                    ] : []

                    delegate: Row {
                        required property var modelData

                        width: column.width
                        spacing: Theme.spacingSmall

                        Text {
                            width: 46
                            text: modelData.label
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontCaption
                        }

                        Text {
                            width: parent.width - 54
                            text: modelData.value
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideMiddle
                        }
                    }
                }
            }

            // -------------------------------------------------------- stills
            Column {
                width: parent.width
                spacing: Theme.spacingSmall
                visible: root.hasItem && root.item.stillImages && root.item.stillImages.length > 0

                Text {
                    text: "剧照"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                    font.letterSpacing: 1.2
                    font.weight: Font.DemiBold
                }

                Grid {
                    width: parent.width
                    columns: 2
                    spacing: 8

                    Repeater {
                        model: root.hasItem && root.item.stillImages ? root.item.stillImages : []

                        delegate: Rectangle {
                            required property var modelData

                            width: (column.width - 8) / 2
                            height: width * 0.58
                            radius: Theme.radiusSmall
                            color: Theme.surfaceElevated
                            clip: true

                            Image {
                                anchors.fill: parent
                                source: "file:///" + modelData
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: true

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: App.openWithSystemPlayer(modelData)
                                }
                            }
                        }
                    }
                }
            }

            // -------------------------------------------------------- actors
            Column {
                width: parent.width
                spacing: 5
                visible: root.hasItem && root.item.actors && root.item.actors.length > 0

                Text {
                    text: "演员"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                    font.letterSpacing: 1.2
                    font.weight: Font.DemiBold
                }

                Repeater {
                    model: root.hasItem && root.item.actors ? root.item.actors : []

                    delegate: Text {
                        required property var modelData

                        width: column.width
                        text: "· " + modelData
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideRight
                    }
                }
            }

            // ----------------------------------------------------- file paths
            Column {
                width: parent.width
                spacing: 6
                visible: root.hasItem

                Text {
                    width: parent.width
                    text: root.hasItem ? root.item.filePath : ""
                    color: Theme.textMuted
                    font.pixelSize: 10
                    wrapMode: Text.WrapAnywhere
                }

                Row {
                    spacing: Theme.spacingSmall

                    Rectangle {
                        height: 30
                        width: openLabel.implicitWidth + 24
                        radius: 15
                        color: Theme.glass
                        border.width: 1
                        border.color: Theme.border

                        Text {
                            id: openLabel
                            anchors.centerIn: parent
                            text: "系统播放器"
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: App.openWithSystemPlayer(root.item.filePath)
                        }
                    }

                    Rectangle {
                        height: 30
                        width: revealLabel.implicitWidth + 24
                        radius: 15
                        color: Theme.glass
                        border.width: 1
                        border.color: Theme.border

                        Text {
                            id: revealLabel
                            anchors.centerIn: parent
                            text: "打开目录"
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: App.revealInFileManager(root.item.filePath)
                        }
                    }
                }
            }
        }
    }
}
