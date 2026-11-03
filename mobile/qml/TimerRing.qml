import QtQuick

// Anillo de progreso con el tiempo al medio.
Item {
    id: root
    property real progress: 0
    property color ringColor: "#e0483f"
    property color trackColor: "#222222"
    property color textColor: "#ececec"
    property color kickerColor: "#e0483f"
    property string clockText: "25:00"
    property string kicker: ""

    onProgressChanged: canvas.requestPaint()
    onRingColorChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const w = width, r = w / 2 - 10
            ctx.lineWidth = 12
            ctx.lineCap = "round"
            ctx.strokeStyle = root.trackColor
            ctx.beginPath(); ctx.arc(w / 2, w / 2, r, 0, 2 * Math.PI); ctx.stroke()
            if (root.progress > 0) {
                ctx.strokeStyle = root.ringColor
                ctx.beginPath()
                ctx.arc(w / 2, w / 2, r, -Math.PI / 2, -Math.PI / 2 + 2 * Math.PI * Math.min(1, root.progress))
                ctx.stroke()
            }
        }
    }
    Column {
        anchors.centerIn: parent
        spacing: 6
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.kicker
            color: root.kickerColor
            font.pixelSize: 13
            font.weight: Font.DemiBold
            font.letterSpacing: 1.6
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.clockText
            color: root.textColor
            font.pixelSize: root.width * 0.24
            font.weight: Font.Bold
            font.family: "sans-serif-condensed"
        }
    }
}
