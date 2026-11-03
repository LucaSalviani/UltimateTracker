import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: win
    visible: true
    width: 412
    height: 880
    title: "UltimateTracker"
    color: theme.bg

    QtObject {
        id: theme
        readonly property color bg: "#161616"
        readonly property color surface: "#222222"
        readonly property color surfaceHi: "#2c2c2c"
        readonly property color line: "#333333"
        readonly property color text: "#ececec"
        readonly property color dim: "#9a9a9a"
        readonly property color faint: "#5f5f5f"
        readonly property color accent: "#e0483f"
        readonly property color brk: "#3cb89b"
        readonly property string font: "sans-serif"
    }

    readonly property string st: ut.state
    readonly property bool isBreak: st === "break" || st === "breakover"
    readonly property color modeColor: isBreak ? theme.brk : theme.accent

    function kicker() {
        switch (st) {
        case "running": return "POMODORO " + (ut.pomodoroNumber < 10 ? "0" : "") + ut.pomodoroNumber
        case "paused": return "EN PAUSA"
        case "workdone": return "¡POMODORO TERMINADO!"
        case "break": return "RECREO"
        case "breakover": return "TERMINÓ EL RECREO"
        default: return "LISTO · POMODORO " + (ut.pomodoroNumber < 10 ? "0" : "") + ut.pomodoroNumber
        }
    }

    // Para revisar la interfaz sin un celu (capturas)
    Component.onCompleted: {
        const a = Qt.application.arguments
        if (a.indexOf("--demo-sync") >= 0) syncSheet.open()
        if (a.indexOf("--demo-acts") >= 0) sheet.open()
    }

    Connections {
        target: ut
        function onCompleted() { pulse.restart() }
        function onBreakOver() { pulse.restart() }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: column.implicitHeight + 40
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: column
            width: parent.width
            spacing: 18

            // ---- Cabecera: el día y lo que llevás
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 22
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                ColumnLayout {
                    spacing: 2
                    Label { text: "Hoy"; color: theme.text; font.pixelSize: 24; font.weight: Font.DemiBold }
                    Label { text: ut.dateLabel; color: theme.dim; font.pixelSize: 13 }
                }
                Item { Layout.fillWidth: true }
                // Nube: estado de la sincronización (toca para la cuenta)
                Rectangle {
                    implicitWidth: 38; implicitHeight: 38; radius: 19
                    color: cloudMouse.pressed ? theme.surfaceHi : "transparent"
                    Canvas {
                        id: cloud
                        anchors.centerIn: parent
                        width: 22; height: 16
                        property bool on: ut.syncSignedIn
                        onOnChanged: requestPaint()
                        onPaint: {
                            const c = getContext("2d"); c.reset()
                            c.strokeStyle = on ? theme.text : theme.faint; c.lineWidth = 1.8
                            c.beginPath()
                            c.moveTo(5, 15); c.lineTo(17, 15)
                            c.arc(17, 10, 5, Math.PI / 2, -Math.PI / 2, true)
                            c.arc(11, 7, 6, -0.2, Math.PI + 0.6, true)
                            c.arc(5, 11, 4, -Math.PI / 2, Math.PI / 2, true)
                            c.stroke()
                        }
                    }
                    MouseArea { id: cloudMouse; anchors.fill: parent; onClicked: syncSheet.open() }
                }
                ColumnLayout {
                    spacing: 2
                    Label { Layout.alignment: Qt.AlignRight; text: ut.todayStudied; color: theme.text; font.pixelSize: 24; font.weight: Font.Bold }
                    Label { Layout.alignment: Qt.AlignRight; text: ut.todayCount + (ut.todayCount === 1 ? " pomodoro" : " pomodoros"); color: theme.dim; font.pixelSize: 13 }
                }
            }

            // ---- Actividad
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: actRow.implicitWidth + 32
                implicitHeight: 40
                radius: 20
                color: actMouse.pressed ? theme.surfaceHi : theme.surface
                border.color: theme.line
                RowLayout {
                    id: actRow
                    anchors.centerIn: parent
                    spacing: 10
                    Rectangle { width: 10; height: 10; radius: 5; color: ut.folderColor }
                    Label { text: ut.folderName; color: theme.text; font.pixelSize: 15; font.weight: Font.Medium }
                    Label { text: "▾"; color: theme.dim; font.pixelSize: 13 }
                }
                MouseArea { id: actMouse; anchors.fill: parent; onClicked: sheet.open() }
            }

            // ---- El reloj
            TimerRing {
                id: ring
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.min(win.width - 60, 330)
                Layout.preferredHeight: Layout.preferredWidth
                progress: ut.progress
                ringColor: win.modeColor
                trackColor: theme.surface
                clockText: ut.clock(ut.remainingSec)
                kicker: win.kicker()
                textColor: theme.text
                kickerColor: st === "paused" ? theme.dim : win.modeColor
                SequentialAnimation on scale {
                    id: pulse
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
                textColor: theme.text; dimColor: theme.dim; buttonColor: theme.surface; lineColor: theme.line
            }
            Stepper {
                Layout.alignment: Qt.AlignHCenter
                visible: st === "workdone"
                value: ut.breakMin
                suffix: " min"
                label: "recreo"
                onMinus: ut.nudgeBreak(-1)
                onPlus: ut.nudgeBreak(1)
                textColor: theme.text; dimColor: theme.dim; buttonColor: theme.surface; lineColor: theme.line
            }

            // ---- Botones
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 18
                UtButton {
                    // secundario a la izquierda
                    visible: st === "running" || st === "paused" || st === "workdone"
                    diameter: 58
                    fillColor: theme.surface
                    lineColor: theme.line
                    iconColor: theme.text
                    icon: st === "workdone" ? "skip" : "stop"
                    caption: st === "workdone" ? "Seguir sin recreo" : "Terminar"
                    captionColor: theme.dim
                    onClicked: st === "workdone" ? ut.skipBreak() : ut.finishPomodoro()
                }
                UtButton {
                    diameter: 84
                    fillColor: win.modeColor
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
                    captionColor: theme.dim
                    onClicked: {
                        if (st === "workdone") ut.startBreak()
                        else if (st === "break" || st === "breakover") ut.endBreak()
                        else ut.primary()
                    }
                }
            }

            // ---- Los cuadraditos de hoy
            Label {
                Layout.leftMargin: 22
                Layout.topMargin: 8
                text: "CUADRADITOS DE HOY"
                color: theme.dim
                font.pixelSize: 11
                font.weight: Font.DemiBold
                font.letterSpacing: 1.2
            }
            Tiles {
                Layout.fillWidth: true
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                model: ut.today
                progress: ut.progress
                accent: theme.accent
                surface: theme.surface
                line: theme.line
                textColor: theme.text
                faint: theme.faint
            }
        }
    }

    SyncSheet {
        id: syncSheet
        bg: theme.surface
        textColor: theme.text
        dimColor: theme.dim
        lineColor: theme.line
        accent: theme.accent
    }

    ActivitySheet {
        id: sheet
        folders: ut.folders
        current: ut.folderId
        bg: theme.surface
        textColor: theme.text
        dimColor: theme.dim
        lineColor: theme.line
        onChosen: (id) => { ut.chooseFolder(id); sheet.close() }
        onAdded: (name) => { const id = ut.addFolder(name); if (id > 0) { ut.chooseFolder(id); sheet.close() } }
    }
}
