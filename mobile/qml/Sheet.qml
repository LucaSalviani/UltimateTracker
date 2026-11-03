import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Templates as T

// Hoja que sube desde abajo, con título y contenido que se puede desplazar.
Drawer {
    id: root
    property string title: ""
    property string subtitle: ""
    default property alias content: body.data
    property real maxFraction: 0.88
    edge: Qt.BottomEdge
    width: parent ? parent.width : 400
    height: Math.min((parent ? parent.height : 800) * maxFraction, body.implicitHeight + head.implicitHeight + 64)
    modal: true
    dim: true
    T.Overlay.modal: Rectangle { color: "#99000000" }
    background: Rectangle {
        color: Theme.sheet
        radius: 20
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 20; color: Theme.sheet }
        Rectangle { anchors.horizontalCenter: parent.horizontalCenter; y: 8; width: 36; height: 4; radius: 2; color: Theme.line }
    }

    ColumnLayout {
        id: head
        x: Theme.margin
        y: 26
        width: root.width - 2 * Theme.margin
        spacing: 2
        Label { text: root.title; color: Theme.text; font.pixelSize: 19; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
        Label { visible: root.subtitle.length > 0; text: root.subtitle; color: Theme.dim; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    }
    Flickable {
        id: flick
        anchors.top: head.bottom
        anchors.topMargin: 14
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        contentHeight: body.implicitHeight + 28
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height
        ColumnLayout {
            id: body
            x: Theme.margin
            width: flick.width - 2 * Theme.margin
            spacing: 12
        }
    }
}
