import QtQuick

Item {
    id: root
    property int pulseTick: 0
    property int hoverId: 0
    property real hoverMx: 0
    property real hoverMy: 0
    property string hoverText: ""

    Timer {
        interval: 40
        running: true
        repeat: true
        onTriggered: {
            pulseTick++
            canvas.requestPaint()
        }
    }

    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    Connections {
        target: App
        function onMapNodesChanged() { canvas.requestPaint() }
    }

    Text {
        x: Theme.padPage
        y: 16
        text: "Zone plot"
        color: Theme.text
        font.family: App.uiFont
        font.pixelSize: 20
        font.weight: Font.Medium
    }
    Text {
        x: Theme.padPage
        y: 42
        text: "Schematic site layout — not a geographic map"
        color: Theme.textDim
        font.family: App.uiFont
        font.pixelSize: 12
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        anchors.margins: Theme.padPage
        anchors.topMargin: 68
        antialiasing: true

        property var hitNodes: []

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const w = width
            const h = height

            ctx.fillStyle = Theme.bgCard
            roundFill(ctx, 0, 0, w, h, 12)
            ctx.fill()
            ctx.strokeStyle = Theme.border
            ctx.lineWidth = 1
            ctx.stroke()

            ctx.strokeStyle = Qt.rgba(Theme.textDim.r, Theme.textDim.g, Theme.textDim.b, 0.08)
            ctx.lineWidth = 1
            for (let gx = 24; gx < w; gx += 32) {
                ctx.beginPath(); ctx.moveTo(gx + 0.5, 12); ctx.lineTo(gx + 0.5, h - 12); ctx.stroke()
            }
            for (let gy = 24; gy < h; gy += 32) {
                ctx.beginPath(); ctx.moveTo(12, gy + 0.5); ctx.lineTo(w - 12, gy + 0.5); ctx.stroke()
            }

            const zones = App.zones
            const nodes = App.mapNodes
            hitNodes = []

            ctx.font = '12px "' + App.uiFont + '"'
            for (let i = 0; i < zones.length; ++i) {
                const z = zones[i]
                const zx = 16 + z.x * (w - 32)
                const zy = 16 + z.y * (h - 32)
                const zw = z.w * (w - 32)
                const zh = z.h * (h - 32)
                ctx.save()
                ctx.setLineDash([5, 5])
                ctx.lineWidth = 1.25
                ctx.globalAlpha = 0.40
                ctx.strokeStyle = z.color || Theme.safe
                roundStroke(ctx, zx, zy, zw, zh, 8)
                ctx.stroke()
                ctx.restore()
                ctx.fillStyle = Theme.textMuted
                ctx.fillText(z.name, zx + 10, zy + 18)
                if (z.coverage) {
                    ctx.fillStyle = Theme.textFaint
                    ctx.font = '11px "' + App.uiFont + '"'
                    ctx.fillText(z.coverage, zx + 10, zy + 34)
                    ctx.font = '12px "' + App.uiFont + '"'
                }
            }

            const t = pulseTick / (1600 / 40)
            const breath = 0.5 + 0.5 * Math.sin(t * Math.PI * 2)

            for (let i = 0; i < nodes.length; ++i) {
                const n = nodes[i]
                const nx = 16 + n.x * (w - 32)
                const ny = 16 + n.y * (h - 32)
                const col = Theme.statusColor(n.status)
                let radius = 6
                if (n.status >= 2 && !n.acknowledged)
                    radius = 6 * (1 + 0.08 * breath)

                ctx.beginPath()
                ctx.arc(nx, ny, radius + 8, 0, Math.PI * 2)
                ctx.fillStyle = Qt.rgba(col.r, col.g, col.b, 0.15)
                ctx.fill()

                ctx.beginPath()
                ctx.arc(nx, ny, radius, 0, Math.PI * 2)
                ctx.fillStyle = col
                ctx.fill()

                ctx.fillStyle = Theme.text
                ctx.font = '11px "' + App.monoFont + '"'
                ctx.fillText(n.idString, nx + 12, ny + 4)

                hitNodes.push({
                    id: n.deviceId,
                    x: nx,
                    y: ny,
                    r: radius + 10,
                    moisture: n.moisture,
                    tilt: n.tiltMag,
                    name: n.idString
                })
            }
        }

        function roundFill(ctx, x, y, w, h, r) {
            ctx.beginPath()
            ctx.moveTo(x + r, y)
            ctx.arcTo(x + w, y, x + w, y + h, r)
            ctx.arcTo(x + w, y + h, x, y + h, r)
            ctx.arcTo(x, y + h, x, y, r)
            ctx.arcTo(x, y, x + w, y, r)
            ctx.closePath()
        }
        function roundStroke(ctx, x, y, w, h, r) {
            roundFill(ctx, x, y, w, h, r)
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onPositionChanged: function(mouse) {
                const hits = canvas.hitNodes
                root.hoverId = 0
                root.hoverText = ""
                for (let i = 0; i < hits.length; ++i) {
                    const dx = mouse.x - hits[i].x
                    const dy = mouse.y - hits[i].y
                    if (dx * dx + dy * dy <= hits[i].r * hits[i].r) {
                        root.hoverId = hits[i].id
                        root.hoverMx = canvas.x + hits[i].x
                        root.hoverMy = canvas.y + hits[i].y
                        const m = (typeof hits[i].moisture === "number") ? hits[i].moisture.toFixed(1) : "—"
                        const t = (typeof hits[i].tilt === "number") ? hits[i].tilt.toFixed(1) : "—"
                        root.hoverText = hits[i].name + "   " + m + "%   tilt " + t + "°"
                        return
                    }
                }
            }
            onClicked: function(mouse) {
                const hits = canvas.hitNodes
                for (let i = 0; i < hits.length; ++i) {
                    const dx = mouse.x - hits[i].x
                    const dy = mouse.y - hits[i].y
                    if (dx * dx + dy * dy <= hits[i].r * hits[i].r) {
                        App.openDetail(hits[i].id)
                        return
                    }
                }
            }
        }
    }

    Rectangle {
        visible: root.hoverText.length > 0
        x: Math.min(root.hoverMx + 16, parent.width - width - 12)
        y: Math.max(12, root.hoverMy - 36)
        width: tipLab.implicitWidth + 16
        height: 28
        radius: 8
        color: Theme.bgElevated
        border.color: Theme.border
        border.width: 1
        z: 5
        Text {
            id: tipLab
            anchors.centerIn: parent
            text: root.hoverText
            color: Theme.text
            font.family: App.monoFont
            font.pixelSize: 11
        }
    }
}
