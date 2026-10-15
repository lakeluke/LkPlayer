import QtQuick
import LkPlayer

// Root / Scene / Act1 / PPC-001 path with clickable ancestors.
Item {
    id: root

    implicitHeight: 40
    implicitWidth: crumbRow.width
    property var entries: App.breadcrumb

    Row {
        id: crumbRow
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Repeater {
            model: root.entries

            delegate: Row {
                required property var modelData
                required property int index

                readonly property bool isLast: index === root.entries.length - 1

                spacing: 2

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    height: 30
                    width: label.implicitWidth + 22
                    radius: 15
                    color: crumbArea.containsMouse ? Theme.surfaceHover : "transparent"
                    border.width: 0

                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.durationFast
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        visible: modelData.index === 0 && !crumbArea.containsMouse
                        radius: parent.radius
                        color: Theme.glass
                    }

                    Text {
                        id: label
                        anchors.centerIn: parent
                        text: modelData.name
                        color: isLast ? Theme.textPrimary : Theme.textSecondary
                        font.pixelSize: Theme.fontBody
                        font.weight: isLast ? Font.DemiBold : Font.Normal
                    }

                    MouseArea {
                        id: crumbArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: App.navigateToBreadcrumb(index)
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !isLast
                    text: "›"
                    color: Theme.textMuted
                    font.pixelSize: 16
                }
            }
        }
    }
}
