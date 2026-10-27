import QtQuick

Item {
    id: root
    property string mode: "moisture"   // moisture | tilt
    property var points: []
    property real warnY: 38
    property real critY: 58
    property real reveal: 1
    property color lineColor: Theme.chartTeal

    onPointsChanged: canvas.requestPaint()
    onWarnYChanged: canvas.requestPaint()
    onCritYChanged: canvas.requestPaint()
    onRevealChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
    onModeChanged: canvas.requestPaint()

    function playReveal() {
        reveal = 0.15
        revealAnim.restart()
    }

    NumberAnimation {
        id: revealAnim
        target: root
        property: "reveal"
        to: 1
        duration: 350
        easing.type: Easing.OutCubic
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const w = width
            const h = height
            const l = 48
            const r = 12
            const t = 12
            const b = 28
            const pw = w - l - r
            const ph = h - t - b

            ctx.strokeStyle = Theme.borderSoft
            ctx.lineWidth = 1
            ctx.beginPath()
            ctx.moveTo(l, t)
            ctx.lineTo(l, t + ph)
            ctx.lineTo(l + pw, t + ph)
            ctx.stroke()

            const pts = root.points
            if (!pts || pts.length < 2)
                return

            let t0 = pts[0].t
            let t1 = pts[pts.length - 1].t
            if (t1 === t0) t1 = t0 + 1

            let ymin = 0, ymax = 1
            if (root.mode === "moisture") {
                ymin = 0
                ymax = 100
            } else {
                ymin = 1e9
                ymax = -1e9
                for (let i = 0; i < pts.length; ++i) {
                    ymin = Math.min(ymin, pts[i].x, pts[i].y, pts[i].z, pts[i].m)
                    ymax = Math.max(ymax, pts[i].x, pts[i].y, pts[i].z, pts[i].m)
                }
                const pad = Math.max(1.5, (ymax - ymin) * 0.18)
                ymin -= pad
                ymax += pad
            }

            function X(ms) { return l + ((ms - t0) / (t1 - t0)) * pw * root.reveal }
            function Y(v) { return t + ph - ((v - ymin) / (ymax - ymin)) * ph }

            function dashH(y, color) {
                ctx.save()
                ctx.strokeStyle = color
                ctx.lineWidth = 1
                ctx.setLineDash([5, 4])
                ctx.beginPath()
                ctx.moveTo(l, Y(y))
                ctx.lineTo(l + pw, Y(y))
                ctx.stroke()
                ctx.restore()
            }

            if (root.mode === "moisture") {
                dashH(root.warnY, Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.7))
                dashH(root.critY, Qt.rgba(Theme.critical.r, Theme.critical.g, Theme.critical.b, 0.7))
            }

            function drawLine(key, color, width, fill) {
                ctx.beginPath()
                ctx.moveTo(X(pts[0].t), Y(pts[0][key]))
                for (let i = 1; i < pts.length; ++i)
                    ctx.lineTo(X(pts[i].t), Y(pts[i][key]))
                if (fill) {
                    ctx.lineTo(X(pts[pts.length - 1].t), t + ph)
                    ctx.lineTo(X(pts[0].t), t + ph)
                    ctx.closePath()
                    const g = ctx.createLinearGradient(0, t, 0, t + ph)
                    g.addColorStop(0, Qt.rgba(color.r, color.g, color.b, 0.18))
                    g.addColorStop(1, Qt.rgba(color.r, color.g, color.b, 0.0))
                    ctx.fillStyle = g
                    ctx.fill()
                    ctx.beginPath()
                    ctx.moveTo(X(pts[0].t), Y(pts[0][key]))
                    for (let i = 1; i < pts.length; ++i)
                        ctx.lineTo(X(pts[i].t), Y(pts[i][key]))
                }
                ctx.strokeStyle = color
                ctx.lineWidth = width
                ctx.lineJoin = "round"
                ctx.stroke()
            }

            if (root.mode === "moisture") {
                drawLine("v", root.lineColor, 1.8, true)
            } else {
                drawLine("x", Theme.chartX, 1.2, false)
                drawLine("y", Theme.chartY, 1.2, false)
                drawLine("z", Theme.chartZ, 1.2, false)
                drawLine("m", Theme.chartMag, 2.1, false)
            }

            ctx.fillStyle = Theme.textDim
            // Family must be quoted: the CSS shorthand drops the space in
            // names like "Cascadia Mono" and the family resolves to nothing.
            ctx.font = '10px "' + App.monoFont + '"'
            ctx.fillText(ymax.toFixed(0), 6, t + 8)
            ctx.fillText(ymin.toFixed(0), 6, t + ph)
            const mid = new Date((t0 + t1) / 2)
            const left = new Date(t0)
            const right = new Date(t1)
            function fmt(d) {
                const hh = d.getHours().toString().padStart(2, "0")
                const mm = d.getMinutes().toString().padStart(2, "0")
                return hh + ":" + mm
            }
            ctx.fillText(fmt(left), l, h - 8)
            ctx.fillText(fmt(mid), l + pw / 2 - 14, h - 8)
            ctx.fillText(fmt(right), l + pw - 28, h - 8)
        }
    }
}
