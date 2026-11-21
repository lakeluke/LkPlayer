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
    //
    // Palette follows the GitHub theme family from VS Code (GitHub Dark /
    // GitHub Light), mapped onto the Primer design tokens the theme ships with.
    readonly property color background: dark ? "#0D1117" : "#FFFFFF"
    readonly property color backgroundDeep: dark ? "#010409" : "#F6F8FA"
    readonly property color surface: dark ? "#161B22" : "#F6F8FA"
    readonly property color surfaceElevated: dark ? "#1C2128" : "#FFFFFF"
    readonly property color surfaceHover: dark ? "#30363D" : "#EFF2F5"
    readonly property color border: dark ? "#30363D" : "#D0D7DE"
    readonly property color borderStrong: dark ? "#484F58" : "#AFB8C1"
    readonly property color textPrimary: dark ? "#E6EDF3" : "#1F2328"
    readonly property color textSecondary: dark ? "#8B949E" : "#656D76"
    readonly property color textMuted: dark ? "#6E7681" : "#818B98"
    readonly property color accent: dark ? "#34D399" : "#059669"
    readonly property color accentSoft: dark ? "#6EE7B7" : "#047857"
    readonly property color accentDeep: dark ? "#059669" : "#065F46"
    readonly property color success: dark ? "#3FB950" : "#1A7F37"
    readonly property color danger: dark ? "#F85149" : "#CF222E"
    readonly property color warning: dark ? "#D29922" : "#9A6700"

    readonly property color glass: dark ? Qt.rgba(255, 255, 255, 0.05) : Qt.rgba(255, 255, 255, 0.75)
    readonly property color glassStrong: dark ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(255, 255, 255, 0.92)
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
