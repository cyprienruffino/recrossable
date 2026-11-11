#ifndef PUZZLE_H
#define PUZZLE_H

#include <string>
#include <unordered_map>
#include <vector>

class PuzzleGenerator
{
public:
    bool loadWordlist(const std::string &path);

    // Generates a crossword from the pattern file and returns it as JSON,
    // or an empty string on failure.
    std::string generate(const std::string &patternPath);

    // Some generations get stuck backtracking, so each attempt runs in a child
    // process that gets killed after timeoutMs, and is retried with a new seed.
    std::string generateWithTimeout(const std::string &patternPath, int timeoutMs, int attempts);

private:
    std::unordered_map<std::string, std::string> m_hints;
    std::vector<std::string> m_words;
};

#endif // PUZZLE_H
