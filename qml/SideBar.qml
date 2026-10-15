import QtQuick
import QtQuick.Dialogs
import LkPlayer

// Collapsible left column: library summary, folder navigation and tag filter.
Item {
    id: root

    signal folderSelected(string path)

    Rectangle {
        anchors.fill: parent
        color: Theme.dark ? Qt.rgba(1, 1, 1, 0.022) : Qt.rgba(0, 0, 0, 0.015)

        Rectangle {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: 1
            color: Theme.border
        }
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
            spacing: Theme.spacingLarge

            // ------------------------------------------------------- library
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
                    implicitHeight: libraryColumn.implicitHeight + 24
                    radius: Theme.radiusMedium
                    color: Theme.glass
                    border.width: 1
                    border.color: Theme.border

                    Column {
                        id: libraryColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: 12
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 6

                        Text {
                            width: parent.width
                            text: App.libraryRoot.length > 0 ? App.libraryRoot : "尚未设置目录"
                            color: App.libraryRoot.length > 0 ? Theme.textPrimary : Theme.textMuted
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideMiddle
                        }

                        Text {
                            width: parent.width
                            text: App.scannedVideos + " 个视频 · " + App.folderCount + " 个文件夹"
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                        }
                    }
                }

                Row {
                    width: parent.width
                    spacing: Theme.spacingSmall

                    Rectangle {
                        width: (parent.width - Theme.spacingSmall) / 2
                        height: 34
                        radius: 10
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
                            onClicked: folderDialog.open()
                        }
                    }

                    Rectangle {
                        width: (parent.width - Theme.spacingSmall) / 2
                        height: 34
                        radius: 10
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
                            enabled: !App.scanning
                            onClicked: App.rescan()
                        }
                    }
                }
            }

            // -------------------------------------------------------- folders
            Column {
                width: parent.width
                spacing: Theme.spacingSmall
                visible: App.viewMode === "drill"

                Text {
                    text: "目录导航"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                    font.letterSpacing: 1.2
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    width: parent.width
                    height: 34
                    radius: 10
                    color: upArea.containsMouse ? Theme.surfaceHover : "transparent"
                    border.width: 1
                    border.color: Theme.border

                    Row {
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "↑"
                            color: Theme.textSecondary
                            font.pixelSize: 13
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "返回上级"
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                        }
                    }

                    MouseArea {
                        id: upArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: App.navigateUp()
                    }
                }

                Repeater {
                    model: App.childFolders

                    delegate: Rectangle {
                        required property string name
                        required property string path
                        required property int videoCount

                        width: column.width
                        height: 32
                        radius: 9
                        color: childArea.containsMouse ? Theme.glassStrong : "transparent"

                        Behavior on color {
                            ColorAnimation {
                                duration: Theme.durationFast
                            }
                        }

                        Row {
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "▸"
                                color: Theme.accentSoft
                                font.pixelSize: 11
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 78
                                text: name
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                                elide: Text.ElideRight
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: videoCount
                                color: Theme.textMuted
                                font.pixelSize: 10
                            }
                        }

                        MouseArea {
                            id: childArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.folderSelected(path)
                        }
                    }
                }

                Text {
                    visible: App.childFolders.count === 0
                    width: parent.width
                    text: "该目录下没有子文件夹"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                }
            }

            // ----------------------------------------------------------- tags
            TagFilterPanel {
                width: parent.width
            }
        }
    }

    FolderDialog {
        id: folderDialog
        title: "选择媒体库根目录"
        onAccepted: App.setLibraryRoot(App.toLocalPath(selectedFolder.toString()))
    }
}
