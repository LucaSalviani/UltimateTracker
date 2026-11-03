import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: win
    visible: true
    width: 412
    height: 880
    title: "UltimateTracker"
    color: Theme.bg
    property int tab: 0

    // Para revisar la interfaz sin un celu (capturas)
    Component.onCompleted: {
        const a = Qt.application.arguments
        for (let i = 0; i < a.length; ++i) {
            if (a[i].startsWith("--tab=")) tab = parseInt(a[i].substring(6))
            if (a[i].startsWith("--period=")) statsPage.period = a[i].substring(9)
            if (a[i].startsWith("--scroll=")) scrollTimer.y = parseInt(a[i].substring(9))
        }
        if (a.indexOf("--demo-sync") >= 0) syncSheet.open()
        if (a.indexOf("--demo-acts") >= 0) actSheet.open()
        if (a.indexOf("--demo-goal") >= 0) goalSheet.open()
        if (a.indexOf("--demo-pomo") >= 0 && ut.today.length > 0) pomoSheet.show(ut.today[0].id)
        if (a.indexOf("--demo-edit") >= 0 && ut.folders.length > 0) { tab = 3; settingsPage.editFirst() }
        if (a.indexOf("--demo-celebrate") >= 0) celebration.show("Marca del día", "Plata", "4h 10m hoy")
        if (a.indexOf("--demo-toast") >= 0) toast.show("Cuadradito borrado", true)
    }

    Timer {
        id: scrollTimer
        property int y: 0
        interval: 300; running: y > 0
        onTriggered: [todayPage, statsPage, hitosPage, settingsPage][win.tab].contentY = y
    }

    // Atrás de Android: primero vuelve a «Hoy», después sale.
    onClosing: (close) => { if (tab !== 0) { close.accepted = false; tab = 0 } }

    Connections {
        target: ut
        function onCompleted() { todayPage.pulse() }
        function onBreakOver() { todayPage.pulse() }
        function onToast(text) { toast.show(text, text === "Cuadradito borrado") }
        function onCelebrate(kicker, title, detail) { celebration.show(kicker, title, detail) }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: win.tab
            TodayPage {
                id: todayPage
                onOpenGoal: goalSheet.open()
                onOpenActivity: actSheet.open()
                onOpenSync: syncSheet.open()
                onOpenPomodoro: (id) => pomoSheet.show(id)
            }
            StatsPage { id: statsPage; onOpenPomodoro: (id) => pomoSheet.show(id) }
            HitosPage { id: hitosPage }
            SettingsPage { id: settingsPage; onOpenSync: syncSheet.open() }
        }
        NavBar {
            Layout.fillWidth: true
            current: win.tab
            running: ut.state === "running" || ut.state === "break"
            onPicked: (i) => win.tab = i
        }
    }

    SyncSheet { id: syncSheet }
    GoalSheet { id: goalSheet }
    PomodoroSheet { id: pomoSheet }
    ActivitySheet {
        id: actSheet
        current: ut.folderId
        onChosen: (id) => { ut.chooseFolder(id); actSheet.close() }
    }

    // ---- Avisos chiquitos (abajo) con "Deshacer" cuando corresponde
    Rectangle {
        id: toast
        property bool undo: false
        property alias text: toastText.text
        function show(t, canUndo) { text = t; undo = canUndo; opacity = 1; toastTimer.restart() }
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height - 64 - height - 16
        width: Math.min(parent.width - 32, toastRow.implicitWidth + 36)
        height: 48
        radius: 24
        color: "#2e2e2e"
        border.color: Theme.line
        opacity: 0
        visible: opacity > 0
        z: 100
        Behavior on opacity { NumberAnimation { duration: 200 } }
        RowLayout {
            id: toastRow
            anchors.centerIn: parent
            spacing: 16
            Text { id: toastText; color: Theme.text; font.pixelSize: 14 }
            Text {
                visible: toast.undo
                text: "Deshacer"
                color: Theme.accent; font.pixelSize: 14; font.weight: Font.DemiBold
                MouseArea { anchors.fill: parent; anchors.margins: -10; onClicked: { ut.undoDelete(); toast.opacity = 0 } }
            }
        }
        Timer { id: toastTimer; interval: 4000; onTriggered: toast.opacity = 0 }
    }

    // ---- Festejo (objetivo cumplido, marca del día, nueva etapa…)
    Item {
        id: celebration
        anchors.fill: parent
        z: 200
        visible: card.opacity > 0
        property var queue: []
        function show(k, t, d) {
            queue = queue.concat([{k: k, t: t, d: d}])
            if (!celTimer.running) next()
        }
        function next() {
            if (queue.length === 0) { card.opacity = 0; return }
            const e = queue[0]; queue = queue.slice(1)
            kick.text = e.k.toUpperCase(); ttl.text = e.t; det.text = e.d
            card.opacity = 1; card.scale = 0.9; pop.restart(); celTimer.restart()
        }
        MouseArea { anchors.fill: parent; enabled: card.opacity > 0.5; onClicked: { celTimer.stop(); celebration.next() } }
        Rectangle { anchors.fill: parent; color: "#000000"; opacity: card.opacity * 0.5 }
        Rectangle {
            id: card
            anchors.centerIn: parent
            width: Math.min(parent.width - 64, 320)
            height: cc.implicitHeight + 48
            radius: 22
            color: Theme.surface
            border.color: Theme.line
            opacity: 0
            Behavior on opacity { NumberAnimation { duration: 220 } }
            NumberAnimation { id: pop; target: card; property: "scale"; to: 1; duration: 380; easing.type: Easing.OutBack }
            ColumnLayout {
                id: cc
                anchors.centerIn: parent
                width: parent.width - 48
                spacing: 6
                Icon { Layout.alignment: Qt.AlignHCenter; name: "trophy"; width: 44; height: 44; color: Theme.gold; stroke: 2 }
                Text { id: kick; Layout.alignment: Qt.AlignHCenter; color: Theme.accent; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.2 }
                Text { id: ttl; Layout.alignment: Qt.AlignHCenter; color: Theme.text; font.pixelSize: 26; font.weight: Font.Bold; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Text { id: det; Layout.alignment: Qt.AlignHCenter; color: Theme.dim; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            }
        }
        Timer { id: celTimer; interval: 3200; onTriggered: celebration.next() }
    }
}
