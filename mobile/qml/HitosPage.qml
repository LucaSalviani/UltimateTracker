import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Hitos en tres plazos (como en la PC): hoy y la semana, el mes, y el camino a las 10.000 h.
Flickable {
    id: page
    property int tick: 0
    readonly property var m: { ut.revision; page.tick; return ut.milestones() }
    readonly property var cfg: m.cfg
    contentHeight: column.implicitHeight + 30
    boundsBehavior: Flickable.StopAtBounds
    Timer { interval: 30000; repeat: true; running: page.visible && ut.state === "running"; onTriggered: page.tick++ }

    function markColor(level) { return level === 3 ? Theme.gold : level === 2 ? Theme.silver : level === 1 ? Theme.bronze : Theme.faint }

    component Big: Text { color: Theme.text; font.pixelSize: 26; font.weight: Font.Bold }
    component Small: Text { color: Theme.dim; font.pixelSize: 12; wrapMode: Text.WordWrap }

    ColumnLayout {
        id: column
        x: Theme.margin
        width: page.width - 2 * Theme.margin
        spacing: 14

        Label { Layout.topMargin: 18; text: "Hitos"; color: Theme.text; font.pixelSize: 24; font.weight: Font.DemiBold }

        Text {
            visible: !cfg.day && !cfg.streaks && !cfg.weekly && !cfg.hours && !cfg.stages && !cfg.month
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: "Tenés todos los hitos apagados. Prendelos en Ajustes › Hitos."
            color: Theme.faint; font.pixelSize: 14
        }

        // ---- Largo plazo: nivel global
        Card {
            visible: cfg.stages
            Layout.fillWidth: true
            spacing: 10
            SectionLabel { text: "Camino a las 10.000 h" }
            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    spacing: 0
                    Big { text: m.global.label }
                    Small { text: m.global.hours + " en total" }
                }
                Item { Layout.fillWidth: true }
                ColumnLayout {
                    spacing: 0
                    Text { Layout.alignment: Qt.AlignRight; text: m.global.pct10k + " %"; color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold }
                    Small { Layout.alignment: Qt.AlignRight; text: "de las 10.000 h" }
                }
            }
            // Las 8 etapas
            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                Repeater {
                    model: m.stageNames
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 6
                        radius: 3
                        color: index < m.global.index ? Theme.accent : index === m.global.index ? Theme.accentSoft : Theme.bg
                    }
                }
            }
            Meter { Layout.fillWidth: true; value: m.global.progress }
            Small {
                Layout.fillWidth: true
                text: m.global.next ? ("Faltan " + m.global.left + " para el próximo nivel (" + m.global.next + ")") : "Llegaste a Maestro."
            }
        }

        // ---- Corto plazo: hoy
        Card {
            visible: cfg.day
            Layout.fillWidth: true
            spacing: 10
            SectionLabel { text: "Marca del día" }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Icon { name: "medal"; width: 34; height: 34; color: page.markColor(m.mark.level); stroke: 2 }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Text { text: m.mark.level > 0 ? m.mark.name : "Todavía sin marca"; color: m.mark.level > 0 ? page.markColor(m.mark.level) : Theme.dim; font.pixelSize: 20; font.weight: Font.DemiBold }
                    Small { text: m.mark.today + " hoy" + (m.mark.level < 3 ? " · faltan " + m.mark.left + " para " + m.mark.nextName.toLowerCase() : "") }
                }
            }
            Meter { visible: m.mark.level < 3; Layout.fillWidth: true; value: m.mark.progress; fill: page.markColor(m.mark.level + 1) }
            RowLayout {
                Layout.fillWidth: true
                Repeater {
                    model: ["Bronce", "Plata", "Oro"]
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Rectangle { width: 9; height: 9; radius: 4.5; color: m.mark.level > index ? page.markColor(index + 1) : "transparent"; border.color: page.markColor(index + 1) }
                        Text { text: modelData + " " + m.mark.marks[index]; color: m.mark.level > index ? Theme.text : Theme.dim; font.pixelSize: 12 }
                    }
                }
            }
        }

        // ---- Rachas y récords
        RowLayout {
            visible: cfg.streaks
            Layout.fillWidth: true
            spacing: 12
            Card {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                SectionLabel { text: "Racha" }
                RowLayout {
                    spacing: 8
                    Icon { name: "flame"; width: 24; height: 24; color: m.streak.days > 0 ? Theme.accent : Theme.faint }
                    Big { text: m.streak.days + (m.streak.days === 1 ? " día" : " días") }
                }
                Small {
                    Layout.fillWidth: true
                    text: (m.streak.todayDone ? "Hoy ya sumaste." : "Hoy todavía no estudiaste.") +
                          (m.streak.jokers > 0 ? " Comodines: " + m.streak.jokersLeft + " de " + m.streak.jokers + " esta semana." : "") +
                          (m.streak.next ? " Próxima: " + m.streak.next + "." : "")
                }
            }
            Card {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                SectionLabel { text: "Récords" }
                Text { text: m.records.bestDay ? m.records.bestDay : "—"; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
                Small { text: m.records.bestDay ? "mejor día · " + m.records.bestDayDate : "mejor día" }
                Text { text: m.records.bestWeek ? m.records.bestWeek : "—"; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
                Small { text: "mejor semana" }
                Small { text: "racha más larga: " + m.streak.best + (m.streak.best === 1 ? " día" : " días") }
            }
        }

        // ---- Meta semanal
        Card {
            visible: cfg.weekly
            Layout.fillWidth: true
            spacing: 10
            SectionLabel { text: "Meta semanal" }
            RowLayout {
                Layout.fillWidth: true
                Big { text: m.week.done }
                Text { text: "de " + m.week.goal; color: Theme.dim; font.pixelSize: 15; Layout.alignment: Qt.AlignBottom; Layout.bottomMargin: 4 }
                Item { Layout.fillWidth: true }
                Text { visible: m.week.progress >= 1; text: "¡Cumplida!"; color: Theme.good; font.pixelSize: 14; font.weight: Font.DemiBold }
            }
            Meter { Layout.fillWidth: true; value: m.week.progress; fill: m.week.progress >= 1 ? Theme.good : Theme.accent }
            Small {
                Layout.fillWidth: true
                text: (m.week.progress < 1 ? "Faltan " + m.week.left + " · quedan " + m.week.daysLeft + (m.week.daysLeft === 1 ? " día" : " días") + "." : "") +
                      (m.week.inARow > 0 ? " " + m.week.inARow + (m.week.inARow === 1 ? " semana cumpliendo la meta." : " semanas seguidas cumpliendo la meta.") : "")
            }
        }

        // ---- Mediano plazo: el mes
        Card {
            visible: cfg.month
            Layout.fillWidth: true
            spacing: 6
            SectionLabel { text: m.month.name }
            RowLayout {
                Layout.fillWidth: true
                Big { text: m.month.total }
                Item { Layout.fillWidth: true }
                Text {
                    visible: m.month.hasPrev
                    text: (m.month.delta >= 0 ? "+" : "") + m.month.delta + " %"
                    color: m.month.delta >= 0 ? Theme.good : Theme.accent
                    font.pixelSize: 17; font.weight: Font.DemiBold
                }
            }
            Small {
                Layout.fillWidth: true
                text: "En " + m.month.days + (m.month.days === 1 ? " día" : " días") + "." +
                      (m.month.hasPrev ? " El mes pasado a esta altura llevabas " + m.month.prevSamePoint + "." : "")
            }
        }

        // ---- Por actividad
        SectionLabel { visible: (cfg.stages || cfg.hours) && m.acts.length > 0; text: "Por actividad"; Layout.topMargin: 4 }
        Repeater {
            model: (cfg.stages || cfg.hours) ? m.acts : []
            delegate: Card {
                Layout.fillWidth: true
                Layout.leftMargin: modelData.depth * 16
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Rectangle { width: 10; height: 10; radius: 5; color: modelData.color }
                    Text { Layout.fillWidth: true; text: modelData.name; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold; elide: Text.ElideRight }
                    Text { text: modelData.hours; color: Theme.text; font.pixelSize: 14 }
                }
                RowLayout {
                    visible: cfg.stages
                    Layout.fillWidth: true
                    Text { text: modelData.label; color: Theme.dim; font.pixelSize: 13 }
                    Item { Layout.fillWidth: true }
                    Text { text: modelData.next ? "próximo nivel a las " + modelData.next : ""; color: Theme.faint; font.pixelSize: 12 }
                }
                Meter { visible: cfg.stages; Layout.fillWidth: true; value: modelData.progress; fill: modelData.color }
                Small { visible: cfg.hours && modelData.nextMark > 0; text: "Próxima marca: " + modelData.nextMark + " h" }
            }
        }
    }
}
