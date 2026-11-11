// AppLoad backend for reCrossable: generates crosswords and recognizes
// handwritten letters, talking to the QML frontend over AppLoad's socket.

#include "puzzle.h"
#include "recognizer.h"

#include "cwc/symbol.hh"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

// Must match AppLoad's src/protocol.h
constexpr int MESSAGE_SYSTEM_TERMINATE = -1;
constexpr int MESSAGE_SYSTEM_NEW_COORDINATOR = -2;
constexpr size_t MAX_MESSAGE_LENGTH = 10485760;

struct PacketHeader
{
    int type;
    int messageLength;
};

// Frontend -> backend
constexpr int MSG_NEW_PUZZLE = 1; // contents: pattern name
constexpr int MSG_RECOGNIZE = 2; // contents: "<cell> <cell size>;x,y x,y ...;x,y ..."

// Backend -> frontend
constexpr int MSG_READY = 100;
constexpr int MSG_PUZZLE = 101; // contents: puzzle JSON
constexpr int MSG_RECOGNIZED = 102; // contents: "<cell> <letter>", letter is "" when empty and "!" to clear
constexpr int MSG_ERROR = 103;

constexpr int generationTimeoutMs = 8000;

int g_socket = -1;

bool sendMessage(int type, const std::string &contents)
{
    const PacketHeader header = { type, int(contents.size()) };
    if (send(g_socket, &header, sizeof header, 0) == -1) {
        perror("send");
        return false;
    }
    if (send(g_socket, contents.data(), contents.size(), 0) == -1) {
        perror("send");
        return false;
    }
    return true;
}

bool isValidPatternName(const std::string &name)
{
    if (name.empty()) {
        return false;
    }
    for (const char c : name) {
        if (!std::isalnum((unsigned char)c) && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

void handleRecognize(CharacterRecognizer &recognizer, const std::string &contents)
{
    std::istringstream input(contents);
    std::string head;
    std::getline(input, head, ';');
    int cell = -1;
    int cellSize = 0;
    std::istringstream(head) >> cell >> cellSize;

    std::vector<Stroke> strokes;
    std::string strokeText;
    while (std::getline(input, strokeText, ';')) {
        Stroke stroke;
        std::istringstream points(strokeText);
        Point point;
        char comma;
        while (points >> point.x >> comma >> point.y) {
            stroke.push_back(point);
        }
        if (!stroke.empty()) {
            strokes.push_back(stroke);
        }
    }

    char letter = 0;
    std::string result;
    switch (recognizer.recognize(strokes, cellSize, &letter)) {
    case CharacterRecognizer::Result::Letter:
        result = std::string(1, letter);
        break;
    case CharacterRecognizer::Result::Clear:
        result = "!";
        break;
    case CharacterRecognizer::Result::Empty:
        break;
    }
    sendMessage(MSG_RECOGNIZED, std::to_string(cell) + ' ' + result);
}

} // namespace

int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <appload socket path>" << std::endl;
        return 1;
    }

    std::srand(std::time(nullptr));
    Symbol::buildindex();

    PuzzleGenerator generator;
    if (!generator.loadWordlist("data/nyt.tsv")) {
        return 1;
    }

    // For testing patterns without a frontend
    if (std::string(argv[1]) == "--generate" && argc > 2) {
        const std::string puzzle = generator.generateWithTimeout("data/patterns/" + std::string(argv[2]), generationTimeoutMs, 5);
        std::cout << puzzle << std::endl;
        return puzzle.empty() ? 1 : 0;
    }

    CharacterRecognizer recognizer;
    if (!recognizer.load("data/net.dat")) {
        return 1;
    }

    g_socket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (g_socket == -1) {
        perror("socket");
        return 1;
    }
    sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, argv[1], sizeof address.sun_path - 1);
    if (connect(g_socket, reinterpret_cast<sockaddr *>(&address), sizeof address) != 0) {
        perror("connect");
        return 1;
    }

    std::vector<char> buffer(MAX_MESSAGE_LENGTH);
    while (true) {
        PacketHeader header;
        if (recv(g_socket, &header, sizeof header, 0) < 1) {
            break;
        }
        if (header.messageLength < 0 || size_t(header.messageLength) > MAX_MESSAGE_LENGTH) {
            std::cerr << "Invalid message length " << header.messageLength << std::endl;
            break;
        }
        // AppLoad always sends the contents as a second packet, even when empty
        const ssize_t received = recv(g_socket, buffer.data(), header.messageLength, 0);
        if (received < 0) {
            break;
        }
        const std::string contents(buffer.data(), received);

        switch (header.type) {
        case MESSAGE_SYSTEM_TERMINATE:
            return 0;
        case MESSAGE_SYSTEM_NEW_COORDINATOR:
            sendMessage(MSG_READY, "ready");
            break;
        case MSG_NEW_PUZZLE: {
            if (!isValidPatternName(contents)) {
                sendMessage(MSG_ERROR, "Invalid pattern: " + contents);
                break;
            }
            const std::string puzzle = generator.generateWithTimeout("data/patterns/" + contents, generationTimeoutMs, 5);
            if (puzzle.empty()) {
                sendMessage(MSG_ERROR, "Could not generate a crossword, try again");
            } else {
                sendMessage(MSG_PUZZLE, puzzle);
            }
            break;
        }
        case MSG_RECOGNIZE:
            handleRecognize(recognizer, contents);
            break;
        default:
            std::cerr << "Unknown message type " << header.type << std::endl;
            break;
        }
    }
    return 0;
}
