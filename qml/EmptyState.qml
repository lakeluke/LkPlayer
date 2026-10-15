import QtQuick
import LkPlayer

// Centred illustration + guidance shown for empty, loading and error states.
Item {
    id: root

    property string icon: "📚"
    property string title: ""
    property string message: ""
    property string actionText: ""
    property bool busy: false
    signal actionTriggered()

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.7, 460)
        spacing: Theme.spacing

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 78
            height: 78
            radius: width / 2
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
            border.width: 1
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.32)

            Row {
                anchors.centerIn: parent
                visible: root.busy
                spacing: 6

                Repeater {
                    model: 3

                    Rectangle {
                        width: 9
                        height: 9
                        radius: 4.5
                        color: Theme.accent
                        opacity: 0.35
                        y: 0

                        SequentialAnimation on y {
                            running: root.busy
                            loops: Animation.Infinite
                            PauseAnimation {
                                duration: index * 130
                            }
                            NumberAnimation {
                                to: -9
                                duration: 320
                                easing: Easing.OutQuad
                            }
                            NumberAnimation {
                                to: 0
                                duration: 320
                                easing: Easing.InQuad
                            }
                            PauseAnimation {
                                duration: (2 - index) * 130
                            }
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: !root.busy
                text: root.icon
                font.pixelSize: 34
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            text: root.title
            color: Theme.textPrimary
            font.pixelSize: Theme.fontTitle
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            text: root.message
            visible: text.length > 0
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

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.actionText.length > 0
            width: Math.max(140, actionLabel.implicitWidth + 44)
            height: 42
            radius: 21
            gradient: Theme.accentGradient

            scale: actionArea.pressed ? 0.96 : (actionArea.containsMouse ? 1.04 : 1.0)
            Behavior on scale {
                NumberAnimation {
                    duration: Theme.durationFast
                    easing: Theme.easingStandard
                }
            }

            Text {
                id: actionLabel
                anchors.centerIn: parent
                text: root.actionText
                color: "#FFFFFF"
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
            }

            MouseArea {
                id: actionArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.actionTriggered()
            }
        }
    }
}
