import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Elegir la actividad (o crear una nueva). Con allowAny aparece "Cualquiera" (para el objetivo).
Sheet {
    id: root
    property var current: 0
    property bool allowAny: false
    property bool allowAdd: true
    property string anyLabel: "Cualquiera"
    signal chosen(var id)
    title: allowAny ? "¿De qué actividad?" : "¿En qué vas a trabajar?"

    component Row_: Rectangle {
        id: rr
        property string label: ""
        property color dot: "transparent"
        property int depth: 0
        property bool on: false
        signal tapped()
        Layout.fillWidth: true
        implicitHeight: 48
        radius: 10
        color: ma.pressed ? Theme.surfaceHi : on ? Theme.surface : "transparent"
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12 + rr.depth * 20
            anchors.rightMargin: 12
            spacing: 12
            Rectangle { width: 10; height: 10; radius: 5; color: rr.dot; border.color: rr.dot == "#00000000" ? Theme.faint : "transparent" }
            Label { text: rr.label; color: Theme.text; font.pixelSize: 16; Layout.fillWidth: true; elide: Text.ElideRight }
            Icon { visible: rr.on; name: "check"; width: 18; height: 18; color: Theme.text }
        }
        MouseArea { id: ma; anchors.fill: parent; onClicked: rr.tapped() }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2
        Row_ { visible: root.allowAny; label: root.anyLabel; on: root.current == 0; onTapped: root.chosen(0) }
        Repeater {
            model: ut.folders
            delegate: Row_ {
                label: modelData.name; dot: modelData.color; depth: modelData.depth
                on: modelData.id == root.current
                onTapped: root.chosen(modelData.id)
            }
        }
        Text {
            visible: ut.folders.length === 0
            text: "Todavía no hay actividades. Creá la primera abajo."
            color: Theme.faint; font.pixelSize: 13
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            Layout.bottomMargin: 6
        }
    }
    RowLayout {
        visible: root.allowAdd
        Layout.fillWidth: true
        Layout.topMargin: 4
        spacing: 8
        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: "Nueva actividad"
            color: Theme.text
            font.pixelSize: 16
            placeholderTextColor: Theme.dim
            background: Rectangle { implicitHeight: 46; color: Theme.bg; radius: 10; border.color: field.activeFocus ? Theme.faint : Theme.line }
            onAccepted: add()
            function add() {
                const id = ut.addFolder(text)
                if (id > 0) { text = ""; root.chosen(id) }
            }
        }
        Pill { text: "Agregar"; strong: true; enabledLook: field.text.trim().length > 0; onClicked: field.add() }
    }
}
