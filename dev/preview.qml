// Previews the app at the Paper Pro Move's resolution:
//   QML_XHR_ALLOW_FILE_READ=1 qml -I dev/mock dev/preview.qml
// Pass "-- screenshot.png" to save a screenshot after filling in a few letters and quit.
import QtQuick
import QtQuick.Window

Window {
    width: 954
    height: 1696
    visible: true
    title: "reCrossable preview"

    Loader {
        id: app
        anchors.fill: parent
        source: "../ui/main.qml"
    }

    Timer {
        interval: 1500
        running: Qt.application.arguments.length > 1 && Qt.application.arguments[Qt.application.arguments.length - 1].endsWith(".png")
        onTriggered: {
            const root = app.item
            const solution = root.puzzle.solution
            for (let i = 0; i < 20; i++) {
                if (solution[i] !== "#") {
                    root.setEntry(i, i === 3 ? "X" : solution[i])
                }
            }
            root.selectCell(16)
            root.selectAcross = false
            root.selectCell(150)
            root.checking = true
            capture.start()
        }
    }
    Timer {
        id: capture
        interval: 300
        onTriggered: app.grabToImage(result => {
            result.saveToFile(Qt.application.arguments[Qt.application.arguments.length - 1])
            Qt.quit()
        })
    }
}
