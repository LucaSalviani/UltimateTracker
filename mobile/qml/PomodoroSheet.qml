import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Templates as T

// Tocar un cuadradito: en qué trabajaste (nota), cambiarle la actividad o borrarlo.
Sheet {
    id: root
    property var pid: 0
    property var info: ({})
    property bool confirmDelete: false
    title: info.running ? "Pomodoro en curso" : "Pomodoro"
    subtitle: (info.day || "") + " · " + (info.when || "") + " · " + (info.studied || "")
    function show(id) { pid = id; info = ut.pomodoroInfo(id); note.text = info.note || ""; confirmDelete = false; open() }
    onClosed: if (info.id && note.text.trim() !== (info.note || "")) ut.setNote(pid, note.text)

    SectionLabel { text: "Actividad" }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 46
        radius: 12
        color: am.pressed ? Theme.surfaceHi : Theme.surface
        border.color: Theme.line
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 12
            spacing: 10
            Rectangle { width: 10; height: 10; radius: 5; color: root.info.color || "transparent" }
            Text { text: root.info.folderName || ""; color: Theme.text; font.pixelSize: 15; Layout.fillWidth: true; elide: Text.ElideRight }
            Text { text: "Cambiar"; color: Theme.dim; font.pixelSize: 13 }
        }
        MouseArea { id: am; anchors.fill: parent; onClicked: { picker.current = root.info.folder; picker.open() } }
    }

    // Mejoras 2.0: los otros de la misma actividad ese día, de una
    Text {
        readonly property var others: root.pid ? ut.sameActivityThatDay(root.pid) : []
        visible: others.length > 1 && !root.info.running
        Layout.fillWidth: true
        text: "Mover los " + others.length + " de esta actividad de ese día…"
        color: Theme.accent; font.pixelSize: 13
        MouseArea { anchors.fill: parent; onClicked: { pickerAll.current = root.info.folder; pickerAll.open() } }
    }

    SectionLabel { text: "¿En qué trabajaste?"; Layout.topMargin: 4 }
    TextArea {
        id: note
        Layout.fillWidth: true
        Layout.minimumHeight: 90
        wrapMode: TextEdit.Wrap
        placeholderText: "Ej.: ejercicios 3 a 7 de la guía"
        color: Theme.text
        placeholderTextColor: Theme.faint
        font.pixelSize: 15
        background: Rectangle { color: Theme.bg; radius: 12; border.color: note.activeFocus ? Theme.faint : Theme.line }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 4
        spacing: 10
        Pill {
            text: root.confirmDelete ? (root.info.running ? "Sí, descartarlo" : "Sí, borrarlo") : (root.info.running ? "Descartar" : "Borrar")
            icon: "trash"
            danger: true
            onClicked: {
                if (!root.confirmDelete) { root.confirmDelete = true; return }
                root.info = ({})
                ut.deletePomodoro(root.pid)
                root.close()
            }
        }
        Item { Layout.fillWidth: true }
        Pill { text: "Listo"; strong: true; onClicked: root.close() }
    }

    ActivitySheet {
        id: pickerAll
        parent: T.Overlay.overlay
        allowAdd: false
        title: "Mover todos a…"
        onChosen: (id) => {
            if (id > 0) { ut.movePomodoros(ut.sameActivityThatDay(root.pid), id); root.info = ut.pomodoroInfo(root.pid) }
            pickerAll.close()
        }
    }
    ActivitySheet {
        id: picker
        parent: T.Overlay.overlay
        allowAdd: false
        onChosen: (id) => {
            if (id > 0) { ut.movePomodoro(root.pid, id); root.info = ut.pomodoroInfo(root.pid) }
            picker.close()
        }
    }
}
