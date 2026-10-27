import QtQuick

Item {
    id: root

    readonly property int gap: Theme.gap
    readonly property int minCard: 280
    readonly property int cols: Math.max(1, Math.floor((width - 40 + gap) / (minCard + gap)))
    readonly property int cardW: Math.max(260, Math.floor((width - 40 - gap * (cols - 1)) / cols))
    readonly property int unassignedCount: {
        var n = 0
        var rows = App.mapNodes
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].everSeen && !rows[i].assigned)
                n++
        }
        return n
    }

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: grid.height + 40 + (root.unassignedCount > 0 ? 56 : 0)
        boundsBehavior: Flickable.StopAtBounds
        flickDeceleration: 2500

        Rectangle {
            visible: root.unassignedCount > 0
            x: Theme.padPage
            y: Theme.padPage
            width: root.width - Theme.padPage * 2
            height: 40
            radius: Theme.radiusCtl
            color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.14)
            border.color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.35)
            border.width: 1
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: root.unassignedCount + (root.unassignedCount === 1
                      ? " new node is unassigned — Settings → Nodes to place it"
                      : " new nodes are unassigned — Settings → Nodes to place them")
                color: Theme.warning
                font.family: App.uiFont
                font.pixelSize: 13
                font.weight: Font.Medium
            }
        }

        Grid {
            id: grid
            x: Theme.padPage
            y: root.unassignedCount > 0 ? 76 : Theme.padPage
            width: root.width - Theme.padPage * 2
            columns: root.cols
            columnSpacing: root.gap
            rowSpacing: root.gap

            Repeater {
                model: App.nodes
                delegate: NodeCard {
                    width: root.cardW
                    height: 248
                    deviceId: model.deviceId
                    idString: model.idString
                    zone: model.zone
                    status: model.status
                    acknowledged: model.acknowledged
                    moisture: model.moisture
                    tiltX: model.tiltX
                    tiltY: model.tiltY
                    tiltZ: model.tiltZ
                    relativeTime: model.relativeTime
                    sparkline: model.sparkline
                }
            }
        }
    }

    Column {
        visible: App.liveNodeCount === 0
        anchors.centerIn: parent
        spacing: 8
        width: 380
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: "Waiting for the field"
            color: Theme.text
            font.family: App.uiFont
            font.pixelSize: 20
            font.weight: Font.Medium
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: "Connect the receiver, then send TX FULL_DUMP. Nodes appear here as records decode."
            color: Theme.textMuted
            font.family: App.uiFont
            font.pixelSize: 13
            lineHeight: 1.45
        }
    }
}
