import QtQuick

Item {
    id: root
    property int deviceId: 0
    property string idString: ""
    property string zone: ""
    property int status: 0
    property bool acknowledged: false
    property real moisture: 0
    property real tiltX: 0
    property real tiltY: 0
    property real tiltZ: 0
    property string relativeTime: ""
    property var sparkline: []

    property real shownMoisture: 0
    property real shownX: 0
    property real shownY: 0
    property real shownZ: 0
    property color chipColor: Theme.statusColor(status)
    property real lift: 0
    property real press: 1
    property bool ready: false
    readonly property bool criticalLive: status === 3 && !acknowledged
    readonly property bool warnLive: status === 2 || criticalLive

    Behavior on shownMoisture { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on shownX { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on shownY { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on shownZ { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on chipColor { ColorAnimation { duration: 200 } }
    Behavior on lift { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
    Behavior on press { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

    onMoistureChanged: shownMoisture = moisture
    onTiltXChanged: shownX = tiltX
    onTiltYChanged: shownY = tiltY
    onTiltZChanged: shownZ = tiltZ
    onStatusChanged: chipColor = Theme.statusColor(status)

    scale: press
    opacity: 0
    Component.onCompleted: {
        shownMoisture = moisture
        shownX = tiltX
        shownY = tiltY
        shownZ = tiltZ
        appear.start()
        ready = true
    }
    NumberAnimation { id: appear; target: root; property: "opacity"; to: 1; duration: Theme.motion; easing.type: Easing.OutCubic }

    Rectangle {
        anchors.fill: card
        anchors.margins: -6
        radius: 16
        color: root.chipColor
        opacity: mouse.containsMouse ? 0.12 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.motion } }
    }

    Rectangle {
        id: card
        anchors.fill: parent
        anchors.topMargin: -root.lift
        anchors.bottomMargin: root.lift
        radius: Theme.radiusCard
        color: mouse.containsMouse ? Theme.bgElevated : Theme.bgCard
        border.width: 1
        border.color: mouse.containsMouse
                      ? Qt.rgba(chipColor.r, chipColor.g, chipColor.b, 0.50)
                      : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.motion } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion } }

        Column {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 0

            Row {
                width: parent.width
                StatusChip {
                    text: Theme.statusLabel(root.status)
                    tone: root.chipColor
                }
                Item { width: 8; height: 1 }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.relativeTime
                    color: Theme.textDim
                    font.family: App.monoFont
                    font.pixelSize: 12
                }
            }

            Item { width: 1; height: 12 }

            Text {
                text: root.idString
                color: Theme.text
                font.family: App.monoFont
                font.pixelSize: 13
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.6
            }
            Text {
                width: parent.width
                text: root.zone
                elide: Text.ElideRight
                color: Theme.textMuted
                font.family: App.uiFont
                font.pixelSize: 12
            }

            Item { width: 1; height: 10 }

            Text {
                text: root.shownMoisture.toFixed(1) + "%"
                color: Theme.text
                font.family: App.monoFont
                font.pixelSize: 30
                font.weight: Font.Bold
            }
            Text {
                text: "Moisture"
                color: Theme.textMuted
                font.family: App.uiFont
                font.pixelSize: 11
                font.weight: Font.Medium
            }

            Item { width: 1; height: 12 }

            Row {
                width: parent.width
                height: 28
                spacing: 0
                Repeater {
                    model: [
                        { k: "Tilt X", v: root.shownX },
                        { k: "Y", v: root.shownY },
                        { k: "Z", v: root.shownZ }
                    ]
                    delegate: Row {
                        height: 28
                        Rectangle {
                            visible: index > 0
                            width: 1
                            height: 22
                            color: Theme.border
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Column {
                            width: 64
                            leftPadding: index > 0 ? 12 : 0
                            spacing: 2
                            Text {
                                text: (modelData.v >= 0 ? "+" : "") + modelData.v.toFixed(1) + "°"
                                color: Theme.text
                                font.family: App.monoFont
                                font.pixelSize: 13
                            }
                            Text {
                                text: modelData.k
                                color: Theme.textMuted
                                font.family: App.uiFont
                                font.pixelSize: 11
                                font.weight: Font.Medium
                            }
                        }
                    }
                }
            }

            Item { width: 1; height: 8 }

            Sparkline {
                width: parent.width
                height: 48
                values: root.sparkline
                lineColor: root.chipColor
            }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onEntered: root.lift = 2
        onExited: { root.lift = 0; root.press = 1 }
        onPressed: root.press = 0.98
        onReleased: root.press = 1
        onClicked: App.openDetail(root.deviceId)
    }
}
