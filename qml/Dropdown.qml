import QtQuick
import QtQuick.Controls
import LkPlayer

// Themed dropdown list. A lightweight trigger button plus a popup list, so the
// text, spacing and alignment are fully under our control (unlike the stock
// ComboBox). Callers set `model` (a list of display strings) and `currentIndex`,
// and react to `activated`.
Item {
    id: control

    property var model: []
    property int currentIndex: 0
    property bool enabled: true
    signal activated(int index)

    implicitWidth: 120
    implicitHeight: 36

    readonly property string displayText: {
        if (!model || currentIndex < 0 || currentIndex >= model.length)
            return "";
        return model[currentIndex];
    }

    // The popup widens past the trigger to fit the longest option, so entries
    // are never clipped by the (otherwise fixed) trigger width.
    property real popupWidth: control.width

    TextMetrics {
        id: probe
        font.pixelSize: Theme.fontCaption
        text: ""
    }

    function recalcPopupWidth() {
        let maxW = 0;
        if (model && model.length > 0) {
            for (let i = 0; i < model.length; ++i) {
                probe.text = String(model[i]);
                maxW = Math.max(maxW, probe.advanceWidth);
            }
        }
        // 12px side padding in each row + 4px popup padding on each side.
        popupWidth = Math.max(control.width, maxW + 36);
    }

    onModelChanged: recalcPopupWidth()
    onWidthChanged: recalcPopupWidth()
    Component.onCompleted: recalcPopupWidth()

    // ------------------------------------------------------------- trigger
    Rectangle {
        id: trigger
        anchors.fill: parent
        radius: control.height / 2
        color: popup.opened
                   ? Theme.surfaceHover
                   : (triggerArea.containsMouse
                          ? Theme.surfaceHover
                          : (Theme.dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.045)))
        border.width: 1
        border.color: popup.opened
                          ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.55)
                          : Theme.border

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            text: control.displayText
            color: Theme.textPrimary
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideRight
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: "▾"
            color: Theme.textSecondary
            font.pixelSize: 10
        }

        MouseArea {
            id: triggerArea
            anchors.fill: parent
            enabled: control.enabled
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: popup.open()
        }
    }

    // ---------------------------------------------------------------- list
    Popup {
        id: popup
        parent: control
        x: 0
        y: control.height + 4
        width: control.popupWidth
        padding: 4
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: {
            // Flip upward when the list would run past the window's bottom edge
            // (the speed control sits in the bottom transport bar).
            const spaceBelow = (control.Window ? control.Window.height : 0)
                               - control.mapToItem(null, 0, 0).y - control.height;
            y = (spaceBelow < popup.height + 8) ? -(popup.height + 4) : (control.height + 4);
        }

        background: Rectangle {
            radius: Theme.radiusMedium
            color: Theme.surfaceElevated
            border.width: 1
            border.color: Theme.borderStrong
        }

        contentItem: Column {
            id: listColumn
            width: popup.width - 8
            spacing: 2

            Repeater {
                model: control.model

                delegate: Rectangle {
                    required property int index
                    required property var modelData

                    width: listColumn.width
                    height: 32
                    radius: Theme.radiusSmall
                    color: itemArea.containsMouse || control.currentIndex === index
                           ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                           : "transparent"

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        verticalAlignment: Text.AlignVCenter
                        text: modelData
                        color: control.currentIndex === index ? Theme.accentSoft : Theme.textPrimary
                        font.pixelSize: Theme.fontCaption
                        // No elide: the popup width was measured to fit the text.
                        elide: Text.ElideNone
                    }

                    MouseArea {
                        id: itemArea
                        anchors.fill: parent
                        enabled: control.enabled
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            control.currentIndex = index;
                            popup.close();
                            control.activated(index);
                        }
                    }
                }
            }
        }
    }
}
