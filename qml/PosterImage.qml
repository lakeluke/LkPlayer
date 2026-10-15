import QtQuick
import QtQuick.Effects
import LkPlayer

// Cover artwork with a rounded clipping mask, a graceful placeholder and a
// cache-busting hook so late generated thumbnails can be picked up.
Item {
    id: root

    property url source
    property int radius: Theme.radiusCard
    property int reloadTick: 0
    property bool showPlaceholder: true
    property size requestedSize: Qt.size(Math.max(1, width), Math.max(1, height))

    readonly property string baseSource: source ? source.toString() : ""
    readonly property string effectiveSource: {
        if (baseSource.length === 0)
            return "";
        if (reloadTick <= 0 || baseSource.indexOf("image://") !== 0)
            return baseSource;
        return baseSource + (baseSource.indexOf("?") >= 0 ? "&" : "?") + "v=" + reloadTick;
    }

    readonly property bool imageReady: image.status === Image.Ready && image.progress === 1.0

    PosterPlaceholder {
        anchors.fill: parent
        visible: root.showPlaceholder && !root.imageReady
    }

    Image {
        id: image
        anchors.fill: parent
        visible: false
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectCrop
        source: root.effectiveSource
        sourceSize: root.requestedSize
        smooth: true
        mipmap: true
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        radius: root.radius
        color: "black"
        visible: false
        layer.enabled: true
    }

    MultiEffect {
        anchors.fill: parent
        visible: root.imageReady
        source: image
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 0.5
    }
}
