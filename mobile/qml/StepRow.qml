import QtQuick
import QtQuick.Layouts

// Fila de ajuste: texto a la izquierda, [−] valor [+] a la derecha.
Item {
    id: root
    property string text: ""
    property string detail: ""
    property string valueText: ""
    signal minus()
    signal plus()
    implicitHeight: Math.max(48, col.implicitHeight + 16)
    component Btn: Rectangle {
        property string icon: "plus"
        signal tapped()
        width: 36; height: 36; radius: 18
        color: m.pressed ? Theme.surfaceHi : Theme.bg
        border.color: Theme.line
        Icon { anchors.centerIn: parent; name: parent.icon; width: 16; height: 16; color: Theme.text }
        MouseArea { id: m; anchors.fill: parent; onClicked: parent.tapped(); onPressAndHold: rep.start(); onReleased: rep.stop(); onCanceled: rep.stop() }
        Timer { id: rep; interval: 120; repeat: true; onTriggered: parent.tapped() }
    }
    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: ctl.left
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        Text { text: root.text; color: Theme.text; font.pixelSize: 15; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Text { visible: root.detail.length > 0; text: root.detail; color: Theme.dim; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    }
    RowLayout {
        id: ctl
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        Btn { icon: "minus"; onTapped: root.minus() }
        Text { Layout.minimumWidth: 58; horizontalAlignment: Text.AlignHCenter; text: root.valueText; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
        Btn { icon: "plus"; onTapped: root.plus() }
    }
}
