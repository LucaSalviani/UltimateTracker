import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Templates as T

// Hoy: el reloj, el objetivo y los cuadraditos.
Flickable {
    id: page
    signal openGoal()
    signal openActivity()
    signal openSync()
    signal openPomodoro(var id)
    signal openTitle()
    contentHeight: column.implicitHeight + 30
    boundsBehavior: Flickable.StopAtBounds

    readonly property string st: ut.state
    readonly property bool isBreak: st === "break" || st === "breakover"
    readonly property color modeColor: isBreak ? Theme.brk : Theme.accent
    function pulse() { pulseAnim.restart() }

    function kicker() {
        const n = (ut.pomodoroNumber < 10 ? "0" : "") + ut.pomodoroNumber
        switch (st) {
        case "running": return "POMODORO " + n
        case "paused": return "EN PAUSA"
        case "workdone": return "¡POMODORO TERMINADO!"
        case "break": return "RECREO"
        case "breakover": return "TERMINÓ EL RECREO"
        default: return "LISTO · POMODORO " + n
        }
    }

    ColumnLayout {
        id: column
        width: page.width
        spacing: 16

        // ---- Cabecera: el día y lo que llevás
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 18
            Layout.leftMargin: Theme.margin
            Layout.rightMargin: Theme.margin
            ColumnLayout {
                spacing: 2
                Label { text: "Hoy"; color: Theme.text; font.pixelSize: 24; font.weight: Font.DemiBold }
                Label { text: ut.dateLabel; color: Theme.dim; font.pixelSize: 13 }
            }
            Item { Layout.fillWidth: true }
            Rectangle {   // nube: cuenta y sincronización
                implicitWidth: 42; implicitHeight: 42; radius: 21
                color: cloudMouse.pressed ? Theme.surfaceHi : "transparent"
                Icon { anchors.centerIn: parent; width: 26; height: 26; name: ut.syncSignedIn ? "cloud-ok" : "cloud"; color: ut.syncSignedIn ? Theme.text : Theme.faint }
                MouseArea { id: cloudMouse; anchors.fill: parent; onClicked: page.openSync() }
            }
            ColumnLayout {
                spacing: 2
                Label { Layout.alignment: Qt.AlignRight; text: ut.todayStudied; color: Theme.text; font.pixelSize: 24; font.weight: Font.Bold }
                Label { Layout.alignment: Qt.AlignRight; text: ut.todayCount + (ut.todayCount === 1 ? " pomodoro" : " pomodoros"); color: Theme.dim; font.pixelSize: 13 }
            }
        }

        // ---- ¿Qué querés lograr hoy? + objetivo
        Card {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.margin
            Layout.rightMargin: Theme.margin
            pressable: true
            spacing: 10
            onClicked: page.openGoal()
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Icon { name: "target"; width: 20; height: 20; color: ut.goal.complete ? Theme.good : ut.goal.has ? Theme.accent : Theme.dim }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Text {
                        Layout.fillWidth: true
                        text: ut.dayTitle.length > 0 ? ut.dayTitle : (ut.goal.has ? "Objetivo de hoy" : "¿Qué querés lograr hoy?")
                        color: ut.dayTitle.length > 0 || ut.goal.has ? Theme.text : Theme.dim
                        font.pixelSize: 15; font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: ut.goal.has ? (ut.goal.describe + (ut.goal.activities ? " · " + ut.goal.activities : "") + (ut.goal.repeats ? " · se repite" : ""))
                                          : "Tocá para poner un objetivo"
                        color: Theme.dim; font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
                Text {
                    visible: ut.goal.has === true
                    text: ut.goal.complete ? "¡Cumplido!" : ut.goal.studied + " / " + ut.goal.target + (ut.goal.withBreaks ? "\n≈ " + ut.goal.withBreaks + " c/ recreos" : "")
                    horizontalAlignment: Text.AlignRight
                    color: ut.goal.complete ? Theme.good : Theme.text
                    font.pixelSize: 13; font.weight: Font.DemiBold
                }
                Icon { name: "chevron-right"; width: 16; height: 16; color: Theme.faint }
            }
            Meter {
                visible: ut.goal.has === true
                Layout.fillWidth: true
                value: ut.goal.progress || 0
                fill: ut.goal.complete ? Theme.good : Theme.accent
            }
        }

        // ---- Actividad
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: Math.min(page.width - 40, actRow.implicitWidth + 32)
            implicitHeight: 40
            radius: 20
            color: actMouse.pressed ? Theme.surfaceHi : Theme.surface
            border.color: Theme.line
            RowLayout {
                id: actRow
                anchors.centerIn: parent
                width: Math.min(implicitWidth, parent.width - 32)
                spacing: 10
                Rectangle { width: 10; height: 10; radius: 5; color: ut.folderColor }
                Label { text: ut.folderName; color: Theme.text; font.pixelSize: 15; font.weight: Font.Medium; elide: Text.ElideRight; Layout.maximumWidth: page.width - 130 }
                Icon { name: "chevron-down"; width: 16; height: 16; color: Theme.dim }
            }
            MouseArea { id: actMouse; anchors.fill: parent; onClicked: page.openActivity() }
        }

        // ---- En vivo: lo maneja la PC (tocar cualquier botón lo toma el celu)
        Rectangle {
            visible: ut.liveFrom.length > 0
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: liveRow.implicitWidth + 24
            implicitHeight: 28
            radius: 14
            color: "transparent"
            border.color: Theme.line
            RowLayout {
                id: liveRow
                anchors.centerIn: parent
                spacing: 8
                Rectangle {
                    width: 8; height: 8; radius: 4; color: Theme.brk
                    SequentialAnimation on opacity {
                        running: ut.liveFrom.length > 0; loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 900 }
                        NumberAnimation { to: 1; duration: 900 }
                    }
                }
                Text { text: "En vivo desde " + ut.liveFrom; color: Theme.dim; font.pixelSize: 12 }
            }
        }

        // ---- El reloj
        TimerRing {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.min(page.width - 80, 300)
            Layout.preferredHeight: Layout.preferredWidth
            progress: ut.progress
            ringColor: page.modeColor
            trackColor: Theme.surface
            clockText: ut.clock(ut.remainingSec)
            kicker: page.kicker()
            textColor: Theme.text
            kickerColor: st === "paused" ? Theme.dim : page.modeColor
            SequentialAnimation on scale {
                id: pulseAnim
                running: false
                NumberAnimation { to: 1.05; duration: 180; easing.type: Easing.OutCubic }
                NumberAnimation { to: 1.0; duration: 420; easing.type: Easing.OutBack }
            }
        }

        // ---- Duración (antes de arrancar) o recreo (al terminar)
        Stepper {
            Layout.alignment: Qt.AlignHCenter
            visible: st === "idle" || st === "ready" || st === "breakover"
            value: ut.workMin
            suffix: " min"
            label: "pomodoro"
            onMinus: ut.nudgeWork(-1)
            onPlus: ut.nudgeWork(1)
        }
        Stepper {
            Layout.alignment: Qt.AlignHCenter
            visible: st === "workdone"
            value: ut.breakMin
            suffix: " min"
            label: "recreo"
            onMinus: ut.nudgeBreak(-1)
            onPlus: ut.nudgeBreak(1)
        }

        // ---- Botones
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 18
            UtButton {
                visible: st === "running" || st === "paused" || st === "workdone"
                diameter: 58
                fillColor: Theme.surface
                lineColor: Theme.line
                iconColor: Theme.text
                icon: st === "workdone" ? "skip" : "stop"
                caption: st === "workdone" ? "Seguir sin recreo" : "Terminar"
                captionColor: Theme.dim
                onClicked: st === "workdone" ? ut.skipBreak() : ut.finishPomodoro()
            }
            UtButton {
                diameter: 84
                fillColor: page.modeColor
                lineColor: "transparent"
                iconColor: "#ffffff"
                icon: st === "running" ? "pause"
                    : st === "workdone" ? "coffee"
                    : st === "break" || st === "breakover" ? "stop" : "play"
                caption: st === "running" ? "Pausa"
                       : st === "paused" ? "Seguir"
                       : st === "workdone" ? "Empezar recreo"
                       : st === "break" ? "Terminar recreo"
                       : st === "breakover" ? "Listo" : "Empezar"
                captionColor: Theme.dim
                onClicked: {
                    if (st === "workdone") ut.startBreak()
                    else if (st === "break" || st === "breakover") ut.endBreak()
                    else ut.primary()
                }
            }
        }

        // ---- Me voy: termina la sesión (lo que sigue no es recreo indebido)
        Text {
            visible: st !== "idle" && st !== "running"
            Layout.alignment: Qt.AlignHCenter
            text: "Me voy · terminar la sesión"
            color: leaveMouse.pressed ? Theme.text : Theme.faint
            font.pixelSize: 13
            MouseArea { id: leaveMouse; anchors.fill: parent; anchors.margins: -10; onClicked: ut.endSession() }
        }

        // ---- Los cuadraditos de hoy
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.margin
            Layout.rightMargin: Theme.margin
            Layout.topMargin: 6
            SectionLabel { text: "Cuadraditos de hoy"; Layout.fillWidth: true }
            Text { visible: ut.goal.has === true; text: (ut.goal.full || 0) + " de " + (ut.goal.count || 0) + " del objetivo"; color: Theme.faint; font.pixelSize: 12 }
        }
        // Varios elegidos: moverlos juntos
        RowLayout {
            visible: tiles.selecting
            Layout.fillWidth: true
            Layout.leftMargin: Theme.margin
            Layout.rightMargin: Theme.margin
            spacing: 10
            Text { text: tiles.selected.length + (tiles.selected.length === 1 ? " elegido" : " elegidos"); color: Theme.text; font.pixelSize: 14; Layout.fillWidth: true }
            Pill { text: "Soltar"; implicitHeight: 38; onClicked: tiles.selected = [] }
            Pill { text: "Mover a…"; strong: true; implicitHeight: 38; onClicked: { movePicker.current = 0; movePicker.open() } }
        }
        Text {
            visible: !tiles.selecting && ut.todayCount > 1
            Layout.leftMargin: Theme.margin
            text: "Mantené apretado un cuadradito para elegir varios"
            color: Theme.faint; font.pixelSize: 11
        }
        Tiles {
            id: tiles
            Layout.fillWidth: true
            Layout.leftMargin: Theme.margin
            Layout.rightMargin: Theme.margin
            model: ut.today
            progress: ut.progress
            onTapped: (id) => page.openPomodoro(id)
        }
    }

    ActivitySheet {
        id: movePicker
        parent: T.Overlay.overlay
        allowAdd: false
        title: "Mover " + tiles.selected.length + " a…"
        onChosen: (id) => { ut.movePomodoros(tiles.selected, id); tiles.selected = []; movePicker.close() }
    }
}
