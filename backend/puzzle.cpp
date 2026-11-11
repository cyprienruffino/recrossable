#include "puzzle.h"
#include "json.h"

#include "cwc/cwc.hh"
#include "cwc/dict.hh"
#include "cwc/grid.hh"
#include "cwc/letterdict.hh"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static std::string trimmed(const std::string &s)
{
    const size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

bool PuzzleGenerator::loadWordlist(const std::string &path)
{
    const auto start = std::chrono::steady_clock::now();

    std::ifstream file(path);
    if (!file) {
        std::cerr << "Failed to load " << path << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        const size_t tab = line.find('\t');
        if (tab == std::string::npos) {
            continue;
        }
        std::string hint = trimmed(line.substr(0, tab));
        std::string word = trimmed(line.substr(line.rfind('\t') + 1));
        std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c) { return std::tolower(c); });

        if (hint.size() >= 2 && hint.front() == '"' && hint.back() == '"') {
            hint = hint.substr(1, hint.size() - 2);
        }
        for (size_t pos = hint.find("\"\""); pos != std::string::npos; pos = hint.find("\"\"", pos + 1)) {
            hint.erase(pos, 1);
        }
        if (hint.empty() || word.empty()) {
            continue;
        }

        if (m_hints.emplace(word, hint).second) {
            m_words.push_back(word);
        } else {
            m_hints[word] = hint;
        }
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    std::cerr << "Loaded " << m_words.size() << " words in " << elapsed.count() << "ms" << std::endl;
    return !m_words.empty();
}

// Cells belonging to the word starting at `start`, walking `step` cells at a time
static std::vector<int> wordCells(Grid &grid, int start, bool across)
{
    std::vector<int> cells;
    int x = start % grid.w;
    int y = start / grid.w;
    while (grid.cellat(x, y).isinside()) {
        cells.push_back(grid.cellnofromxy(x, y));
        if (across) {
            x++;
        } else {
            y++;
        }
    }
    return cells;
}

std::string PuzzleGenerator::generate(const std::string &patternPath)
{
    const auto start = std::chrono::steady_clock::now();

    std::ifstream patternFile(patternPath);
    if (!patternFile) {
        std::cerr << "Failed to load pattern " << patternPath << std::endl;
        return {};
    }

    LetterDict dict;
    auto wordlist = std::make_unique<WordList>();
    dict.wl = wordlist.get();
    for (const std::string &word : m_words) {
        dict.wl->addWord(word);
    }
    const int nwords = dict.wl->numwords();
    for (int i = 0; i < nwords; i++) {
        dict.addword((*dict.wl)[i], i);
    }

    Grid grid;
    grid.load_template(patternFile);

    FloodWalker walker(grid);
    SmartBacktracker backtracker(grid);
    Compiler compiler(grid, walker, backtracker, dict);
    if (!compiler.compile()) {
        std::cerr << "Failed to compile crossword" << std::endl;
        return {};
    }
    Answers answers = grid.getanswers();

    std::string solution;
    for (int i = 0; i < grid.numcells(); i++) {
        Cell &cell = grid.cellno(i);
        solution += cell.isoutside() ? '#' : char(std::toupper(cell.tostring()[0]));
    }

    std::ostringstream out;
    out << "{\"rows\":" << grid.h << ",\"columns\":" << grid.w
        << ",\"solution\":" << json::quote(solution)
        << ",\"numbers\":{";
    bool first = true;
    for (const auto &[cell, number] : answers.celltoclue) {
        out << (first ? "" : ",") << json::quote(std::to_string(cell)) << ':' << number;
        first = false;
    }
    out << '}';

    auto writeClues = [&](const char *name, ClueNumbering &numbering, bool across) {
        out << ",\"" << name << "\":[";
        bool firstClue = true;
        for (const int number : numbering.clues) {
            const std::string &answer = numbering.cluetoanswer[number];
            int startCell = -1;
            for (const auto &[cell, n] : answers.celltoclue) {
                if (n == number) {
                    startCell = cell;
                }
            }
            out << (firstClue ? "" : ",") << "{\"number\":" << number
                << ",\"clue\":" << json::quote(m_hints[answer])
                << ",\"cells\":[";
            bool firstCell = true;
            for (const int cell : wordCells(grid, startCell, across)) {
                out << (firstCell ? "" : ",") << cell;
                firstCell = false;
            }
            out << "]}";
            firstClue = false;
        }
        out << ']';
    };
    writeClues("across", answers.across, true);
    writeClues("down", answers.down, false);
    out << '}';

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    std::cerr << "Generated crossword in " << elapsed.count() << "ms" << std::endl;
    return out.str();
}

std::string PuzzleGenerator::generateWithTimeout(const std::string &patternPath, int timeoutMs, int attempts)
{
    for (int attempt = 0; attempt < attempts; attempt++) {
        int fds[2];
        if (pipe(fds) != 0) {
            perror("pipe");
            return {};
        }
        // Don't let a dying child take the backend down with it
        signal(SIGPIPE, SIG_IGN);

        const pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close(fds[0]);
            close(fds[1]);
            return {};
        }
        if (pid == 0) {
            close(fds[0]);
            std::srand(std::time(nullptr) ^ getpid());
            const std::string puzzle = generate(patternPath);
            size_t written = 0;
            while (written < puzzle.size()) {
                const ssize_t n = write(fds[1], puzzle.data() + written, puzzle.size() - written);
                if (n <= 0) {
                    break;
                }
                written += n;
            }
            _exit(puzzle.empty() ? 1 : 0);
        }
        close(fds[1]);

        std::string puzzle;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        bool timedOut = false;
        while (true) {
            const int remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
            if (remaining <= 0) {
                timedOut = true;
                break;
            }
            pollfd pfd = { fds[0], POLLIN, 0 };
            if (poll(&pfd, 1, remaining) <= 0) {
                continue;
            }
            char buffer[65536];
            const ssize_t n = read(fds[0], buffer, sizeof buffer);
            if (n <= 0) {
                break;
            }
            puzzle.append(buffer, n);
        }
        close(fds[0]);
        if (timedOut) {
            kill(pid, SIGKILL);
        }
        int status = 0;
        waitpid(pid, &status, 0);

        if (!timedOut && WIFEXITED(status) && WEXITSTATUS(status) == 0 && !puzzle.empty()) {
            return puzzle;
        }
        std::cerr << "Generation attempt " << attempt + 1 << (timedOut ? " timed out" : " failed") << std::endl;
    }
    return {};
}
