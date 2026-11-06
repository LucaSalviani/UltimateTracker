import QtQuick

// Los cuadraditos del día: hechos (oscuros, con la rayita del color de la actividad),
// el que corre (llenándose) y los fantasmas de lo que falta del objetivo.
Flow {
    id: root
    property var model: []
    property real progress: 0
    property string emptyText: "Todavía no hay cuadraditos. Dale play para sumar el primero."
    signal tapped(var id)
    // Mejoras 2.0: mantener apretado elige varios (para moverlos juntos)
    property var selected: []
    readonly property bool selecting: selected.length > 0
    function toggle(id) {
        const s = selected.slice(); const i = s.indexOf(id)
        if (i >= 0) s.splice(i, 1); else s.push(id)
        selected = s
    }
    spacing: 10
    readonly property int side: Math.floor((width - 4 * spacing) / 5)

    Text {
        visible: root.model.length === 0
        text: root.emptyText
        color: Theme.faint
        font.pixelSize: 13
        width: root.width
        wrapMode: Text.WordWrap
    }

    Repeater {
        model: root.model
        delegate: Item {
            width: root.side; height: root.side + 18
            readonly property bool ghost: modelData.ghost === true
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: ghost ? "" : "Nº" + modelData.number
                color: Theme.faint
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }
            Rectangle {
                id: tile
                y: 18
                width: root.side; height: root.side; radius: 9
                color: ghost ? "transparent" : modelData.running ? Theme.surface : "#0c0c0c"
                border.color: ghost ? "transparent" : modelData.running ? Theme.accent : Theme.line
                clip: true
                // fantasma: borde punteado
                Canvas {
                    visible: ghost
                    anchors.fill: parent
                    onPaint: {
                        const c = getContext("2d"); c.reset()
                        c.strokeStyle = Theme.faint; c.lineWidth = 1.2; c.setLineDash([4, 4])
                        const r = 9, w = width - 1, h = height - 1
                        c.beginPath(); c.roundedRect(0.5, 0.5, w, h, r, r); c.stroke()
                    }
                }
                Rectangle {   // lo que va: del que corre o la parte hecha del fantasma
                    visible: modelData.running || (ghost && modelData.fill > 0)
                    width: parent.width * Math.min(1, modelData.running ? root.progress : (modelData.fill || 0)); height: parent.height
                    color: modelData.running ? Theme.accent : Theme.surfaceHi
                    opacity: modelData.running ? 0.85 : 1
                }
                Rectangle {   // rayita del color de la actividad
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: ghost ? parent.width - 16 : parent.width; height: ghost ? 3 : 4
                    anchors.bottomMargin: ghost ? 6 : 0
                    radius: ghost ? 1.5 : 0
                    color: modelData.color || "transparent"
                    opacity: modelData.running ? 0 : ghost ? 0.55 : 0.9
                }
                Rectangle {   // tiene nota
                    visible: !ghost && (modelData.note || "").length > 0
                    anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 6
                    width: 6; height: 6; radius: 3; color: Theme.dim
                }
                Text {
                    anchors.centerIn: parent
                    text: modelData.minutes + "m"
                    color: ghost ? Theme.faint : Theme.text
                    font.pixelSize: Math.round(root.side * 0.26)
                    font.weight: Font.DemiBold
                }
                Rectangle {   // elegido
                    visible: root.selected.indexOf(modelData.id) >= 0
                    anchors.fill: parent; anchors.margins: -3
                    radius: 11; color: "transparent"; border.color: Theme.text; border.width: 2
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: !ghost
                    onClicked: root.selecting ? (modelData.running ? null : root.toggle(modelData.id)) : root.tapped(modelData.id)
                    onPressAndHold: if (!modelData.running) root.toggle(modelData.id)
                }
            }
        }
    }
}
