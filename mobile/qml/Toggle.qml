import QtQuick
import QtQuick.Layouts

// Fila con texto y un interruptor.
Item {
    id: root
    property string text: ""
    property string detail: ""
    property bool checked: false
    signal toggled(bool on)
    implicitHeight: Math.max(44, col.implicitHeight + 16)
    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: sw.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        Text { text: root.text; color: Theme.text; font.pixelSize: 15; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Text { visible: root.detail.length > 0; text: root.detail; color: Theme.dim; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    }
    Rectangle {
        id: sw
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 46; height: 28; radius: 14
        color: root.checked ? Theme.accent : Theme.surfaceHi
        Behavior on color { ColorAnimation { duration: 150 } }
        Rectangle {
            width: 22; height: 22; radius: 11; y: 3
            x: root.checked ? parent.width - width - 3 : 3
            color: "#ffffff"
            Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        }
    }
    MouseArea { anchors.fill: parent; onClicked: root.toggled(!root.checked) }
}
