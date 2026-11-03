import QtQuick
import QtQuick.Layouts

// [−]  25 min  [+]
RowLayout {
    id: root
    property int value: 25
    property string suffix: ""
    property string label: ""
    property color textColor: Theme.text
    property color dimColor: Theme.dim
    property color buttonColor: Theme.surface
    property color lineColor: Theme.line
    signal minus()
    signal plus()
    spacing: 22

    component StepButton: Rectangle {
        property string sign: "minus"
        signal tapped()
        implicitWidth: 46; implicitHeight: 46; radius: 23
        color: ma.pressed ? Qt.lighter(root.buttonColor, 1.4) : root.buttonColor
        border.color: root.lineColor
        Icon { anchors.centerIn: parent; name: parent.sign; width: 20; height: 20; color: root.textColor }
        MouseArea {
            id: ma
            anchors.fill: parent
            onClicked: parent.tapped()
            onPressAndHold: repeat.start()
            onReleased: repeat.stop()
            onCanceled: repeat.stop()
        }
        Timer { id: repeat; interval: 110; repeat: true; onTriggered: parent.tapped() }
    }

    StepButton { sign: "minus"; onTapped: root.minus() }
    Column {
        Layout.minimumWidth: 92
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.value + root.suffix
            color: root.textColor
            font.pixelSize: 22
            font.weight: Font.DemiBold
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.label
            color: root.dimColor
            font.pixelSize: 12
        }
    }
    StepButton { sign: "plus"; onTapped: root.plus() }
}
