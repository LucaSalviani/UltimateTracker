import QtQuick
import QtQuick.Layouts

// Botón de texto redondeado: "strong" = rojo lleno; "danger" = texto rojo.
Rectangle {
    id: root
    property string text: ""
    property bool strong: false
    property bool danger: false
    property bool enabledLook: true
    property string icon: ""
    signal clicked()
    implicitHeight: 46
    implicitWidth: row.implicitWidth + 36
    radius: height / 2
    opacity: enabledLook ? 1 : 0.4
    color: strong ? (ma.pressed ? Qt.darker(Theme.accent, 1.15) : Theme.accent)
                  : (ma.pressed ? Theme.surfaceHi : Theme.surface)
    border.color: strong ? "transparent" : Theme.line
    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 8
        Icon { visible: root.icon.length > 0; name: root.icon; width: 18; height: 18; color: root.strong ? "#ffffff" : root.danger ? Theme.accent : Theme.text }
        Text { text: root.text; color: root.strong ? "#ffffff" : root.danger ? Theme.accent : Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
    }
    MouseArea { id: ma; anchors.fill: parent; enabled: root.enabledLook; onClicked: root.clicked() }
}
