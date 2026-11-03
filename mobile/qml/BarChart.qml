import QtQuick
import QtQuick.Layouts

// Barras de estudio por día o por mes. Verde = ese día se cumplió el objetivo.
Item {
    id: root
    property var bars: []
    property real maxSec: 0
    property int labelEvery: 1
    implicitHeight: 150
    readonly property real gap: bars.length > 14 ? 2 : 6
    readonly property real barW: bars.length ? (width - gap * (bars.length - 1)) / bars.length : 0
    property int selected: -1

    Text {   // el valor de la barra tocada (o el máximo)
        anchors.right: parent.right
        y: 0
        text: root.selected >= 0 && root.bars[root.selected] ? (root.bars[root.selected].label + " · " + (root.bars[root.selected].text || "0m"))
              : (root.maxSec > 0 ? "máx. " + Math.floor(root.maxSec / 3600) + "h" + (Math.floor(root.maxSec % 3600 / 60) < 10 ? "0" : "") + Math.floor(root.maxSec % 3600 / 60) : "")
        color: Theme.dim; font.pixelSize: 11
    }
    Repeater {
        model: root.bars
        delegate: Item {
            x: index * (root.barW + root.gap)
            y: 18
            width: root.barW
            height: root.height - 18
            readonly property real h: root.maxSec > 0 ? (height - 20) * modelData.sec / root.maxSec : 0
            Rectangle {   // fondo
                anchors.bottom: parent.bottom; anchors.bottomMargin: 18
                width: parent.width; height: parent.height - 20
                radius: Math.min(4, width / 2)
                color: modelData.current ? Theme.surfaceHi : modelData.future ? "transparent" : Theme.surface
            }
            Rectangle {
                anchors.bottom: parent.bottom; anchors.bottomMargin: 18
                width: parent.width
                height: modelData.sec > 0 ? Math.max(3, parent.h) : 0
                radius: Math.min(4, width / 2)
                color: modelData.goal === 1 ? Theme.good : Theme.accent
                opacity: root.selected === -1 || root.selected === index ? (modelData.current ? 1 : 0.85) : 0.4
                Behavior on height { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
            }
            Rectangle {   // objetivo no cumplido: rayita abajo
                visible: modelData.goal === 2
                anchors.bottom: parent.bottom; anchors.bottomMargin: 14
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.max(3, parent.width * 0.6); height: 2; radius: 1
                color: Theme.faint
            }
            Text {
                visible: index % root.labelEvery === 0 || modelData.current
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                text: modelData.label
                color: modelData.current ? Theme.text : Theme.faint
                font.pixelSize: 10
                font.weight: modelData.current ? Font.DemiBold : Font.Normal
            }
            MouseArea { anchors.fill: parent; onClicked: root.selected = root.selected === index ? -1 : index }
        }
    }
    onBarsChanged: selected = -1
}
