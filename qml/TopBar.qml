import QtQuick
import LkPlayer

// Sticky header: brand, search, view mode switch and settings entry point.
Item {
    id: root

    signal settingsRequested()
    signal libraryRequested()

    Rectangle {
        anchors.fill: parent
        color: Theme.dark ? Qt.rgba(0.055, 0.059, 0.075, 0.92) : Qt.rgba(1, 1, 1, 0.86)

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.border
        }
    }

    AppLogo {
        anchors.left: parent.left
        anchors.leftMargin: Theme.spacingLarge
        anchors.verticalCenter: parent.verticalCenter
    }

    // ------------------------------------------------------------------ search
    Rectangle {
        id: searchPill
        anchors.centerIn: parent
        width: Math.min(420, root.width * 0.36)
        height: 40
        radius: 20
        color: Theme.dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.045)
        border.width: 1
        border.color: searchInput.activeFocus
                      ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.7)
                      : Theme.border

        Behavior on border.color {
            ColorAnimation {
                duration: Theme.durationFast
            }
        }

        Text {
            id: searchIcon
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            text: "⌕"
            color: Theme.textMuted
            font.pixelSize: 18
        }

        TextInput {
            id: searchInput
            anchors.left: searchIcon.right
            anchors.leftMargin: 8
            anchors.right: clearButton.left
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height
            verticalAlignment: TextInput.AlignVCenter
            color: Theme.textPrimary
            font.pixelSize: Theme.fontBody
            selectByMouse: true
            selectionColor: Theme.accent
            selectedTextColor: "#FFFFFF"
            clip: true

            text: App.searchText
            onTextEdited: App.setSearchText(text)

            Connections {
                target: App
                function onSearchTextChanged() {
                    if (App.searchText !== searchInput.text)
                        searchInput.text = App.searchText;
                }
            }
        }

        Text {
            anchors.left: searchInput.left
            anchors.verticalCenter: parent.verticalCenter
            visible: searchInput.text.length === 0 && !searchInput.activeFocus
            text: "搜索标题、文件名或目录…"
            color: Theme.textMuted
            font.pixelSize: Theme.fontBody
        }

        Item {
            id: clearButton
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            width: 26
            height: 26
            visible: searchInput.text.length > 0

            Rectangle {
                anchors.fill: parent
                radius: 13
                color: clearArea.containsMouse ? Theme.surfaceHover : "transparent"
            }

            Text {
                anchors.centerIn: parent
                text: "✕"
                color: Theme.textSecondary
                font.pixelSize: 13
            }

            MouseArea {
                id: clearArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: App.setSearchText("")
            }
        }
    }

    // --------------------------------------------------------------- controls
    Row {
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacingLarge
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.spacingSmall

        // Scanning indicator
        Rectangle {
            visible: App.scanning
            height: 32
            width: scanLabel.implicitWidth + 34
            radius: 16
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
            border.width: 1
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)

            Row {
                anchors.centerIn: parent
                spacing: 7

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 8
                    height: 8
                    radius: 4
                    color: Theme.accentSoft

                    SequentialAnimation on opacity {
                        running: App.scanning
                        loops: Animation.Infinite
                        NumberAnimation {
                            to: 0.25
                            duration: 620
                            easing: Easing.InOutQuad
                        }
                        NumberAnimation {
                            to: 1.0
                            duration: 620
                            easing: Easing.InOutQuad
                        }
                    }
                }

                Text {
                    id: scanLabel
                    anchors.verticalCenter: parent.verticalCenter
                    text: "扫描中 " + App.scannedVideos
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                }
            }
        }

        // Segmented view mode switch
        Rectangle {
            height: 36
            width: 172
            radius: 18
            color: Theme.dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.045)
            border.width: 1
            border.color: Theme.border

            Rectangle {
                id: modePill
                width: (parent.width - 6) / 2
                height: parent.height - 6
                y: 3
                x: App.viewMode === "flat" ? 3 : 3 + width
                radius: height / 2
                gradient: Theme.accentGradient

                Behavior on x {
                    NumberAnimation {
                        duration: Theme.durationNormal
                        easing: Theme.easingStandard
                    }
                }
            }

            Row {
                anchors.fill: parent
                anchors.margins: 3

                Repeater {
                    model: [
                        {
                            key: "flat",
                            label: "全库平铺"
                        },
                        {
                            key: "drill",
                            label: "目录浏览"
                        }
                    ]

                    delegate: Item {
                        required property var modelData

                        width: (parent.width) / 2
                        height: parent.height

                        Text {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: App.viewMode === modelData.key ? "#FFFFFF" : Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                            font.weight: Font.DemiBold

                            Behavior on color {
                                ColorAnimation {
                                    duration: Theme.durationFast
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: App.setViewMode(modelData.key)
                        }
                    }
                }
            }
        }

        // Settings
        Rectangle {
            width: 36
            height: 36
            radius: 18
            color: settingsArea.containsMouse ? Theme.surfaceHover : (Theme.dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.045))
            border.width: 1
            border.color: Theme.border

            Behavior on color {
                ColorAnimation {
                    duration: Theme.durationFast
                }
            }

            Text {
                anchors.centerIn: parent
                text: "⚙"
                color: settingsArea.containsMouse ? Theme.accentSoft : Theme.textSecondary
                font.pixelSize: 17
            }

            MouseArea {
                id: settingsArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.settingsRequested()
            }
        }
    }
}
