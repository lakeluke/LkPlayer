import QtQuick
import LkPlayer

// Multi select tag chips driving MediaFilterProxyModel.
Item {
    id: root

    implicitHeight: content.implicitHeight

    readonly property var tags: App.availableTags
    readonly property var selected: App.selectedTags

    function isSelected(tag) {
        for (let i = 0; i < selected.length; ++i) {
            if (String(selected[i]).toLowerCase() === String(tag).toLowerCase())
                return true;
        }
        return false;
    }

    Column {
        id: content
        width: root.width
        spacing: Theme.spacingSmall

        Row {
            width: parent.width
            spacing: Theme.spacingSmall

            Text {
                text: "标签筛选"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                font.letterSpacing: 1.2
                font.weight: Font.DemiBold
                anchors.verticalCenter: parent.verticalCenter
            }

            Item {
                width: Math.max(0, parent.width - 150)
                height: 1
            }

            Text {
                visible: root.selected.length > 0
                text: "清除 (" + root.selected.length + ")"
                color: Theme.accentSoft
                font.pixelSize: Theme.fontCaption
                anchors.verticalCenter: parent.verticalCenter

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -4
                    cursorShape: Qt.PointingHandCursor
                    onClicked: App.clearTags()
                }
            }
        }

        Text {
            visible: root.tags.length === 0
            width: parent.width
            text: "当前范围没有可用的标签"
            color: Theme.textMuted
            font.pixelSize: Theme.fontCaption
            wrapMode: Text.WordWrap
        }

        Flow {
            width: parent.width
            spacing: 6

            Repeater {
                model: root.tags

                delegate: Rectangle {
                    required property var modelData

                    readonly property bool active: root.isSelected(modelData)

                    height: 26
                    width: chipLabel.implicitWidth + 20
                    radius: 13
                    color: active ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.9) : Theme.glass
                    border.width: 1
                    border.color: active ? "transparent" : Theme.border

                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.durationFast
                        }
                    }

                    Text {
                        id: chipLabel
                        anchors.centerIn: parent
                        text: modelData
                        color: active ? "#FFFFFF" : Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }

                    MouseArea {
                        id: chipArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: App.toggleTag(modelData)
                    }

                    scale: chipArea.containsMouse ? 1.05 : 1.0

                    Behavior on scale {
                        NumberAnimation {
                            duration: Theme.durationFast
                            easing: Theme.easingStandard
                        }
                    }
                }
            }
        }
    }
}
