import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Editar una actividad: nombre, color, horas previas (las de antes de la app), subactividad o borrar.
Sheet {
    id: root
    property var fid: 0
    property var info: ({})
    property bool confirmDelete: false
    title: info.path || "Actividad"
    subtitle: (info.total || "0m") + " en total"
    function show(id) {
        fid = id; info = ut.folderInfo(id); name.text = info.name || ""
        prior = Math.round((info.prior || 0) * 2) / 2
        confirmDelete = false; sub.text = ""
        open()
    }
    property real prior: 0
    onClosed: {
        if (!info.id) return
        if (name.text.trim().length > 0 && name.text.trim() !== info.name) ut.renameFolder(fid, name.text)
        if (Math.abs(prior - (info.prior || 0)) > 0.01) ut.setFolderPriorHours(fid, prior)
    }

    SectionLabel { text: "Nombre" }
    TextField {
        id: name
        Layout.fillWidth: true
        color: Theme.text
        font.pixelSize: 16
        background: Rectangle { implicitHeight: 46; color: Theme.bg; radius: 12; border.color: name.activeFocus ? Theme.faint : Theme.line }
    }

    SectionLabel { text: "Color"; Layout.topMargin: 4 }
    Flow {
        Layout.fillWidth: true
        spacing: 10
        Repeater {
            model: ut.palette()
            delegate: Rectangle {
                width: 34; height: 34; radius: 17
                color: modelData
                border.width: String(root.info.color) === String(modelData) ? 3 : 0
                border.color: Theme.text
                MouseArea { anchors.fill: parent; onClicked: { ut.setFolderColor(root.fid, modelData); root.info = ut.folderInfo(root.fid) } }
            }
        }
    }

    Card {
        Layout.fillWidth: true
        Layout.topMargin: 4
        StepRow {
            Layout.fillWidth: true
            text: "Horas previas"
            detail: "Lo que ya habías estudiado antes de usar la app."
            valueText: (root.prior % 1 === 0 ? root.prior : root.prior.toFixed(1)) + " h"
            onMinus: root.prior = Math.max(0, root.prior - (root.prior > 20 ? 5 : 0.5))
            onPlus: root.prior = root.prior + (root.prior >= 20 ? 5 : 0.5)
        }
    }

    SectionLabel { text: "Subactividad"; Layout.topMargin: 4 }
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        TextField {
            id: sub
            Layout.fillWidth: true
            placeholderText: "Ej.: Parcial 1"
            color: Theme.text
            placeholderTextColor: Theme.faint
            font.pixelSize: 15
            background: Rectangle { implicitHeight: 44; color: Theme.bg; radius: 10; border.color: sub.activeFocus ? Theme.faint : Theme.line }
            onAccepted: if (ut.addFolder(text, root.fid) > 0) text = ""
        }
        Pill { text: "Agregar"; enabledLook: sub.text.trim().length > 0; onClicked: if (ut.addFolder(sub.text, root.fid) > 0) sub.text = "" }
    }

    Text {
        visible: root.confirmDelete
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.dim; font.pixelSize: 13
        text: "Se borra la actividad" + (root.info.children > 0 ? " y sus subactividades" : "") + ". Sus pomodoros quedan «sin actividad» y siguen contando en las horas totales."
    }
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 6
        Pill {
            text: root.confirmDelete ? "Sí, borrarla" : "Borrar"
            icon: "trash"
            danger: true
            onClicked: {
                if (!root.confirmDelete) { root.confirmDelete = true; return }
                if (ut.deleteFolder(root.fid)) { root.info = ({}); root.close() }
            }
        }
        Item { Layout.fillWidth: true }
        Pill { text: "Listo"; strong: true; onClicked: root.close() }
    }
}
