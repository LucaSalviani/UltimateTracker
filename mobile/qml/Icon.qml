import QtQuick

// Íconos dibujados (así no dependen de las fuentes del celu, que no traen todos los símbolos).
Canvas {
    id: root
    property string name: "check"
    property color color: Theme.text
    property real stroke: 1.8
    width: 22; height: 22
    antialiasing: true
    onNameChanged: requestPaint()
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()

    onPaint: {
        const c = getContext("2d"); c.reset()
        const w = width, h = height, s = Math.min(w, h) / 24
        c.save(); c.translate((w - 24 * s) / 2, (h - 24 * s) / 2); c.scale(s, s)
        c.strokeStyle = root.color; c.fillStyle = root.color
        c.lineWidth = Math.max(1.4, root.stroke * 24 / Math.min(w, h))
        c.lineCap = "round"; c.lineJoin = "round"
        const P = (pts) => { c.beginPath(); c.moveTo(pts[0], pts[1]); for (let i = 2; i < pts.length; i += 2) c.lineTo(pts[i], pts[i + 1]); c.stroke() }
        const circle = (x, y, r, fill) => { c.beginPath(); c.arc(x, y, r, 0, Math.PI * 2); fill ? c.fill() : c.stroke() }
        switch (root.name) {
        case "chevron-down": P([7, 10, 12, 15, 17, 10]); break
        case "chevron-up": P([7, 14, 12, 9, 17, 14]); break
        case "chevron-left": P([14, 6, 8, 12, 14, 18]); break
        case "chevron-right": P([10, 6, 16, 12, 10, 18]); break
        case "check": P([5, 12.5, 10, 17, 19, 7.5]); break
        case "close": P([6, 6, 18, 18]); P([18, 6, 6, 18]); break
        case "plus": P([12, 5, 12, 19]); P([5, 12, 19, 12]); break
        case "minus": P([5, 12, 19, 12]); break
        case "cloud":
            c.beginPath()
            c.moveTo(7, 18.5); c.lineTo(17.5, 18.5)
            c.arc(17.5, 14.5, 4, Math.PI / 2, -Math.PI / 2, true)
            c.arc(12, 10.5, 5.5, -0.15, Math.PI + 0.55, true)
            c.arc(7, 14.25, 4.25, -Math.PI / 2 - 0.2, Math.PI / 2, true)
            c.stroke(); break
        case "cloud-ok":
            c.beginPath()
            c.moveTo(7, 18.5); c.lineTo(17.5, 18.5)
            c.arc(17.5, 14.5, 4, Math.PI / 2, -Math.PI / 2, true)
            c.arc(12, 10.5, 5.5, -0.15, Math.PI + 0.55, true)
            c.arc(7, 14.25, 4.25, -Math.PI / 2 - 0.2, Math.PI / 2, true)
            c.stroke(); P([9.5, 14.5, 11.5, 16.3, 15, 12.5]); break
        case "timer":
            circle(12, 13.5, 7.5, false); P([12, 13.5, 12, 9.5]); P([10, 3.5, 14, 3.5]); P([12, 3.5, 12, 6]); break
        case "bars":
            P([5, 19, 5, 12]); P([10, 19, 10, 6]); P([15, 19, 15, 10]); P([20, 19, 20, 14]); break
        case "trophy":
            c.beginPath(); c.moveTo(7, 4); c.lineTo(17, 4); c.lineTo(17, 9); c.arc(12, 9, 5, 0, Math.PI, false); c.closePath(); c.stroke()
            c.beginPath(); c.moveTo(7, 6); c.lineTo(4.5, 6); c.arc(6.5, 8, 2.5, Math.PI, Math.PI / 2, true); c.stroke()
            c.beginPath(); c.moveTo(17, 6); c.lineTo(19.5, 6); c.arc(17.5, 8, 2.5, 0, Math.PI / 2, false); c.stroke()
            P([12, 14, 12, 18]); P([8.5, 20, 15.5, 20]); break
        case "gear":
            c.save(); c.lineWidth = c.lineWidth * 1.9; c.lineCap = "butt"
            for (let i = 0; i < 8; ++i) {
                const a = i * Math.PI / 4
                P([12 + Math.cos(a) * 6.5, 12 + Math.sin(a) * 6.5, 12 + Math.cos(a) * 9.3, 12 + Math.sin(a) * 9.3])
            }
            c.restore()
            circle(12, 12, 6.5, false); circle(12, 12, 2.6, false); break
        case "target":
            circle(12, 12, 8, false); circle(12, 12, 4.5, false); circle(12, 12, 1.4, true); break
        case "flame":
            c.beginPath(); c.moveTo(12, 3)
            c.bezierCurveTo(13, 7, 18, 9, 18, 14); c.bezierCurveTo(18, 18, 15, 21, 12, 21)
            c.bezierCurveTo(9, 21, 6, 18, 6, 14.5); c.bezierCurveTo(6, 11.5, 8, 10, 9, 8)
            c.bezierCurveTo(9.5, 10, 10.5, 11, 11.5, 11); c.bezierCurveTo(12.5, 8.5, 12.5, 6, 12, 3); c.stroke(); break
        case "pencil":
            P([5, 19, 6, 15, 15.5, 5.5, 18.5, 8.5, 9, 18, 5, 19]); P([13.5, 7.5, 16.5, 10.5]); break
        case "trash":
            P([4.5, 7, 19.5, 7]); P([9.5, 7, 9.5, 4.5, 14.5, 4.5, 14.5, 7]); P([6.5, 7, 7.5, 20, 16.5, 20, 17.5, 7]); P([10.5, 10.5, 10.5, 16.5]); P([13.5, 10.5, 13.5, 16.5]); break
        case "folder":
            P([3.5, 7, 3.5, 18.5, 20.5, 18.5, 20.5, 8.5, 11.5, 8.5, 9.5, 6, 3.5, 6, 3.5, 7]); break
        case "medal":
            P([8, 3, 10.5, 9]); P([16, 3, 13.5, 9]); circle(12, 14.5, 5.5, false); circle(12, 14.5, 2, true); break
        case "calendar":
            c.strokeRect(4, 5.5, 16, 14.5); P([4, 10, 20, 10]); P([8.5, 3.5, 8.5, 7]); P([15.5, 3.5, 15.5, 7]); break
        case "undo":
            c.beginPath(); c.arc(12.5, 13, 6.5, Math.PI * 1.15, Math.PI * 0.75, false); c.stroke(); P([4, 7, 5.5, 11.5, 10, 10]); break
        case "dots":
            circle(6, 12, 1.6, true); circle(12, 12, 1.6, true); circle(18, 12, 1.6, true); break
        }
        c.restore()
    }
}
