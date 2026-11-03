import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Templates as T

// Estadísticas: día, semana, mes y año; filtro por actividad (como en la PC).
Flickable {
    id: page
    signal openPomodoro(var id)
    property string period: "week"
    property int offset: 0
    property var folder: 0
    property int tick: 0
    readonly property var d: { ut.revision; page.tick; return ut.stats(period, offset, folder) }
    contentHeight: column.implicitHeight + 30
    boundsBehavior: Flickable.StopAtBounds
    Timer { interval: 30000; repeat: true; running: page.visible && ut.state === "running"; onTriggered: page.tick++ }

    component Stat: ColumnLayout {
        property string value: ""
        property string label: ""
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        spacing: 1
        Text { Layout.fillWidth: true; text: parent.value; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold; elide: Text.ElideRight }
        Text { Layout.fillWidth: true; text: parent.label; color: Theme.dim; font.pixelSize: 12; wrapMode: Text.WordWrap }
    }

    ColumnLayout {
        id: column
        x: Theme.margin
        width: page.width - 2 * Theme.margin
        spacing: 14

        Label { Layout.topMargin: 18; text: "Estadísticas"; color: Theme.text; font.pixelSize: 24; font.weight: Font.DemiBold }

        Segmented {
            Layout.fillWidth: true
            options: [{key: "day", label: "Día"}, {key: "week", label: "Semana"}, {key: "month", label: "Mes"}, {key: "year", label: "Año"}]
            current: page.period
            onPicked: (k) => { page.period = k; page.offset = 0 }
        }

        // ‹ período ›
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            component Nav: Rectangle {
                property string icon: "chevron-left"
                property bool active: true
                signal tapped()
                width: 40; height: 40; radius: 20
                color: nm.pressed ? Theme.surfaceHi : "transparent"
                opacity: active ? 1 : 0.25
                Icon { anchors.centerIn: parent; name: parent.icon; width: 20; height: 20; color: Theme.text }
                MouseArea { id: nm; anchors.fill: parent; enabled: parent.active; onClicked: parent.tapped() }
            }
            Nav { icon: "chevron-left"; onTapped: page.offset-- }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: page.d.label
                color: Theme.text; font.pixelSize: 16; font.weight: Font.DemiBold
                elide: Text.ElideRight
                MouseArea { anchors.fill: parent; onClicked: page.offset = 0 }
            }
            Nav { icon: "chevron-right"; active: page.offset < 0; onTapped: page.offset++ }
        }

        // Filtro por actividad
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: fr.implicitWidth + 28
            implicitHeight: 34
            radius: 17
            color: fm.pressed ? Theme.surfaceHi : "transparent"
            border.color: Theme.line
            RowLayout {
                id: fr
                anchors.centerIn: parent
                spacing: 8
                Icon { name: "folder"; width: 16; height: 16; color: Theme.dim }
                Text { text: page.folder ? page.d.folderName : "Todas las actividades"; color: Theme.text; font.pixelSize: 13 }
                Icon { visible: !page.folder; name: "chevron-down"; width: 14; height: 14; color: Theme.dim }
                Icon { visible: !!page.folder; name: "close"; width: 14; height: 14; color: Theme.dim }
            }
            MouseArea { id: fm; anchors.fill: parent; onClicked: { if (page.folder) page.folder = 0; else { picker.current = 0; picker.open() } } }
        }

        // Totales
        Card {
            Layout.fillWidth: true
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    spacing: 0
                    Text { text: page.d.total; color: Theme.text; font.pixelSize: 34; font.weight: Font.Bold }
                    Text { text: "estudiadas"; color: Theme.dim; font.pixelSize: 12 }
                }
                Item { Layout.fillWidth: true }
                ColumnLayout {
                    visible: page.d.goalDays > 0
                    spacing: 0
                    Text { Layout.alignment: Qt.AlignRight; text: page.d.goalMet + "/" + page.d.goalDays; color: page.d.goalMet === page.d.goalDays ? Theme.good : Theme.text; font.pixelSize: 20; font.weight: Font.DemiBold }
                    Text { Layout.alignment: Qt.AlignRight; text: "objetivos cumplidos"; color: Theme.dim; font.pixelSize: 12 }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Stat { value: page.d.pomodoros; label: page.d.pomodoros === 1 ? "pomodoro" : "pomodoros" }
                Stat { visible: page.period !== "day"; value: page.d.average; label: "promedio por día" }
                Stat { visible: page.period !== "day"; value: page.d.studiedDays + "/" + page.d.days; label: "días con estudio" }
                Stat { visible: page.period === "day"; value: page.d.breaks; label: "recreos" }
            }
            BarChart {
                visible: page.period !== "day"
                Layout.fillWidth: true
                Layout.topMargin: 4
                bars: page.d.bars
                maxSec: page.d.maxSec
                labelEvery: page.period === "month" ? 5 : 1
            }
        }

        // Día: objetivo y cuadraditos con sus notas
        Card {
            visible: page.period === "day" && (page.d.dayGoal.length > 0 || page.d.dayTitle.length > 0)
            Layout.fillWidth: true
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Icon { name: "target"; width: 18; height: 18; color: page.d.dayGoalState === 1 ? Theme.good : Theme.dim }
                Text { Layout.fillWidth: true; text: page.d.dayTitle.length > 0 ? page.d.dayTitle : "Objetivo"; color: Theme.text; font.pixelSize: 14; elide: Text.ElideRight }
                Text {
                    text: page.d.dayGoal + (page.d.dayGoalState === 1 ? " · cumplido" : page.d.dayGoalState === 2 ? " · no cumplido" : page.d.dayGoalState === 3 ? " · en curso" : "")
                    color: page.d.dayGoalState === 1 ? Theme.good : Theme.dim; font.pixelSize: 12
                }
            }
        }
        SectionLabel { visible: page.period === "day"; text: "Cuadraditos" }
        Text {
            visible: page.period === "day" && page.d.tiles.length === 0
            text: "Ese día no hay cuadraditos."
            color: Theme.faint; font.pixelSize: 13
        }
        Repeater {
            model: page.period === "day" ? page.d.tiles : []
            delegate: Rectangle {
                Layout.fillWidth: true
                implicitHeight: tr.implicitHeight + 20
                radius: 12
                color: tm.pressed ? Theme.surfaceHi : Theme.surface
                border.color: modelData.running ? Theme.accent : Theme.line
                RowLayout {
                    id: tr
                    x: 12; y: 10
                    width: parent.width - 24
                    spacing: 12
                    Rectangle {
                        width: 40; height: 40; radius: 8; color: "#0c0c0c"; border.color: Theme.line
                        Text { anchors.centerIn: parent; text: modelData.minutes + "m"; color: Theme.text; font.pixelSize: 12; font.weight: Font.DemiBold }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 3; color: modelData.color }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { Layout.fillWidth: true; text: "Nº" + modelData.number + " · " + modelData.name; color: Theme.text; font.pixelSize: 14; elide: Text.ElideRight }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.note.length > 0 ? modelData.note : modelData.when + " · " + modelData.studied
                            color: modelData.note.length > 0 ? Theme.dim : Theme.faint
                            font.pixelSize: 12; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight
                        }
                    }
                    Text { text: modelData.when; color: Theme.faint; font.pixelSize: 12; visible: modelData.note.length > 0 }
                }
                MouseArea { id: tm; anchors.fill: parent; onClicked: page.openPomodoro(modelData.id) }
            }
        }

        // Por actividad
        SectionLabel { visible: page.d.acts.length > 0; text: page.folder ? "Dentro de " + page.d.folderName : "Por actividad"; Layout.topMargin: 4 }
        Repeater {
            model: page.d.acts
            delegate: Item {
                Layout.fillWidth: true
                implicitHeight: 46
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Rectangle { width: 10; height: 10; radius: 5; color: modelData.color }
                        Text { Layout.fillWidth: true; text: modelData.name; color: Theme.text; font.pixelSize: 14; elide: Text.ElideRight }
                        Text { text: Math.round(modelData.share * 100) + "%"; color: Theme.faint; font.pixelSize: 12 }
                        Text { text: modelData.text; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                        Icon { visible: modelData.hasChildren; name: "chevron-right"; width: 14; height: 14; color: Theme.faint }
                    }
                    Meter { Layout.fillWidth: true; implicitHeight: 5; value: modelData.share; fill: modelData.color; color: Theme.surface }
                }
                MouseArea { anchors.fill: parent; enabled: modelData.id > 0 && modelData.id != page.folder; onClicked: page.folder = modelData.id }
            }
        }

        // Tiempo fuera del estudio
        Card {
            visible: page.period !== "day" || page.d.undue !== "0m"
            Layout.fillWidth: true
            Layout.topMargin: 4
            SectionLabel { text: "Fuera del estudio" }
            RowLayout {
                Layout.fillWidth: true
                Stat { value: page.d.breaks; label: "recreos" }
                Stat { value: page.d.undue; label: "recreo indebido" }
                Stat { value: page.d.inactive; label: "inactivo" }
            }
        }
    }

    ActivitySheet {
        id: picker
        parent: T.Overlay.overlay
        allowAdd: false
        allowAny: true
        title: "Filtrar por actividad"
        anyLabel: "Todas las actividades"
        onChosen: (id) => { page.folder = id; picker.close() }
    }
}
