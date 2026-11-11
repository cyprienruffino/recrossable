import QtQuick
import net.asivery.AppLoad 1.0

Rectangle {
    id: root
    anchors.fill: parent
    color: "white"

    // Must match backend/main.cpp
    readonly property int msgNewPuzzle: 1
    readonly property int msgRecognize: 2
    readonly property int msgReady: 100
    readonly property int msgPuzzle: 101
    readonly property int msgRecognized: 102
    readonly property int msgError: 103
    readonly property int msgEraser: 104

    // Patterns that generate in a few seconds on the device
    readonly property var patternSets: ({
        "Small": ["5x5", "5x5f", "grid5x5", "star2", "symmetric2"],
        "Medium": ["diamonds", "tilt", "circle", "diamond", "three_and_five", "tilt_tough"],
        "Large": ["ginsberg", "ginsberg2", "five", "smithnsteen", "fourdots", "15x15-independent-a", "15x15-guardian-a", "15x15-guardian-b"]
    })
    property string puzzleSize: "Medium"

    property var puzzle: null
    property var entries: []          // letter per cell, "" when empty
    property var cellClues: []        // per cell: { across: clue index, down: clue index }
    property int selectedCell: -1
    property bool selectAcross: true
    property bool checking: false
    property bool generating: true
    property bool erasing: false      // the eraser end of the pen is near the screen
    property string status: "Starting…"

    readonly property bool solved: puzzle !== null && entries.join("") === puzzle.solution.replace(/#/g, "")

    // Called by AppLoad when the app is closed
    function unloading() {
        backend.terminate()
    }

    function newPuzzle() {
        const patterns = patternSets[puzzleSize]
        generating = true
        status = "Generating puzzle…"
        backend.sendMessage(msgNewPuzzle, patterns[Math.floor(Math.random() * patterns.length)])
    }

    function loadPuzzle(json) {
        const p = JSON.parse(json)
        const clues = []
        for (let i = 0; i < p.solution.length; i++) {
            clues.push({ across: -1, down: -1 })
        }
        p.across.forEach((clue, index) => clue.cells.forEach(cell => clues[cell].across = index))
        p.down.forEach((clue, index) => clue.cells.forEach(cell => clues[cell].down = index))

        ink.reset()
        cellClues = clues
        entries = Array.from(p.solution, c => c === "#" ? "#" : "").map(c => c === "#" ? "" : c)
        puzzle = p
        checking = false
        selectedCell = -1
        selectAcross = true
        generating = false
    }

    function setEntry(cell, letter) {
        const updated = entries.slice()
        updated[cell] = letter
        entries = updated
    }

    function isOpen(cell) {
        return puzzle !== null && puzzle.solution[cell] !== "#"
    }

    function selectCell(cell) {
        if (!isOpen(cell)) {
            return
        }
        const clues = cellClues[cell]
        if (selectAcross && clues.across === -1) {
            selectAcross = false
        } else if (!selectAcross && clues.down === -1) {
            selectAcross = true
        }
        selectedCell = cell
    }

    function selectClue(across, index) {
        selectAcross = across
        selectedCell = (across ? puzzle.across : puzzle.down)[index].cells[0]
    }

    readonly property var selectedClue: {
        if (puzzle === null || selectedCell < 0) {
            return null
        }
        const index = selectAcross ? cellClues[selectedCell].across : cellClues[selectedCell].down
        return index < 0 ? null : (selectAcross ? puzzle.across : puzzle.down)[index]
    }

    AppLoad {
        id: backend
        objectName: "backend"
        applicationID: "recrossable"
        onMessageReceived: (type, contents) => {
            switch (type) {
            case root.msgReady:
                if (root.puzzle === null) {
                    root.newPuzzle()
                }
                break
            case root.msgPuzzle:
                root.loadPuzzle(contents)
                break
            case root.msgRecognized: {
                const space = contents.indexOf(" ")
                const cell = parseInt(contents.substring(0, space))
                const letter = contents.substring(space + 1)
                ink.clearCell(cell)
                if (letter === "!") {
                    root.setEntry(cell, "")
                } else if (letter !== "") {
                    root.setEntry(cell, letter)
                }
                break
            }
            case root.msgError:
                root.generating = false
                root.status = contents
                break
            case root.msgEraser:
                root.erasing = contents === "1"
                break
            }
        }
    }

    // Top bar
    Row {
        id: toolbar
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            margins: 16
        }
        height: 72
        spacing: 12

        Repeater {
            model: ["Small", "Medium", "Large"]
            delegate: Button {
                text: modelData
                checked: root.puzzleSize === modelData
                onClicked: root.puzzleSize = modelData
            }
        }
        Button {
            text: "New"
            enabled: !root.generating
            onClicked: root.newPuzzle()
        }
        Button {
            text: "Check"
            enabled: root.puzzle !== null
            checked: root.checking
            onClicked: root.checking = !root.checking
        }
    }

    // Grid and ink layer
    Item {
        id: board
        objectName: "board"
        anchors {
            top: toolbar.bottom
            topMargin: 16
            horizontalCenter: parent.horizontalCenter
        }
        readonly property int columns: root.puzzle ? root.puzzle.columns : 1
        readonly property int rows: root.puzzle ? root.puzzle.rows : 1
        readonly property int cellSize: Math.floor(Math.min((root.width - 32) / columns, root.height * 0.55 / rows))
        width: cellSize * columns
        height: cellSize * rows
        visible: root.puzzle !== null

        Repeater {
            model: root.puzzle ? root.puzzle.solution.length : 0
            delegate: Rectangle {
                readonly property bool open: root.isOpen(index)
                readonly property bool inSelectedWord: root.selectedClue !== null && root.selectedClue.cells.indexOf(index) !== -1
                readonly property string entry: root.entries[index] || ""
                readonly property bool wrong: root.checking && entry !== "" && entry !== root.puzzle.solution[index]

                x: (index % board.columns) * board.cellSize
                y: Math.floor(index / board.columns) * board.cellSize
                width: board.cellSize + 1
                height: board.cellSize + 1
                color: !open ? "black" : index === root.selectedCell ? "#a0a0a0" : inSelectedWord ? "#d8d8d8" : "white"
                border.width: 1
                border.color: "black"

                Text {
                    x: 3
                    y: 1
                    visible: parent.open
                    text: root.puzzle && root.puzzle.numbers[index] !== undefined ? root.puzzle.numbers[index] : ""
                    font.pixelSize: Math.max(10, board.cellSize / 4.5)
                }
                Text {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: board.cellSize / 12
                    text: parent.entry
                    font.pixelSize: board.cellSize * 0.6
                    font.bold: true
                }
                // Strike out wrong letters when checking
                Rectangle {
                    visible: parent.wrong
                    anchors.centerIn: parent
                    width: parent.width * 1.2
                    height: 3
                    rotation: -45
                    color: "black"
                }
            }
        }

        Canvas {
            id: ink
            objectName: "ink"
            anchors.fill: parent

            property var strokes: ({})    // cell -> list of strokes, in cell-local coordinates
            property var current: null    // { cell, points }
            property var pendingSegments: []
            property bool pendingClear: false

            function reset() {
                strokes = {}
                current = null
                pendingSegments = []
                pendingClear = true
                requestPaint()
            }

            function cellAt(x, y) {
                const column = Math.floor(x / board.cellSize)
                const row = Math.floor(y / board.cellSize)
                if (column < 0 || row < 0 || column >= board.columns || row >= board.rows) {
                    return -1
                }
                return row * board.columns + column
            }

            function clearCell(cell) {
                delete strokes[cell]
                const x = (cell % board.columns) * board.cellSize
                const y = Math.floor(cell / board.columns) * board.cellSize
                pendingSegments.push({ clear: true, x: x, y: y })
                markDirty(Qt.rect(x, y, board.cellSize, board.cellSize))
            }

            // Clears the letter and the ink of every cell the eraser goes over
            function erase(x, y) {
                const cell = cellAt(x, y)
                if (!root.isOpen(cell) || cell === current.lastErased) {
                    return
                }
                current.lastErased = cell
                if (recognizeTimer.cell === cell) {
                    recognizeTimer.stop()
                }
                clearCell(cell)
                root.setEntry(cell, "")
            }

            function begin(x, y) {
                if (root.erasing) {
                    current = { erasing: true, lastErased: -1 }
                    erase(x, y)
                    return
                }
                const cell = cellAt(x, y)
                if (!root.isOpen(cell)) {
                    current = null
                    return
                }
                // Moving to another cell: recognize the previous one right away
                if (recognizeTimer.running && recognizeTimer.cell !== cell) {
                    recognizeTimer.stop()
                    recognize(recognizeTimer.cell)
                }
                recognizeTimer.stop()
                if (cell !== root.selectedCell) {
                    root.selectCell(cell)
                }
                current = { cell: cell, points: [Qt.point(x, y)] }
            }

            function extend(x, y) {
                if (current === null) {
                    return
                }
                if (current.erasing) {
                    erase(x, y)
                    return
                }
                // Keep the ink inside the cell it started in
                const left = (current.cell % board.columns) * board.cellSize
                const top = Math.floor(current.cell / board.columns) * board.cellSize
                x = Math.max(left, Math.min(left + board.cellSize - 1, x))
                y = Math.max(top, Math.min(top + board.cellSize - 1, y))

                const last = current.points[current.points.length - 1]
                current.points.push(Qt.point(x, y))
                pendingSegments.push({ from: last, to: Qt.point(x, y) })
                markDirty(Qt.rect(Math.min(last.x, x) - 4, Math.min(last.y, y) - 4,
                                  Math.abs(last.x - x) + 8, Math.abs(last.y - y) + 8))
            }

            function end() {
                if (current === null) {
                    return
                }
                if (current.erasing) {
                    current = null
                    return
                }
                const cell = current.cell
                const left = (cell % board.columns) * board.cellSize
                const top = Math.floor(cell / board.columns) * board.cellSize
                if (!strokes[cell]) {
                    strokes[cell] = []
                }
                strokes[cell].push(current.points.map(p => Qt.point(p.x - left, p.y - top)))
                current = null
                recognizeTimer.cell = cell
                recognizeTimer.restart()
            }

            function recognize(cell) {
                const cellStrokes = strokes[cell]
                if (!cellStrokes) {
                    return
                }
                const encoded = cellStrokes.map(stroke => stroke.map(p => Math.round(p.x) + "," + Math.round(p.y)).join(" "))
                backend.sendMessage(root.msgRecognize, cell + " " + board.cellSize + ";" + encoded.join(";"))
            }

            onPaint: {
                const ctx = getContext("2d")
                if (pendingClear) {
                    ctx.clearRect(0, 0, width, height)
                    pendingClear = false
                }
                ctx.strokeStyle = "black"
                ctx.lineWidth = 6
                ctx.lineCap = "round"
                for (const segment of pendingSegments) {
                    if (segment.clear) {
                        ctx.clearRect(segment.x, segment.y, board.cellSize, board.cellSize)
                        continue
                    }
                    ctx.beginPath()
                    ctx.moveTo(segment.from.x, segment.from.y)
                    ctx.lineTo(segment.to.x, segment.to.y)
                    ctx.stroke()
                }
                pendingSegments = []
            }

            // xochitl delivers the pen as plain mouse events, not as a stylus device,
            // which is also how AppLoad's own qtfb windows receive it. Touches come
            // as mouse events synthesized by Qt; ignore those so a resting palm doesn't write.
            MouseArea {
                id: pen
                anchors.fill: parent
                preventStealing: true
                onPressed: mouse => {
                    if (mouse.source !== Qt.MouseEventNotSynthesized) {
                        mouse.accepted = false
                        return
                    }
                    ink.begin(mouse.x, mouse.y)
                }
                onPositionChanged: mouse => ink.extend(mouse.x, mouse.y)
                onReleased: ink.end()
                onCanceled: ink.end()
            }

            // Letters are often several strokes; wait for the pen to pause before recognizing
            Timer {
                id: recognizeTimer
                property int cell: -1
                interval: 700
                onTriggered: ink.recognize(cell)
            }
        }
    }

    // Current clue; tapping it switches between across and down
    Rectangle {
        id: currentClue
        anchors {
            top: board.bottom
            topMargin: 16
            left: parent.left
            right: parent.right
            margins: 16
        }
        height: Math.max(clueText.implicitHeight + 24, 80)
        border.width: 3
        border.color: "black"
        visible: root.puzzle !== null

        Text {
            id: clueText
            anchors {
                fill: parent
                margins: 12
            }
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
            font.pixelSize: 28
            text: root.solved ? "Solved!"
                : root.selectedClue ? root.selectedClue.number + (root.selectAcross ? " Across: " : " Down: ") + root.selectedClue.clue
                : "Write a letter in a square, or pick a clue below"
        }
        TapHandler {
            onTapped: {
                if (root.selectedCell >= 0) {
                    const clues = root.cellClues[root.selectedCell]
                    if ((root.selectAcross ? clues.down : clues.across) !== -1) {
                        root.selectAcross = !root.selectAcross
                    }
                }
            }
        }
    }

    // Clue lists
    Row {
        anchors {
            top: currentClue.bottom
            topMargin: 16
            bottom: parent.bottom
            bottomMargin: 16
            left: parent.left
            leftMargin: 16
            right: parent.right
            rightMargin: 16
        }
        spacing: 16
        visible: root.puzzle !== null

        Repeater {
            model: [true, false]
            delegate: Column {
                id: clueColumn
                readonly property bool across: modelData
                // Index of the selected clue in this list, -1 when it is in the other one
                readonly property int selectedIndex: root.selectedCell < 0 || root.selectAcross !== across ? -1
                    : across ? root.cellClues[root.selectedCell].across : root.cellClues[root.selectedCell].down
                width: (parent.width - parent.spacing) / 2
                height: parent.height
                spacing: 8

                Text {
                    id: heading
                    text: clueColumn.across ? "Across" : "Down"
                    font.pixelSize: 30
                    font.bold: true
                }
                ListView {
                    id: clueList
                    width: parent.width
                    height: parent.height - heading.height - parent.spacing
                    clip: true
                    spacing: 6
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.puzzle ? (clueColumn.across ? root.puzzle.across : root.puzzle.down) : []
                    delegate: Text {
                        width: ListView.view.width
                        wrapMode: Text.WordWrap
                        font.pixelSize: 24
                        font.bold: index === clueColumn.selectedIndex
                        text: modelData.number + ". " + modelData.clue
                        TapHandler {
                            onTapped: root.selectClue(clueColumn.across, index)
                        }
                    }
                }
                onSelectedIndexChanged: {
                    if (selectedIndex >= 0) {
                        clueList.positionViewAtIndex(selectedIndex, ListView.Contain)
                    }
                }
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: root.generating || root.puzzle === null
        text: root.status
        font.pixelSize: 36
    }

    component Button: Rectangle {
        id: button
        property string text
        property bool checked: false
        property bool enabled: true
        signal clicked()

        width: label.implicitWidth + 40
        height: parent.height
        radius: 8
        border.width: 3
        border.color: enabled ? "black" : "#a0a0a0"
        color: checked ? "black" : "white"

        Text {
            id: label
            anchors.centerIn: parent
            text: button.text
            font.pixelSize: 28
            color: button.checked ? "white" : button.enabled ? "black" : "#a0a0a0"
        }
        TapHandler {
            enabled: button.enabled
            onTapped: button.clicked()
        }
    }
}
