import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Elegir la actividad (o crear una nueva), desde abajo.
Drawer {
    id: root
    property var folders: []
    property var current: 0
    property color bg: "#222222"
    property color textColor: "#ececec"
    property color dimColor: "#9a9a9a"
    property color lineColor: "#333333"
    signal chosen(var id)
    signal added(string name)

    edge: Qt.BottomEdge
    width: parent ? parent.width : 400
    height: Math.min(parent ? parent.height * 0.75 : 600, list.contentHeight + 150)
    background: Rectangle { color: root.bg; radius: 18 }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10
        Label { text: "¿En qué vas a trabajar?"; color: root.textColor; font.pixelSize: 18; font.weight: Font.DemiBold }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.folders
            delegate: Rectangle {
                width: list.width
                height: 48
                color: ma.pressed ? Qt.lighter(root.bg, 1.3) : "transparent"
                radius: 10
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12 + modelData.depth * 20
                    anchors.rightMargin: 12
                    spacing: 12
                    Rectangle { width: 10; height: 10; radius: 5; color: modelData.color }
                    Label { text: modelData.name; color: root.textColor; font.pixelSize: 16; Layout.fillWidth: true; elide: Text.ElideRight }
                    Label { visible: modelData.id === root.current; text: "✓"; color: root.textColor; font.pixelSize: 16 }
                }
                MouseArea { id: ma; anchors.fill: parent; onClicked: root.chosen(modelData.id) }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            TextField {
                id: field
                Layout.fillWidth: true
                placeholderText: "Nueva actividad"
                color: root.textColor
                font.pixelSize: 16
                placeholderTextColor: root.dimColor
                background: Rectangle { implicitHeight: 46; color: Qt.darker(root.bg, 1.3); radius: 10; border.color: root.lineColor }
                onAccepted: { root.added(text); text = "" }
            }
            Rectangle {
                implicitWidth: 96; implicitHeight: 46; radius: 23
                readonly property bool ok: field.text.trim().length > 0
                color: ok ? (addMouse.pressed ? "#c23d35" : "#e0483f") : Qt.lighter(root.bg, 1.25)
                Text { anchors.centerIn: parent; text: "Agregar"; color: parent.ok ? "#ffffff" : root.dimColor; font.pixelSize: 15; font.weight: Font.DemiBold }
                MouseArea { id: addMouse; anchors.fill: parent; enabled: parent.ok; onClicked: { root.added(field.text); field.text = "" } }
            }
        }
    }
}
