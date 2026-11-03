import QtQuick
import QtQuick.Layouts

// [ Día | Semana | Mes | Año ]
Rectangle {
    id: root
    property var options: []      // [{key, label}]
    property string current: ""
    signal picked(string key)
    implicitHeight: 38
    radius: 19
    color: Theme.surface
    border.color: Theme.line
    RowLayout {
        anchors.fill: parent
        anchors.margins: 3
        spacing: 0
        Repeater {
            model: root.options
            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: height / 2
                readonly property bool on: modelData.key === root.current
                color: on ? Theme.surfaceHi : "transparent"
                Behavior on color { ColorAnimation { duration: 150 } }
                Text { anchors.centerIn: parent; text: modelData.label; color: parent.on ? Theme.text : Theme.dim; font.pixelSize: 14; font.weight: parent.on ? Font.DemiBold : Font.Normal }
                MouseArea { anchors.fill: parent; onClicked: root.picked(modelData.key) }
            }
        }
    }
}
