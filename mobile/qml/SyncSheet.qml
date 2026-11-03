import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Cuenta para sincronizar con la PC (la misma que pongas en la app de la PC).
Sheet {
    id: root
    property string note: ""
    title: "Sincronizar con la PC"

    Connections {
        target: ut
        function onSyncMessage(text) { root.note = text }
    }

    component Field: TextField {
        Layout.fillWidth: true
        color: Theme.text
        placeholderTextColor: Theme.faint
        font.pixelSize: 16
        background: Rectangle { implicitHeight: 48; color: Theme.bg; radius: 12; border.color: parent.activeFocus ? Theme.faint : Theme.line }
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.dim
        font.pixelSize: 13
        text: ut.syncSignedIn ? ("Conectado como " + ut.syncEmail + ".\n" + ut.syncStatus)
                              : "Entrá con la misma cuenta que en la app de la PC y tus pomodoros, actividades y objetivos se juntan solos."
    }
    Field { id: email; visible: !ut.syncSignedIn; placeholderText: "Mail"; inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText }
    Field { id: pass; visible: !ut.syncSignedIn; placeholderText: "Contraseña (6 o más letras)"; echoMode: TextInput.Password }
    Label {
        visible: root.note.length > 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: root.note
        color: Theme.text
        font.pixelSize: 13
    }
    RowLayout {
        visible: !ut.syncSignedIn
        Layout.fillWidth: true
        spacing: 10
        Pill { Layout.fillWidth: true; text: "Crear cuenta"; onClicked: { root.note = ""; ut.signUp(email.text, pass.text) } }
        Pill { Layout.fillWidth: true; text: "Entrar"; strong: true; onClicked: { root.note = ""; ut.signIn(email.text, pass.text) } }
    }
    RowLayout {
        visible: ut.syncSignedIn
        Layout.fillWidth: true
        spacing: 10
        Pill { Layout.fillWidth: true; text: "Cerrar sesión"; onClicked: ut.signOut() }
        Pill { Layout.fillWidth: true; text: "Sincronizar ahora"; strong: true; onClicked: ut.syncNow() }
    }
}
