import QtQuick
import LkPlayer

Window {
    id: window

    width: 1480
    height: 920
    minimumWidth: 1024
    minimumHeight: 660
    visible: true
    title: "LkPlayer · 本地媒体库播放器"
    color: Theme.background

    Component.onCompleted: {
        Theme.mode = App.settings.themeMode;
        Qt.callLater(function () {
            App.attachWindow(window);
        });
    }

    Connections {
        target: App.settings
        function onThemeModeChanged() {
            Theme.mode = App.settings.themeMode;
        }
    }

    Connections {
        target: App
        function onPlaybackRequested(filePath) {
            pageHost.navigateTo("player");
        }
    }

    // ---------------------------------------------------------- background
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Theme.dark ? "#12141D" : "#FFFFFF"
            }
            GradientStop {
                position: 0.42
                color: Theme.background
            }
            GradientStop {
                position: 1.0
                color: Theme.dark ? "#0A0B10" : "#EEF0F6"
            }
        }
    }

    // Ambient accent glow keeps the dark shell from feeling flat.
    Rectangle {
        width: Math.max(parent.width, parent.height) * 0.55
        height: width
        radius: width / 2
        x: -width * 0.22
        y: -height * 0.72
        color: Theme.accent
        opacity: Theme.dark ? 0.13 : 0.08
        antialiasing: true
    }

    Item {
        id: pageHost
        anchors.fill: parent

        property string currentPage: "library"

        function navigateTo(name) {
            if (name === currentPage) {
                loader.opacity = 1;
                return;
            }
            currentPage = name;
            transition.restart();
        }

        Loader {
            id: loader
            anchors.fill: parent
        }

        SequentialAnimation {
            id: transition

            NumberAnimation {
                target: loader
                property: "opacity"
                to: 0
                duration: 110
                easing: Easing.OutQuad
            }

            ScriptAction {
                script: loader.sourceComponent = pageHost.componentFor(pageHost.currentPage)
            }

            ParallelAnimation {
                NumberAnimation {
                    target: loader
                    property: "opacity"
                    to: 1
                    duration: 200
                    easing: Easing.OutQuad
                }
                NumberAnimation {
                    target: loader
                    property: "scale"
                    from: 0.985
                    to: 1
                    duration: 240
                    easing: Easing.OutCubic
                }
            }
        }

        function componentFor(name) {
            if (name === "player")
                return playerComponent;
            if (name === "settings")
                return settingsComponent;
            return libraryComponent;
        }

        Component.onCompleted: loader.sourceComponent = libraryComponent
    }

    Component {
        id: libraryComponent

        LibraryPage {
            onSettingsRequested: pageHost.navigateTo("settings")
        }
    }

    Component {
        id: playerComponent

        PlayerPage {
            onBackRequested: pageHost.navigateTo("library")
        }
    }

    Component {
        id: settingsComponent

        SettingsPage {
            onBackRequested: pageHost.navigateTo("library")
        }
    }
}
