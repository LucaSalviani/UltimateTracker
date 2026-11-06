import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Templates as T

// Ajustes (los mismos que en la PC, los que tienen sentido en el celu).
Flickable {
    id: page
    signal openSync()
    signal openTutorial()
    function editFirst() { if (ut.folders.length > 0) editor.show(ut.folders[0].id) }
    contentHeight: column.implicitHeight + 30
    boundsBehavior: Flickable.StopAtBounds

    function val(key, def) { ut.revision; return ut.setting(key, def) }
    function num(key, def) { return Number(val(key, def)) }
    function on(key, def) { const v = val(key, def); return v === true || v === 1 || v === "1" || v === "true" }
    function hm(min) { const h = Math.floor(min / 60), m = min % 60; return h > 0 ? h + "h" + (m ? (m < 10 ? "0" : "") + m : "") : m + "m" }
    function step(m, d) { return d > 0 ? (m >= 10 ? Math.floor(m / 5) * 5 + 5 : m + 1) : (m > 10 ? Math.ceil(m / 5) * 5 - 5 : Math.max(1, m - 1)) }

    component Divider: Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.line }

    ColumnLayout {
        id: column
        x: Theme.margin
        width: page.width - 2 * Theme.margin
        spacing: 14

        Label { Layout.topMargin: 18; text: "Ajustes"; color: Theme.text; font.pixelSize: 24; font.weight: Font.DemiBold }

        // ---- Cuenta
        Card {
            Layout.fillWidth: true
            pressable: true
            onClicked: page.openSync()
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Icon { name: ut.syncSignedIn ? "cloud-ok" : "cloud"; width: 26; height: 26; color: ut.syncSignedIn ? Theme.text : Theme.dim }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Text { text: ut.syncSignedIn ? ut.syncEmail : "Sincronizar con la PC"; color: Theme.text; font.pixelSize: 15; Layout.fillWidth: true; elide: Text.ElideRight }
                    Text { text: ut.syncSignedIn ? ut.syncStatus : "Entrá con tu cuenta para juntar todo"; color: Theme.dim; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                }
                Icon { name: "chevron-right"; width: 16; height: 16; color: Theme.faint }
            }
        }

        // ---- Pomodoros
        SectionLabel { text: "Pomodoros"; Layout.topMargin: 4 }
        Card {
            Layout.fillWidth: true
            spacing: 4
            StepRow {
                Layout.fillWidth: true
                text: "Duración del pomodoro"
                valueText: ut.workMin + "m"
                onMinus: ut.nudgeWork(-1)
                onPlus: ut.nudgeWork(1)
            }
            Divider {}
            StepRow {
                Layout.fillWidth: true
                text: "Duración del recreo"
                valueText: ut.breakMin + "m"
                onMinus: ut.nudgeBreak(-1)
                onPlus: ut.nudgeBreak(1)
            }
            Divider {}
            Item {
                Layout.fillWidth: true
                implicitHeight: brkCol.implicitHeight + 12
                ColumnLayout {
                    id: brkCol
                    width: parent.width
                    spacing: 6
                    Text { text: "Recreos del objetivo («≈ con recreos»)"; color: Theme.text; font.pixelSize: 15; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Segmented {
                        Layout.fillWidth: true
                        options: [{key: "0", label: "Mi recreo"}, {key: "1", label: "Mi promedio real"}]
                        current: String(page.num("goal_break_source", 0))
                        onPicked: (k) => ut.setSetting("goal_break_source", parseInt(k))
                    }
                }
            }
            Divider {}
            Toggle {
                Layout.fillWidth: true
                text: "El pomodoro en curso ocupa un bloque del objetivo"
                detail: "Apagado: el bloque se llena recién a medida que estudiás."
                checked: page.num("goal_running", 0) === 0
                onToggled: (v) => ut.setSetting("goal_running", v ? 0 : 1)
            }
        }

        // ---- El día
        SectionLabel { text: "El día"; Layout.topMargin: 4 }
        Card {
            Layout.fillWidth: true
            spacing: 4
            StepRow {
                Layout.fillWidth: true
                text: "El día empieza a las"
                detail: "Lo que estudies antes de esa hora cuenta para el día anterior."
                valueText: (page.num("day_start_hour", 4) < 10 ? "0" : "") + page.num("day_start_hour", 4) + ":00"
                onMinus: ut.setSetting("day_start_hour", Math.max(0, page.num("day_start_hour", 4) - 1))
                onPlus: ut.setSetting("day_start_hour", Math.min(12, page.num("day_start_hour", 4) + 1))
            }
            Divider {}
            StepRow {
                Layout.fillWidth: true
                text: "Comodines por semana"
                detail: "Días sin estudiar que no cortan la racha."
                valueText: page.num("jokers_per_week", 2)
                onMinus: ut.setSetting("jokers_per_week", Math.max(0, page.num("jokers_per_week", 2) - 1))
                onPlus: ut.setSetting("jokers_per_week", Math.min(7, page.num("jokers_per_week", 2) + 1))
            }
        }

        // ---- Hitos
        SectionLabel { text: "Hitos"; Layout.topMargin: 4 }
        Card {
            Layout.fillWidth: true
            spacing: 2
            Repeater {
                model: [
                    {key: "ms_day", text: "Marca del día", detail: "Bronce, plata y oro según lo que estudiás en el día"},
                    {key: "ms_streaks", text: "Rachas y récords", detail: ""},
                    {key: "ms_weekly", text: "Meta semanal", detail: ""},
                    {key: "ms_hours", text: "Horas por actividad", detail: "Avisos al llegar a 1, 5, 10, 25, 50, 100 h…"},
                    {key: "ms_stages", text: "Etapas hacia las 10.000 h", detail: "De Novato a Maestro, con subniveles"},
                    {key: "ms_month", text: "Resumen del mes", detail: ""}
                ]
                delegate: Toggle {
                    Layout.fillWidth: true
                    text: modelData.text
                    detail: modelData.detail
                    checked: page.on(modelData.key, 1)
                    onToggled: (v) => ut.setSetting(modelData.key, v ? 1 : 0)
                }
            }
            Divider { Layout.topMargin: 4; Layout.bottomMargin: 4 }
            StepRow {
                Layout.fillWidth: true
                text: "Meta semanal"
                valueText: page.hm(page.num("ms_weekly_min", 1200))
                onMinus: ut.setSetting("ms_weekly_min", Math.max(60, page.num("ms_weekly_min", 1200) - 60))
                onPlus: ut.setSetting("ms_weekly_min", Math.min(100 * 60, page.num("ms_weekly_min", 1200) + 60))
            }
            Repeater {
                model: [{key: "ms_bronze_min", text: "Bronce", def: 120}, {key: "ms_silver_min", text: "Plata", def: 240}, {key: "ms_gold_min", text: "Oro", def: 360}]
                delegate: StepRow {
                    Layout.fillWidth: true
                    text: "Marca de " + modelData.text.toLowerCase()
                    valueText: page.hm(page.num(modelData.key, modelData.def))
                    onMinus: ut.setSetting(modelData.key, Math.max(30, page.num(modelData.key, modelData.def) - 30))
                    onPlus: ut.setSetting(modelData.key, Math.min(16 * 60, page.num(modelData.key, modelData.def) + 30))
                }
            }
        }

        // ---- Actividades
        SectionLabel { text: "Actividades"; Layout.topMargin: 4 }
        Card {
            Layout.fillWidth: true
            spacing: 0
            Repeater {
                model: ut.folders
                delegate: Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 46
                    radius: 10
                    color: fm.pressed ? Theme.surfaceHi : "transparent"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 4 + modelData.depth * 18
                        anchors.rightMargin: 4
                        spacing: 12
                        Rectangle { width: 10; height: 10; radius: 5; color: modelData.color }
                        Text { Layout.fillWidth: true; text: modelData.name; color: Theme.text; font.pixelSize: 15; elide: Text.ElideRight }
                        Icon { name: "pencil"; width: 16; height: 16; color: Theme.faint }
                    }
                    MouseArea { id: fm; anchors.fill: parent; onClicked: editor.show(modelData.id) }
                }
            }
            Text {
                visible: ut.folders.length === 0
                text: "Todavía no hay actividades."
                color: Theme.faint; font.pixelSize: 13
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                spacing: 8
                TextField {
                    id: newAct
                    Layout.fillWidth: true
                    placeholderText: "Nueva actividad"
                    color: Theme.text
                    placeholderTextColor: Theme.faint
                    font.pixelSize: 15
                    background: Rectangle { implicitHeight: 44; color: Theme.bg; radius: 10; border.color: newAct.activeFocus ? Theme.faint : Theme.line }
                    onAccepted: if (ut.addFolder(text) > 0) text = ""
                }
                Pill { text: "Agregar"; enabledLook: newAct.text.trim().length > 0; onClicked: if (ut.addFolder(newAct.text) > 0) newAct.text = "" }
            }
        }

        Pill { Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 8; text: "Ver tutorial"; onClicked: page.openTutorial() }
        Text {
            Layout.topMargin: 8
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: "UltimateTracker " + ut.versionText
            color: Theme.faint; font.pixelSize: 12
        }
    }

    ActivityEditor { id: editor; parent: T.Overlay.overlay }
}
