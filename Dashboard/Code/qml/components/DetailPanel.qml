import QtQuick
import QtQuick.Controls

Item {
    id: root
    visible: overlay.opacity > 0.001 || panel.x < width - 2

    readonly property real panelWidth: Math.max(380, Math.min(460, Math.round(width * 0.38)))
    property var node: App.selectedNode
    property int range: 0
    property var moisturePts: []
    property var tiltPts: []

    property int editorId: 0

    function reload() {
        moisturePts = App.moistureSeries(range)
        tiltPts = App.tiltSeries(range)
        mChart.playReveal()
        tChart.playReveal()
        const id = node.deviceId || 0
        if (id !== editorId) {
            editorId = id
            locName.text = node.locationName || ""
            locDesc.text = node.description || ""
            syncPlaceBox()
        }
    }

    function syncPlaceBox() {
        const zid = node.zoneId || "none"
        const z = App.zones
        placeBox.currentIndex = 0
        for (let i = 0; i < z.length; ++i) {
            if (z[i].id === zid) {
                placeBox.currentIndex = i + 1
                return
            }
        }
    }

    function saveMeta() {
        if (!Auth.isAdmin || !node.deviceId)
            return
        App.updateNodeMeta(node.deviceId, locName.text, locDesc.text)
        const z = App.zones
        const i = placeBox.currentIndex
        const placeId = (i <= 0) ? "none" : z[i - 1].id
        App.assignNode(node.deviceId, placeId)
    }

    Connections {
        target: App
        function onSelectedChanged() {
            if (App.detailOpen)
                reload()
            else
                root.editorId = 0
        }
        function onChartNeedsReload() {
            if (App.detailOpen)
                reload()
        }
    }

    Rectangle {
        id: overlay
        anchors.fill: parent
        color: "#000000"
        opacity: App.detailOpen ? 0.28 : 0
        Behavior on opacity { NumberAnimation { duration: App.detailOpen ? 320 : 280; easing.type: Easing.InOutQuad } }
        MouseArea {
            anchors.fill: parent
            enabled: App.detailOpen
            onClicked: App.closeDetail()
        }
    }

    Rectangle {
        id: panel
        width: root.panelWidth
        height: parent.height
        x: App.detailOpen ? root.width - width : root.width
        color: Theme.bgCard
        Behavior on x {
            NumberAnimation {
                duration: App.detailOpen ? 320 : 280
                easing.type: Easing.InOutQuad
            }
        }

        Rectangle {
            anchors.left: parent.left
            width: 1
            height: parent.height
            color: Theme.border
        }

        Column {
            id: header
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 22
            spacing: 6

            Row {
                spacing: 12
                width: parent.width
                Text {
                    text: node.idString || ""
                    color: Theme.text
                    font.family: App.monoFont
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }
                MessageBanner {
                    compact: true
                    kind: {
                        const s = node.status || 0
                        if (s === 3) return "critical"
                        if (s === 2) return "warning"
                        if (s === 1) return "safe"
                        return "info"
                    }
                    text: Theme.statusLabel(node.status || 0)
                    anchors.verticalCenter: parent.verticalCenter
                }
                Item { width: 1; height: 1 }
                Item {
                    width: header.width - 280
                    height: 1
                }
            }

            Text {
                text: node.zone || ""
                color: Theme.textMuted
                font.family: App.uiFont
                font.pixelSize: 13
            }
            Text {
                visible: !!node.hasGeo
                text: "Lat " + Number(node.latitude).toFixed(5)
                      + "   Lon " + Number(node.longitude).toFixed(5)
                      + (isFinite(node.altitude) ? ("   Alt " + Number(node.altitude).toFixed(1) + " m") : "")
                color: Theme.textDim
                font.family: App.monoFont
                font.pixelSize: 11
            }

            Item { width: 1; height: 6 }

            FieldInput {
                id: locName
                width: parent.width
                enabled: Auth.isAdmin
                label: "LOCATION NAME"
                placeholder: "optional site name"
            }
            FieldInput {
                id: locDesc
                width: parent.width
                enabled: Auth.isAdmin
                label: "DESCRIPTION"
                placeholder: "notes or metadata"
            }

            Text {
                text: "ASSIGNED PLACE"
                color: Theme.textDim
                font.family: App.uiFont
                font.pixelSize: 10
                font.letterSpacing: 1.1
                font.weight: Font.DemiBold
            }
            ComboBox {
                id: placeBox
                width: parent.width
                height: 32
                enabled: Auth.isAdmin
                font.family: App.uiFont
                font.pixelSize: 12
                model: {
                    const rows = [{ id: "none", name: "Unassigned" }]
                    const z = App.zones
                    for (let i = 0; i < z.length; ++i)
                        rows.push(z[i])
                    return rows
                }
                textRole: "name"
                displayText: currentIndex >= 0 && model && model[currentIndex] ? model[currentIndex].name : "Unassigned"
                background: Rectangle {
                    implicitHeight: 32
                    color: Theme.bgInset
                    border.color: Theme.border
                    radius: 6
                }
                contentItem: Text {
                    leftPadding: 10
                    text: placeBox.displayText
                    color: Theme.text
                    font: placeBox.font
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                delegate: ItemDelegate {
                    width: placeBox.width
                    highlighted: placeBox.highlightedIndex === index
                    contentItem: Text {
                        text: modelData.name
                        color: Theme.text
                        font.family: App.uiFont
                        font.pixelSize: 12
                    }
                    background: Rectangle { color: highlighted ? Theme.bgCardHover : Theme.bgCard }
                }
            }

            Text {
                text: "Limits  M " + Number(node.moistureWarning).toFixed(0) + "/" + Number(node.moistureCritical).toFixed(0)
                      + "%   T " + Number(node.tiltWarning).toFixed(0) + "/" + Number(node.tiltCritical).toFixed(0) + "°"
                      + (node.customThresholds ? "  (this node)" : "  (default)")
                color: Theme.textDim
                font.family: App.uiFont
                font.pixelSize: 11
            }

            Row {
                spacing: 10
                IndustrialButton {
                    visible: Auth.isAdmin
                    primary: true
                    text: "Save"
                    onClicked: root.saveMeta()
                }
                IndustrialButton {
                    visible: (node.status || 0) >= 2 && !node.acknowledged && Auth.isAdmin
                    text: "Acknowledge Alert"
                    onClicked: App.acknowledgeSelected()
                }
                IndustrialButton {
                    text: "Close"
                    onClicked: App.closeDetail()
                }
            }
        }

        SegmentedControl {
            id: seg
            anchors.top: header.bottom
            anchors.topMargin: 18
            anchors.right: parent.right
            anchors.rightMargin: 22
            currentIndex: root.range
            onIndexChanged: function(i) {
                root.range = i
                reload()
            }
        }

        Column {
            anchors.top: seg.bottom
            anchors.topMargin: 8
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 10
            anchors.rightMargin: 16
            anchors.bottomMargin: 16
            spacing: 8

            Text {
                text: "MOISTURE  %"
                color: Theme.textDim
                font.family: App.uiFont
                font.pixelSize: 10
                font.letterSpacing: 1.2
                font.weight: Font.DemiBold
                leftPadding: 12
            }
            TelemetryChart {
                id: mChart
                width: parent.width
                height: (parent.height - 48) / 2
                mode: "moisture"
                points: root.moisturePts
                warnY: App.moistureWarning
                critY: App.moistureCritical
                lineColor: Theme.chartTeal
            }
            Row {
                spacing: 14
                leftPadding: 12
                Text { text: "TILT  °"; color: Theme.textDim; font.family: App.uiFont; font.pixelSize: 10; font.letterSpacing: 1.2; font.weight: Font.DemiBold }
                Text { text: "X"; color: Theme.chartX; font.family: App.monoFont; font.pixelSize: 10 }
                Text { text: "Y"; color: Theme.chartY; font.family: App.monoFont; font.pixelSize: 10 }
                Text { text: "Z"; color: Theme.chartZ; font.family: App.monoFont; font.pixelSize: 10 }
                Text { text: "MAG"; color: Theme.chartMag; font.family: App.monoFont; font.pixelSize: 10 }
            }
            TelemetryChart {
                id: tChart
                width: parent.width
                height: (parent.height - 48) / 2
                mode: "tilt"
                points: root.tiltPts
            }
        }
    }
}
