import QtQuick
import LkPlayer

// Vector drawn stand-in used whenever a video has neither a poster nor a
// generated frame yet.
Item {
    id: root

    property color tint: Theme.accent

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Theme.dark ? "#1B1D26" : "#E9EBF2"
            }
            GradientStop {
                position: 1.0
                color: Theme.dark ? "#12141A" : "#DDE0EA"
            }
        }
    }

    // Faint diagonal light sweep keeps the empty state from looking flat.
    Rectangle {
        anchors.fill: parent
        opacity: 0.35
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: 0.0
                color: Qt.rgba(1, 1, 1, 0.05)
            }
            GradientStop {
                position: 0.5
                color: Qt.rgba(1, 1, 1, 0.0)
            }
            GradientStop {
                position: 1.0
                color: Qt.rgba(1, 1, 1, 0.03)
            }
        }
    }

    Canvas {
        id: glyph
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height) * 0.42
        height: width
        opacity: 0.55

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const w = width;
            const h = height;

            ctx.lineWidth = Math.max(2, w * 0.075);
            ctx.strokeStyle = root.tint.toString();
            ctx.beginPath();
            ctx.arc(w / 2, h / 2, w * 0.38, 0, Math.PI * 2);
            ctx.stroke();

            ctx.fillStyle = root.tint.toString();
            ctx.beginPath();
            ctx.moveTo(w * 0.41, h * 0.30);
            ctx.lineTo(w * 0.74, h * 0.50);
            ctx.lineTo(w * 0.41, h * 0.70);
            ctx.closePath();
            ctx.fill();
        }

        Connections {
            target: Theme
            function onModeChanged() {
                glyph.requestPaint();
            }
        }
    }
}
