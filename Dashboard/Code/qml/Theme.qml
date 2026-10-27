pragma Singleton
import QtQuick

QtObject {
    readonly property color bgDeep: "#0B0F0E"
    readonly property color bgSidebar: "#0B0F0E"
    readonly property color bgTopBar: "#0B0F0E"
    readonly property color bgCard: "#121816"
    readonly property color bgCardHover: "#1A211E"
    readonly property color bgInset: "#0F1412"
    readonly property color bgElevated: "#1A211E"
    readonly property color border: "#1F2A26"
    readonly property color borderSoft: "#1F2A26"
    readonly property color safe: "#2DD4A8"
    readonly property color warning: "#F5A623"
    readonly property color critical: "#EF4444"
    readonly property color action: "#2DD4A8"
    readonly property color actionInk: "#0B0F0E"
    readonly property color text: "#E8F0ED"
    readonly property color textMuted: "#A3B5AE"
    readonly property color textDim: "#8B9A94"
    readonly property color textFaint: "#5C6B66"
    readonly property color chartTeal: "#2DD4A8"
    readonly property color chartX: "#6A9BB8"
    readonly property color chartY: "#F5A623"
    readonly property color chartZ: "#8FA6B2"
    readonly property color chartMag: "#E8F0ED"

    readonly property int radiusCtl: 8
    readonly property int radiusCard: 12
    readonly property int padPage: 20
    readonly property int gap: 16
    readonly property int topBarHeight: 56
    readonly property int railWidth: 64
    readonly property int motion: 200

    function statusColor(s) {
        switch (s) {
        case 1: return safe
        case 2: return warning
        case 3: return critical
        default: return textDim
        }
    }

    function eventFill(t) {
        switch (t) {
        case 0: return warning
        case 1: return critical
        case 2: return "#B91C1C"
        default: return textDim
        }
    }

    function kindFill(kind) {
        switch (kind) {
        case "safe": return safe
        case "critical": return critical
        case "emergency": return "#B91C1C"
        case "info": return safe
        default: return warning
        }
    }

    function statusLabel(s) {
        switch (s) {
        case 1: return "Safe"
        case 2: return "Warning"
        case 3: return "Critical"
        default: return "No data"
        }
    }
}
