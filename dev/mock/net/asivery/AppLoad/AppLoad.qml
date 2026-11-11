// Stand-in for AppLoad's endpoint, to preview the UI with the qml tool.
// Serves dev/fixture-puzzle.json and "recognizes" every cell as its solution letter.
import QtQuick

Item {
    id: endpoint
    property string applicationID
    signal messageReceived(int type, string contents)

    property var puzzle: null

    property var sent: []

    function sendMessage(type, contents) {
        sent.push([type, contents])
        if (type === 1) {
            const request = new XMLHttpRequest()
            request.open("GET", Qt.resolvedUrl("../../../../fixture-puzzle.json"), false)
            request.send()
            puzzle = JSON.parse(request.responseText)
            reply.later(101, request.responseText)
        } else if (type === 2) {
            const cell = parseInt(contents)
            reply.later(102, cell + " " + puzzle.solution[cell])
        }
    }

    function terminate() {
        Qt.quit()
    }

    Timer {
        id: reply
        property var queue: []
        function later(type, contents) {
            queue.push([type, contents])
            start()
        }
        interval: 50
        onTriggered: {
            const pending = queue
            queue = []
            pending.forEach(message => endpoint.messageReceived(message[0], message[1]))
        }
    }

    Component.onCompleted: reply.later(100, "ready")
}
