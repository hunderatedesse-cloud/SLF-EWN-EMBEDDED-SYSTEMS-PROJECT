import QtQuick

Item {
    id: root
    property string name: "dashboard"
    property color stroke: "#8B93A1"

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = root.stroke
            ctx.fillStyle = root.stroke
            ctx.lineWidth = 1.45
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            const w = width
            const h = height
            const x = 2.5
            const y = 2.5
            const iw = w - 5
            const ih = h - 5

            ctx.beginPath()
            if (root.name === "dashboard") {
                const g = 1.6
                const cw = (iw - g) / 2
                const ch = (ih - g) / 2
                roundRect(ctx, x, y, cw, ch, 1.4)
                roundRect(ctx, x + cw + g, y, cw, ch * 0.72, 1.4)
                roundRect(ctx, x, y + ch + g, cw, ch, 1.4)
                roundRect(ctx, x + cw + g, y + ch * 0.72 + g, cw, ih - (ch * 0.72 + g), 1.4)
                ctx.stroke()
            } else if (root.name === "alerts") {
                ctx.moveTo(x, y + ih * 0.22); ctx.lineTo(x + iw, y + ih * 0.22)
                ctx.moveTo(x, y + ih * 0.50); ctx.lineTo(x + iw * 0.78, y + ih * 0.50)
                ctx.moveTo(x, y + ih * 0.78); ctx.lineTo(x + iw * 0.62, y + ih * 0.78)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(x + iw - 1.5, y + ih * 0.50, 1.7, 0, Math.PI * 2)
                ctx.fill()
            } else if (root.name === "map") {
                ctx.moveTo(x + iw * 0.5, y)
                ctx.bezierCurveTo(x + iw * 0.18, y + ih * 0.08, x + iw * 0.12, y + ih * 0.42, x + iw * 0.5, y + ih * 0.72)
                ctx.bezierCurveTo(x + iw * 0.88, y + ih * 0.42, x + iw * 0.82, y + ih * 0.08, x + iw * 0.5, y)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(x + iw * 0.5, y + ih * 0.32, iw * 0.13, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(x + iw * 0.12, y + ih * 0.90)
                ctx.lineTo(x + iw * 0.88, y + ih * 0.90)
                ctx.stroke()
            } else if (root.name === "zones") {
                const g = 1.8
                const cw = (iw - g) / 2
                const ch = (ih - g) / 2
                roundRect(ctx, x, y, cw, ch, 1.2)
                roundRect(ctx, x + cw + g, y, cw, ch, 1.2)
                roundRect(ctx, x, y + ch + g, cw, ch, 1.2)
                roundRect(ctx, x + cw + g, y + ch + g, cw, ch, 1.2)
                ctx.stroke()
            } else if (root.name === "settings") {
                ctx.arc(x + iw * 0.5, y + ih * 0.5, iw * 0.18, 0, Math.PI * 2)
                ctx.stroke()
                for (let i = 0; i < 6; ++i) {
                    const a = i * Math.PI / 3
                    const cx = x + iw * 0.5
                    const cy = y + ih * 0.5
                    ctx.beginPath()
                    ctx.moveTo(cx + Math.cos(a) * iw * 0.30, cy + Math.sin(a) * ih * 0.30)
                    ctx.lineTo(cx + Math.cos(a) * iw * 0.48, cy + Math.sin(a) * ih * 0.48)
                    ctx.stroke()
                }
            } else if (root.name === "serial") {
                roundRect(ctx, x + iw * 0.08, y + ih * 0.32, iw * 0.84, ih * 0.36, 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(x + iw * 0.28, y + ih * 0.32); ctx.lineTo(x + iw * 0.28, y + ih * 0.18)
                ctx.moveTo(x + iw * 0.50, y + ih * 0.32); ctx.lineTo(x + iw * 0.50, y + ih * 0.18)
                ctx.moveTo(x + iw * 0.72, y + ih * 0.32); ctx.lineTo(x + iw * 0.72, y + ih * 0.18)
                ctx.stroke()
            }
        }
    }

    function roundRect(ctx, x, y, w, h, r) {
        ctx.moveTo(x + r, y)
        ctx.lineTo(x + w - r, y)
        ctx.quadraticCurveTo(x + w, y, x + w, y + r)
        ctx.lineTo(x + w, y + h - r)
        ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h)
        ctx.lineTo(x + r, y + h)
        ctx.quadraticCurveTo(x, y + h, x, y + h - r)
        ctx.lineTo(x, y + r)
        ctx.quadraticCurveTo(x, y, x + r, y)
    }

    onNameChanged: canvas.requestPaint()
    onStrokeChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
}
