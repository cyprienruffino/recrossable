// QML_XHR_ALLOW_FILE_READ=1 qmltestrunner -platform offscreen -import dev/mock -input dev/tests
import QtQuick
import QtTest

Item {
    width: 954
    height: 1696

    Loader {
        id: app
        anchors.fill: parent
        source: "../../ui/main.qml"
    }

    TestCase {
        name: "Writing"
        when: app.item !== null && app.item.puzzle !== null

        function findChild(item, objectName) {
            for (const child of item.children) {
                if (child.objectName === objectName) {
                    return child
                }
                const found = findChild(child, objectName)
                if (found) {
                    return found
                }
            }
            return null
        }

        function test_writeLetter() {
            const root = app.item
            const ink = findChild(root, "ink")
            verify(ink !== null)
            const size = findChild(root, "board").cellSize

            // Write a two stroke letter in the second cell
            const cell = 1
            const x = cell * size
            mousePress(ink, x + size * 0.3, size * 0.2)
            mouseMove(ink, x + size * 0.3, size * 0.5)
            mouseMove(ink, x + size * 0.3, size * 0.8)
            mouseRelease(ink, x + size * 0.3, size * 0.8)
            mousePress(ink, x + size * 0.3, size * 0.8)
            mouseMove(ink, x + size * 0.6, size * 0.8)
            mouseRelease(ink, x + size * 0.6, size * 0.8)

            compare(root.selectedCell, cell)
            // Recognition waits for the pen to pause
            compare(root.entries[cell], "")
            tryVerify(() => root.entries[cell] === root.puzzle.solution[cell], 2000)

            const message = findChild(root, "backend").sent.pop()
            compare(message[0], 2)
            const parts = message[1].split(";")
            compare(parts[0], cell + " " + size)
            compare(parts.length, 3, "one part per stroke")
            // Stroke points are cell-local
            verify(parts[1].split(" ").every(p => parseInt(p.split(",")[0]) < size))
        }

        function test_inkStaysInCell() {
            const root = app.item
            const ink = findChild(root, "ink")
            const size = findChild(root, "board").cellSize
            mousePress(ink, size * 0.5, size * 0.5)
            mouseMove(ink, size * 3.5, size * 3.5)
            mouseRelease(ink, size * 3.5, size * 3.5)
            compare(root.selectedCell, 0)
            const strokes = ink.strokes[0]
            const last = strokes[strokes.length - 1]
            verify(last.every(p => p.x < size && p.y < size))
        }

        function test_blockedCellIgnored() {
            const root = app.item
            const ink = findChild(root, "ink")
            const size = findChild(root, "board").cellSize
            const block = root.puzzle.solution.indexOf("#")
            const columns = root.puzzle.columns
            const before = root.selectedCell
            mousePress(ink, (block % columns + 0.5) * size, (Math.floor(block / columns) + 0.5) * size)
            mouseRelease(ink, (block % columns + 0.5) * size, (Math.floor(block / columns) + 0.5) * size)
            compare(root.selectedCell, before)
            verify(ink.strokes[block] === undefined)
        }

        function test_eraser() {
            const root = app.item
            const ink = findChild(root, "ink")
            const backend = findChild(root, "backend")
            const size = findChild(root, "board").cellSize
            root.setEntry(0, "A")
            root.setEntry(1, "B")
            root.setEntry(2, "C")

            backend.messageReceived(104, "1")
            verify(root.erasing)
            // Rub over the first two cells
            mousePress(ink, size * 0.5, size * 0.5)
            mouseMove(ink, size * 1.0, size * 0.5)
            mouseMove(ink, size * 1.5, size * 0.5)
            mouseRelease(ink, size * 1.5, size * 0.5)
            compare(root.entries[0], "")
            compare(root.entries[1], "")
            compare(root.entries[2], "C")
            verify(ink.strokes[0] === undefined)

            // Back to the tip: writing works again
            backend.messageReceived(104, "0")
            verify(!root.erasing)
            mousePress(ink, size * 2.3, size * 0.3)
            mouseMove(ink, size * 2.3, size * 0.7)
            mouseRelease(ink, size * 2.3, size * 0.7)
            compare(ink.strokes[2].length, 1)
        }
    }
}
