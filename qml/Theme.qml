pragma Singleton

import QtQuick

// Design tokens: colours, geometry, typography and motion.
//
// Every colour is written inline as a `dark ? ... : ...` literal on purpose.
// Pulling the values out of a second QML object (a separate Palette component)
// made the bindings evaluate before that object finished constructing, which
// produced "Unable to assign [undefined] to QColor" warnings at start-up.
QtObject {
    id: theme

    property string mode: "dark"
    readonly property bool dark: mode !== "light"

    // ------------------------------------------------------------- colours
    readonly property color background: dark ? "#0E0F13" : "#F2F3F7"
    readonly property color backgroundDeep: dark ? "#08090C" : "#E7E9F0"
    readonly property color surface: dark ? "#16181F" : "#FFFFFF"
    readonly property color surfaceElevated: dark ? "#1E2029" : "#FFFFFF"
    readonly property color surfaceHover: dark ? "#262936" : "#EDEFF6"
    readonly property color border: dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.08)
    readonly property color borderStrong: dark ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(0, 0, 0, 0.16)
    readonly property color textPrimary: dark ? "#F5F7FA" : "#14161C"
    readonly property color textSecondary: dark ? "#A0A6B8" : "#565C6E"
    readonly property color textMuted: dark ? "#676C7E" : "#878CA0"
    readonly property color accent: dark ? "#7C5CFF" : "#6A46F5"
    readonly property color accentSoft: dark ? "#A78BFA" : "#8B6DFF"
    readonly property color accentDeep: dark ? "#5B3DF5" : "#4A2FD0"
    readonly property color success: dark ? "#22C55E" : "#16A34A"
    readonly property color danger: dark ? "#EF4444" : "#DC2626"
    readonly property color warning: dark ? "#F59E0B" : "#D97706"

    readonly property color glass: dark ? Qt.rgba(1, 1, 1, 0.045) : Qt.rgba(1, 1, 1, 0.75)
    readonly property color glassStrong: dark ? Qt.rgba(1, 1, 1, 0.075) : Qt.rgba(1, 1, 1, 0.92)
    readonly property color scrim: Qt.rgba(0, 0, 0, dark ? 0.55 : 0.35)

    // Text colour that stays readable on top of the accent gradient.
    // (Deliberately not called `onAccent`: QML reserves the `on*` prefix for
    // signal handlers.)
    readonly property color accentText: "#FFFFFF"

    // ------------------------------------------------------------ geometry
    readonly property int radiusSmall: 8
    readonly property int radiusMedium: 12
    readonly property int radiusCard: 16
    readonly property int radiusLarge: 22
    readonly property int spacingTiny: 4
    readonly property int spacingSmall: 8
    readonly property int spacing: 14
    readonly property int spacingLarge: 22
    readonly property int spacingHuge: 34

    /** Cover height = card width * cardAspect (2:3 portrait poster). */
    readonly property real cardAspect: 1.5
    readonly property int cardMinWidth: 176
    readonly property int cardMaxWidth: 244
    /** Breathing room around each grid card so posters never touch. */
    readonly property int cardGap: 12
    /** Vertical room reserved below a cover for the title / meta block. */
    readonly property int cardFooter: 64
    readonly property int topBarHeight: 68
    readonly property int sidebarWidth: 268

    // ---------------------------------------------------------- typography
    //
    // There is deliberately no `fontFamily` token. A QML item that never sets a
    // family falls back to QGuiApplication::font(), which Qt already seeds from
    // the platform theme (Microsoft YaHei UI here), and that is inherited down
    // the item tree. Pinning a literal name ("Noto Sans") instead made Qt fall
    // back to the legacy Tahoma / MS Shell Dlg 2 mapping, whose CJK glyphs come
    // from 宋体 (SimSun).
    readonly property int fontDisplay: 30
    readonly property int fontTitle: 21
    readonly property int fontSubtitle: 16
    readonly property int fontBody: 13
    readonly property int fontCaption: 11

    // -------------------------------------------------------------- motion
    readonly property int durationFast: 140
    readonly property int durationNormal: 220
    readonly property int durationSlow: 320
    readonly property int easingStandard: Easing.OutCubic
    readonly property int easingEmphasis: Easing.OutBack

    // ----------------------------------------------------------- gradients
    readonly property Gradient accentGradient: Gradient {
        GradientStop {
            position: 0.0
            color: theme.accentSoft
        }
        GradientStop {
            position: 1.0
            color: theme.accentDeep
        }
    }

    readonly property Gradient coverScrim: Gradient {
        GradientStop {
            position: 0.0
            color: Qt.rgba(0, 0, 0, 0)
        }
        GradientStop {
            position: 0.62
            color: Qt.rgba(0, 0, 0, 0.1)
        }
        GradientStop {
            position: 1.0
            color: Qt.rgba(0, 0, 0, 0.78)
        }
    }
}
