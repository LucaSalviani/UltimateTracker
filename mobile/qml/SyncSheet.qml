import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Cuenta para sincronizar con la PC (la misma que pongas en la app de la PC).
Drawer {
    id: root
    property color bg: "#222222"
    property color textColor: "#ececec"
    property color dimColor: "#9a9a9a"
    property color lineColor: "#333333"
    property color accent: "#e0483f"
    property string note: ""

    edge: Qt.BottomEdge
    width: parent ? parent.width : 400
    height: content.implicitHeight + 48
    background: Rectangle { color: root.bg; radius: 18 }

    Connections {
        target: ut
        function onSyncMessage(text) { root.note = text }
    }

    component Field: TextField {
        Layout.fillWidth: true
        color: root.textColor
        placeholderTextColor: root.dimColor
        font.pixelSize: 16
        background: Rectangle { implicitHeight: 46; color: Qt.darker(root.bg, 1.3); radius: 10; border.color: root.lineColor }
    }
    component Action: Rectangle {
        property string label: ""
        property bool strong: false
        signal tapped()
        Layout.fillWidth: true
        implicitHeight: 46
        radius: 23
        color: strong ? (am.pressed ? Qt.darker(root.accent, 1.15) : root.accent) : (am.pressed ? Qt.lighter(root.bg, 1.3) : "transparent")
        border.color: strong ? "transparent" : root.lineColor
        Text { anchors.centerIn: parent; text: parent.label; color: parent.strong ? "#ffffff" : root.textColor; font.pixelSize: 15; font.weight: Font.DemiBold }
        MouseArea { id: am; anchors.fill: parent; onClicked: parent.tapped() }
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 22
        spacing: 12

        Label { text: "Sincronizar con la PC"; color: root.textColor; font.pixelSize: 18; font.weight: Font.DemiBold }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: root.dimColor
            font.pixelSize: 13
            text: ut.syncSignedIn ? ("Conectado como " + ut.syncEmail + ".\n" + ut.syncStatus)
                                  : "Entrá con la misma cuenta que en la app de la PC y tus pomodoros, actividades y objetivos se juntan solos."
        }

        Field { id: email; visible: !ut.syncSignedIn; placeholderText: "Mail"; inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase }
        Field { id: pass; visible: !ut.syncSignedIn; placeholderText: "Contraseña (6 o más letras)"; echoMode: TextInput.Password }
        Label {
            visible: root.note.length > 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: root.note
            color: root.textColor
            font.pixelSize: 13
        }
        RowLayout {
            visible: !ut.syncSignedIn
            Layout.fillWidth: true
            spacing: 10
            Action { label: "Crear cuenta"; onTapped: { root.note = ""; ut.signUp(email.text, pass.text) } }
            Action { label: "Entrar"; strong: true; onTapped: { root.note = ""; ut.signIn(email.text, pass.text) } }
        }
        RowLayout {
            visible: ut.syncSignedIn
            Layout.fillWidth: true
            spacing: 10
            Action { label: "Cerrar sesión"; onTapped: ut.signOut() }
            Action { label: "Sincronizar ahora"; strong: true; onTapped: ut.syncNow() }
        }
    }
}
