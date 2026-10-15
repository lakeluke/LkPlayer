import QtQuick
import LkPlayer

// Frosted card used for panels, sidebars and the control bar.
Rectangle {
    id: root

    property real elevation: 1.0
    property bool outlined: true
    property color tintColor: Theme.surface

    radius: Theme.radiusLarge
    color: tintColor
    border.width: outlined ? 1 : 0
    border.color: Theme.border

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Math.max(0, root.radius - 1)
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Qt.rgba(1, 1, 1, Theme.dark ? 0.05 : 0.6)
            }
            GradientStop {
                position: 0.55
                color: Qt.rgba(1, 1, 1, 0.0)
            }
        }
    }
}
