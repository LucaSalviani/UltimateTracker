import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Templates as T

// El objetivo del día (como el panel de la PC): por pomodoros o por tiempo,
// cada grupo de pomodoros puede ser de una actividad, y se puede repetir.
Sheet {
    id: root
    title: "Objetivo de hoy"
    subtitle: ut.dateLabel
    property bool has: false
    property bool timeMode: false
    property int totalMin: 240
    property var parts: []
    property int weekdays: 0
    property int pickFor: -1

    function load() {
        const d = ut.goalDraft()
        has = d.has; timeMode = d.timeMode; totalMin = d.totalMin; weekdays = d.weekdays
        parts = d.parts
        titleField.text = ut.dayTitle
    }
    onAboutToShow: load()
    onClosed: if (titleField.text.trim() !== ut.dayTitle) ut.setDayTitle(titleField.text)

    function setPart(i, key, v) {
        const p = parts.slice(); const o = Object.assign({}, p[i]); o[key] = v; p[i] = o; parts = p
    }
    function step(m, d) { return d > 0 ? (m >= 10 ? Math.floor(m / 5) * 5 + 5 : m + 1) : (m > 10 ? Math.ceil(m / 5) * 5 - 5 : Math.max(1, m - 1)) }
    function folderName(id) {
        if (!id) return "Cualquiera"
        for (const f of ut.folders) if (f.id == id) return f.name
        return "Cualquiera"
    }
    function folderColor(id) {
        for (const f of ut.folders) if (f.id == id) return f.color
        return "transparent"
    }

    TextField {
        id: titleField
        Layout.fillWidth: true
        placeholderText: "¿Qué querés lograr hoy? (opcional)"
        color: Theme.text
        placeholderTextColor: Theme.faint
        font.pixelSize: 16
        background: Rectangle { implicitHeight: 48; color: Theme.bg; radius: 12; border.color: titleField.activeFocus ? Theme.faint : Theme.line }
        onAccepted: { ut.setDayTitle(text); focus = false }
    }

    Segmented {
        Layout.fillWidth: true
        options: [{key: "p", label: "Por pomodoros"}, {key: "t", label: "Por tiempo"}]
        current: root.timeMode ? "t" : "p"
        onPicked: (k) => root.timeMode = k === "t"
    }

    // ---- Por pomodoros: grupos "4 × 25m" con su actividad
    Repeater {
        model: root.timeMode ? [] : root.parts
        delegate: Card {
            Layout.fillWidth: true
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                component Mini: Rectangle {
                    property string icon: "plus"
                    signal tapped()
                    width: 34; height: 34; radius: 17
                    color: mm.pressed ? Theme.surfaceHi : Theme.bg
                    border.color: Theme.line
                    Icon { anchors.centerIn: parent; name: parent.icon; width: 15; height: 15 }
                    MouseArea { id: mm; anchors.fill: parent; onClicked: parent.tapped(); onPressAndHold: r.start(); onReleased: r.stop(); onCanceled: r.stop() }
                    Timer { id: r; interval: 120; repeat: true; onTriggered: parent.tapped() }
                }
                Mini { icon: "minus"; onTapped: root.setPart(index, "count", Math.max(1, modelData.count - 1)) }
                Text { text: modelData.count; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold; Layout.minimumWidth: 22; horizontalAlignment: Text.AlignHCenter }
                Mini { icon: "plus"; onTapped: root.setPart(index, "count", Math.min(50, modelData.count + 1)) }
                Text { text: "×"; color: Theme.dim; font.pixelSize: 17; Layout.leftMargin: 2; Layout.rightMargin: 2 }
                Mini { icon: "minus"; onTapped: root.setPart(index, "minutes", root.step(modelData.minutes, -1)) }
                Text { text: modelData.minutes + "m"; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold; Layout.minimumWidth: 44; horizontalAlignment: Text.AlignHCenter }
                Mini { icon: "plus"; onTapped: root.setPart(index, "minutes", Math.min(240, root.step(modelData.minutes, 1))) }
                Item { Layout.fillWidth: true }
                Rectangle {
                    visible: root.parts.length > 1
                    width: 34; height: 34; radius: 17
                    color: dm.pressed ? Theme.surfaceHi : "transparent"
                    Icon { anchors.centerIn: parent; name: "trash"; width: 18; height: 18; color: Theme.dim }
                    MouseArea { id: dm; anchors.fill: parent; onClicked: { const p = root.parts.slice(); p.splice(index, 1); root.parts = p } }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 38
                radius: 10
                color: am.pressed ? Theme.surfaceHi : Theme.bg
                border.color: Theme.line
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 10
                    spacing: 10
                    Rectangle { width: 9; height: 9; radius: 4.5; color: root.folderColor(modelData.folder); border.color: modelData.folder ? "transparent" : Theme.faint }
                    Text { text: root.folderName(modelData.folder); color: modelData.folder ? Theme.text : Theme.dim; font.pixelSize: 14; Layout.fillWidth: true; elide: Text.ElideRight }
                    Icon { name: "chevron-down"; width: 14; height: 14; color: Theme.dim }
                }
                MouseArea { id: am; anchors.fill: parent; onClicked: { root.pickFor = index; picker.current = modelData.folder; picker.open() } }
            }
        }
    }
    Pill {
        visible: !root.timeMode
        Layout.alignment: Qt.AlignLeft
        text: "Agregar otro grupo"
        icon: "plus"
        onClicked: { const p = root.parts.slice(); p.push({count: 2, minutes: ut.workMin, folder: 0}); root.parts = p }
    }

    // ---- Por tiempo
    Card {
        visible: root.timeMode
        Layout.fillWidth: true
        StepRow {
            Layout.fillWidth: true
            text: "Tiempo de estudio"
            detail: "Los cuadraditos se arman con tu duración de pomodoro (" + ut.workMin + "m)"
            valueText: Math.floor(root.totalMin / 60) + "h" + (root.totalMin % 60 ? (root.totalMin % 60 < 10 ? "0" : "") + root.totalMin % 60 : "")
            onMinus: root.totalMin = Math.max(15, root.totalMin - (root.totalMin > 60 ? 30 : 15))
            onPlus: root.totalMin = Math.min(16 * 60, root.totalMin + (root.totalMin >= 60 ? 30 : 15))
        }
    }

    Text {
        Layout.fillWidth: true
        text: "Total: " + ut.describeParts(root.timeMode, root.totalMin, root.parts)
        color: Theme.dim; font.pixelSize: 13
    }

    SectionLabel { text: "Repetir"; Layout.topMargin: 4 }
    Segmented {
        Layout.fillWidth: true
        options: [{key: "0", label: "Solo hoy"}, {key: "31", label: "Lun a vie"}, {key: "127", label: "Todos los días"}]
        current: String(root.weekdays)
        onPicked: (k) => root.weekdays = parseInt(k)
    }
    // Días sueltos (como en la PC)
    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        Repeater {
            model: ["L", "M", "M", "J", "V", "S", "D"]
            delegate: Rectangle {
                Layout.fillWidth: true
                implicitHeight: 34
                radius: 17
                readonly property bool on: (root.weekdays & (1 << index)) !== 0
                color: on ? Theme.surfaceHi : "transparent"
                border.color: on ? Theme.faint : Theme.line
                Text { anchors.centerIn: parent; text: modelData; color: parent.on ? Theme.text : Theme.dim; font.pixelSize: 13; font.weight: Font.DemiBold }
                MouseArea { anchors.fill: parent; onClicked: root.weekdays = root.weekdays ^ (1 << index) }
            }
        }
    }
    Text {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.faint; font.pixelSize: 12
        text: root.weekdays === 0 ? "Solo para hoy." : "Se repite desde hoy los días marcados. Los días anteriores no cambian."
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 6
        spacing: 10
        Pill {
            visible: root.has
            text: "Quitar"
            danger: true
            onClicked: { if (ut.goalDraft().weekdays !== 0) removeMenu.open(); else { ut.removeGoal(false); root.close() } }
        }
        Item { Layout.fillWidth: true }
        Pill {
            text: "Guardar"
            strong: true
            onClicked: {
                ut.setDayTitle(titleField.text)
                ut.saveGoal(root.timeMode, root.totalMin, root.parts, root.weekdays)
                root.close()
            }
        }
    }
    ColumnLayout {
        id: removeMenu
        visible: false
        function open() { visible = true }
        Layout.fillWidth: true
        spacing: 8
        Text { text: "Este objetivo se repite. ¿Qué querés quitar?"; color: Theme.dim; font.pixelSize: 13 }
        Pill { Layout.fillWidth: true; text: "Solo el de hoy"; onClicked: { ut.removeGoal(false); removeMenu.visible = false; root.close() } }
        Pill { Layout.fillWidth: true; text: "De hoy en adelante"; danger: true; onClicked: { ut.removeGoal(true); removeMenu.visible = false; root.close() } }
    }

    ActivitySheet {
        id: picker
        parent: T.Overlay.overlay
        allowAny: true
        allowAdd: false
        onChosen: (id) => { root.setPart(root.pickFor, "folder", id); picker.close() }
    }
}
