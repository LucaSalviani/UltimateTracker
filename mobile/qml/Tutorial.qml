import QtQuick
import QtQuick.Layouts

// Mejoras 2.0: mini tutorial del celu (la primera vez y desde Ajustes).
// Cada paso puede llevarte a la pestaña de la que habla.
Item {
    id: root
    property int index: 0
    signal goTab(int tab)
    signal done()
    visible: false
    anchors.fill: parent
    z: 300
    readonly property var steps: [
        {tab: 0, title: "Bienvenido a UltimateTracker", text: "Cuenta tus horas de estudio con pomodoros, como el contador de horas de un juego. La meta: las 10.000 horas de un experto."},
        {tab: 0, title: "La actividad y el reloj", text: "Arriba del reloj elegís en qué trabajás. Ajustá la duración con − / + y dale play. Al terminar vibra y elegís recreo o seguir."},
        {tab: 0, title: "El objetivo y los cuadraditos", text: "Tocá la tarjeta del objetivo para poner cuánto querés estudiar hoy. Cada pomodoro es un cuadradito; tocalo para la nota o mantené apretado para elegir varios."},
        {tab: 1, title: "Estadísticas", text: "Día, semana, mes y año, con filtro por actividad. Las barras verdes son días con el objetivo cumplido."},
        {tab: 2, title: "Hitos", text: "Las etapas hacia las 10.000 h, la marca del día, la racha, los récords y la meta semanal."},
        {tab: 3, title: "Ajustes y la PC", text: "Entrá con tu cuenta para juntar todo con la PC: el reloj es uno solo en los dos aparatos. Este tutorial se puede volver a ver desde acá."}
    ]
    function start() { index = 0; visible = true; goTab(steps[0].tab) }
    function finish() { visible = false; ut.setSetting("tutorial/done", 1); done() }

    Rectangle { anchors.fill: parent; color: "#000000"; opacity: 0.6 }
    MouseArea { anchors.fill: parent }   // no se toca lo de abajo
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 84
        width: parent.width - 32
        height: col.implicitHeight + 36
        radius: 18
        color: Theme.surface
        border.color: Theme.line
        ColumnLayout {
            id: col
            x: 18; y: 18
            width: parent.width - 36
            spacing: 8
            Text { text: (root.index + 1) + " de " + root.steps.length; color: Theme.faint; font.pixelSize: 11; font.letterSpacing: 1 }
            Text { text: root.steps[root.index].title; color: Theme.text; font.pixelSize: 19; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Text { text: root.steps[root.index].text; color: Theme.dim; font.pixelSize: 14; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 6
                spacing: 8
                Text {
                    text: "Saltar"; color: Theme.faint; font.pixelSize: 14
                    MouseArea { anchors.fill: parent; anchors.margins: -10; onClicked: root.finish() }
                }
                Item { Layout.fillWidth: true }
                Pill { visible: root.index > 0; text: "Anterior"; implicitHeight: 40; onClicked: { root.index--; root.goTab(root.steps[root.index].tab) } }
                Pill {
                    text: root.index + 1 >= root.steps.length ? "Listo" : "Siguiente"
                    strong: true; implicitHeight: 40
                    onClicked: {
                        if (root.index + 1 >= root.steps.length) { root.finish(); return }
                        root.index++; root.goTab(root.steps[root.index].tab)
                    }
                }
            }
        }
    }
}
