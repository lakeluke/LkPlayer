import QtQuick
import LkPlayer

// Vector brand mark; drawn instead of shipping a bitmap so it stays crisp.
Item {
    id: root

    property real size: 30
    property bool withWordmark: true

    implicitWidth: withWordmark ? size + 10 + wordmark.width : size
    implicitHeight: size

    Canvas {
        id: mark
        width: root.size
        height: root.size
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const w = width;
            const h = height;

            const gradient = ctx.createLinearGradient(0, 0, w, h);
            gradient.addColorStop(0, Theme.accentSoft.toString());
            gradient.addColorStop(1, Theme.accentDeep.toString());

            ctx.lineWidth = Math.max(2, w * 0.1);
            ctx.strokeStyle = gradient;
            ctx.beginPath();
            ctx.arc(w / 2, h / 2, w * 0.42, 0, Math.PI * 2);
            ctx.stroke();

            ctx.fillStyle = gradient;
            ctx.beginPath();
            ctx.moveTo(w * 0.41, h * 0.29);
            ctx.lineTo(w * 0.75, h * 0.5);
            ctx.lineTo(w * 0.41, h * 0.71);
            ctx.closePath();
            ctx.fill();

            ctx.fillStyle = Qt.rgba(1, 1, 1, 0.9);
            ctx.beginPath();
            ctx.arc(w * 0.80, h * 0.20, w * 0.075, 0, Math.PI * 2);
            ctx.fill();
        }

        Connections {
            target: Theme
            function onModeChanged() {
                mark.requestPaint();
            }
        }
    }

    Text {
        id: wordmark
        visible: root.withWordmark
        anchors.left: mark.right
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        text: "LkPlayer"
        color: Theme.textPrimary
        font.pixelSize: 19
        font.weight: Font.DemiBold
    }
}
