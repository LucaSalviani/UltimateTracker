import QtQuick

// Botón redondo con ícono dibujado y una leyenda abajo.
Item {
    id: root
    property real diameter: 72
    property color fillColor: "#e0483f"
    property color lineColor: "transparent"
    property color iconColor: "#ffffff"
    property color captionColor: "#9a9a9a"
    property string icon: "play"     // play, pause, stop, skip, coffee
    property string caption: ""
    signal clicked()
    implicitWidth: Math.max(diameter, cap.implicitWidth)
    implicitHeight: diameter + 8 + cap.implicitHeight
    onIconChanged: ic.requestPaint()
    onIconColorChanged: ic.requestPaint()

    Rectangle {
        id: disc
        width: root.diameter; height: width; radius: width / 2
        anchors.horizontalCenter: parent.horizontalCenter
        color: ma.pressed ? Qt.darker(root.fillColor, 1.15) : root.fillColor
        border.color: root.lineColor
        scale: ma.pressed ? 0.95 : 1
        Behavior on scale { NumberAnimation { duration: 90 } }
        Behavior on color { ColorAnimation { duration: 200 } }
        Canvas {
            id: ic
            anchors.centerIn: parent
            width: root.diameter * 0.42; height: width
            antialiasing: true
            onPaint: {
                const c = getContext("2d"); c.reset()
                const w = width
                c.fillStyle = root.iconColor; c.strokeStyle = root.iconColor
                c.lineWidth = w * 0.09; c.lineCap = "round"; c.lineJoin = "round"
                if (root.icon === "play") {
                    c.beginPath(); c.moveTo(w * 0.28, w * 0.16); c.lineTo(w * 0.86, w * 0.5); c.lineTo(w * 0.28, w * 0.84); c.closePath(); c.fill()
                } else if (root.icon === "pause") {
                    c.fillRect(w * 0.22, w * 0.16, w * 0.18, w * 0.68); c.fillRect(w * 0.60, w * 0.16, w * 0.18, w * 0.68)
                } else if (root.icon === "stop") {
                    c.fillRect(w * 0.24, w * 0.24, w * 0.52, w * 0.52)
                } else if (root.icon === "skip") {
                    c.beginPath(); c.moveTo(w * 0.18, w * 0.2); c.lineTo(w * 0.62, w * 0.5); c.lineTo(w * 0.18, w * 0.8); c.closePath(); c.fill()
                    c.fillRect(w * 0.66, w * 0.2, w * 0.14, w * 0.6)
                } else if (root.icon === "coffee") {
                    c.beginPath(); c.moveTo(w * 0.16, w * 0.36); c.lineTo(w * 0.70, w * 0.36); c.lineTo(w * 0.64, w * 0.84); c.lineTo(w * 0.22, w * 0.84); c.closePath(); c.fill()
                    c.beginPath(); c.arc(w * 0.74, w * 0.54, w * 0.12, -Math.PI / 2, Math.PI / 2); c.stroke()
                    c.beginPath(); c.moveTo(w * 0.36, w * 0.10); c.lineTo(w * 0.36, w * 0.24); c.moveTo(w * 0.50, w * 0.10); c.lineTo(w * 0.50, w * 0.24); c.stroke()
                }
            }
        }
        MouseArea { id: ma; anchors.fill: parent; onClicked: root.clicked() }
    }
    Text {
        id: cap
        anchors.top: disc.bottom
        anchors.topMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.caption
        color: root.captionColor
        font.pixelSize: 12
    }
}
