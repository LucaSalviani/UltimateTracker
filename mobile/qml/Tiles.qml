import QtQuick

// Los cuadraditos de hoy: hechos (oscuros, con la rayita del color de la actividad)
// y el que corre, llenándose.
Flow {
    id: root
    property var model: []
    property real progress: 0
    property color accent: "#e0483f"
    property color surface: "#222222"
    property color line: "#333333"
    property color textColor: "#ececec"
    property color faint: "#5f5f5f"
    spacing: 10

    Text {
        visible: root.model.length === 0
        text: "Todavía no hay cuadraditos. Dale play para sumar el primero."
        color: root.faint
        font.pixelSize: 13
        width: root.width
        wrapMode: Text.WordWrap
    }

    Repeater {
        model: root.model
        delegate: Item {
            width: 64; height: 82
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Nº" + modelData.number
                color: root.faint
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }
            Rectangle {
                id: tile
                y: 18
                width: 64; height: 64; radius: 9
                color: modelData.running ? root.surface : "#0c0c0c"
                border.color: modelData.running ? root.accent : root.line
                clip: true
                Rectangle {   // lo que va del que corre
                    visible: modelData.running
                    width: parent.width * Math.min(1, root.progress); height: parent.height
                    color: root.accent
                    opacity: 0.85
                }
                Rectangle {   // rayita del color de la actividad
                    anchors.bottom: parent.bottom
                    width: parent.width; height: 4
                    color: modelData.color
                    opacity: modelData.running ? 0 : 0.9
                }
                Text {
                    anchors.centerIn: parent
                    text: modelData.minutes + "m"
                    color: root.textColor
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
