import QtQuick

Rectangle {
    id: root
    property real value: 0
    property color fill: Theme.accent
    implicitHeight: 6
    radius: height / 2
    color: Theme.bg
    Rectangle {
        width: parent.width * Math.max(0, Math.min(1, root.value))
        height: parent.height
        radius: parent.radius
        color: root.fill
        Behavior on width { NumberAnimation { duration: 350; easing.type: Easing.OutCubic } }
    }
}
