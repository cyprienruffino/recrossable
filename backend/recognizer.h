#ifndef RECOGNIZER_H
#define RECOGNIZER_H

#include <memory>
#include <string>
#include <vector>

struct Network;

struct Point
{
    float x;
    float y;
};
using Stroke = std::vector<Point>;

class CharacterRecognizer
{
public:
    enum class Result { Letter, Empty, Clear };

    CharacterRecognizer();
    ~CharacterRecognizer();

    bool load(const std::string &netPath);

    // Strokes are in cell-local coordinates, the cell being cellSize x cellSize.
    Result recognize(const std::vector<Stroke> &strokes, int cellSize, char *letter);

private:
    std::unique_ptr<Network> m_net;
};

#endif // RECOGNIZER_H
