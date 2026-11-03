import QtQuick
import QtQuick.Layouts

// Barra de abajo: Hoy · Estadísticas · Hitos · Ajustes
Rectangle {
    id: root
    property int current: 0
    property bool running: false
    signal picked(int index)
    implicitHeight: 64
    color: Theme.bg
    Rectangle { width: parent.width; height: 1; color: Theme.line }
    RowLayout {
        anchors.fill: parent
        spacing: 0
        Repeater {
            model: [{icon: "timer", label: "Hoy"}, {icon: "bars", label: "Estadísticas"}, {icon: "trophy", label: "Hitos"}, {icon: "gear", label: "Ajustes"}]
            delegate: Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                readonly property bool on: index === root.current
                Column {
                    anchors.centerIn: parent
                    spacing: 3
                    Item {
                        width: 24; height: 24
                        anchors.horizontalCenter: parent.horizontalCenter
                        Icon { anchors.fill: parent; name: modelData.icon; color: on ? Theme.text : Theme.faint }
                        Rectangle {   // punto rojo: hay un pomodoro corriendo y estás en otra pestaña
                            visible: index === 0 && root.running && !on
                            width: 7; height: 7; radius: 3.5; color: Theme.accent
                            anchors.right: parent.right; anchors.top: parent.top
                        }
                    }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: on ? Theme.text : Theme.faint; font.pixelSize: 11; font.weight: on ? Font.DemiBold : Font.Normal }
                }
                MouseArea { anchors.fill: parent; onClicked: root.picked(index) }
            }
        }
    }
}
