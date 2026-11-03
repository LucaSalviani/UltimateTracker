import QtQuick
import QtQuick.Layouts

// Tarjeta gris con contenido en columna.
Rectangle {
    id: root
    default property alias content: col.data
    property alias spacing: col.spacing
    property bool pressable: false
    signal clicked()
    implicitHeight: col.implicitHeight + 32
    radius: Theme.radius
    color: pressable && ma.pressed ? Theme.surfaceHi : Theme.surface
    border.color: Theme.line
    ColumnLayout {
        id: col
        x: 16; y: 16
        width: root.width - 32
        spacing: 8
    }
    MouseArea { id: ma; anchors.fill: parent; enabled: root.pressable; onClicked: root.clicked(); z: -1 }
}
