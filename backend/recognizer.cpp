#include "recognizer.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <dlib/dnn.h>

using dlib::relu;
using dlib::con;
using dlib::max_pool;
using dlib::inception3;
using dlib::inception4;
using dlib::fc;
using dlib::loss_multiclass_log;
template <typename SUBNET> using block_a1 = relu<con<10,1,1,1,1,SUBNET>>;
template <typename SUBNET> using block_a2 = relu<con<10,3,3,1,1,relu<con<16,1,1,1,1,SUBNET>>>>;
template <typename SUBNET> using block_a3 = relu<con<10,5,5,1,1,relu<con<16,1,1,1,1,SUBNET>>>>;
template <typename SUBNET> using block_a4 = relu<con<10,1,1,1,1,max_pool<3,3,1,1,SUBNET>>>;
template <typename SUBNET> using incept_a = inception4<block_a1,block_a2,block_a3,block_a4, SUBNET>;

template <typename SUBNET> using block_b1 = relu<con<4,1,1,1,1,SUBNET>>;
template <typename SUBNET> using block_b2 = relu<con<4,3,3,1,1,SUBNET>>;
template <typename SUBNET> using block_b3 = relu<con<4,1,1,1,1,max_pool<3,3,1,1,SUBNET>>>;
template <typename SUBNET> using incept_b = inception3<block_b1,block_b2,block_b3,SUBNET>;

// Must match the network net.dat was trained with
using net_type = loss_multiclass_log<
        fc<26,
        relu<fc<32,
        max_pool<2,2,2,2,incept_b<
        max_pool<2,2,2,2,incept_a<
        dlib::input<dlib::matrix<unsigned char>>
        >>>>>>>>;

struct Network
{
    net_type net;
};

static constexpr float penWidth = 6;

CharacterRecognizer::CharacterRecognizer() = default;
CharacterRecognizer::~CharacterRecognizer() = default;

bool CharacterRecognizer::load(const std::string &netPath)
{
    try {
        m_net = std::make_unique<Network>();
        dlib::deserialize(netPath) >> m_net->net;
    } catch (const std::exception &e) {
        std::cerr << "Failed to load " << netPath << ": " << e.what() << std::endl;
        m_net.reset();
        return false;
    }
    return true;
}

// Rasterizes the strokes like the original QPainter code did: black ink on white.
static std::vector<unsigned char> rasterize(const std::vector<Stroke> &strokes, int size)
{
    std::vector<unsigned char> image(size * size, 255);
    const float radius = penWidth / 2;

    auto stamp = [&](float cx, float cy) {
        const int x0 = std::max(0, int(std::floor(cx - radius)));
        const int x1 = std::min(size - 1, int(std::ceil(cx + radius)));
        const int y0 = std::max(0, int(std::floor(cy - radius)));
        const int y1 = std::min(size - 1, int(std::ceil(cy + radius)));
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                const float dx = x + 0.5f - cx;
                const float dy = y + 0.5f - cy;
                if (dx * dx + dy * dy <= radius * radius) {
                    image[y * size + x] = 0;
                }
            }
        }
    };

    for (const Stroke &stroke : strokes) {
        for (size_t i = 0; i < stroke.size(); i++) {
            const Point &b = stroke[i];
            const Point &a = i > 0 ? stroke[i - 1] : b;
            const float length = std::hypot(b.x - a.x, b.y - a.y);
            const int steps = std::max(1, int(std::ceil(length)));
            for (int s = 0; s <= steps; s++) {
                const float t = float(s) / steps;
                stamp(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
            }
        }
    }
    return image;
}

CharacterRecognizer::Result CharacterRecognizer::recognize(const std::vector<Stroke> &strokes, int cellSize, char *letter)
{
    if (!m_net || cellSize < 8) {
        return Result::Empty;
    }

    const int size = cellSize;
    const std::vector<unsigned char> drawn = rasterize(strokes, size);

    int beginX = size, beginY = size, endX = 0, endY = 0;
    int black = 0;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (drawn[y * size + x] < 128) {
                beginX = std::min(x, beginX);
                beginY = std::min(y, beginY);
                endX = std::max(x, endX);
                endY = std::max(y, endY);
                black++;
            }
        }
    }

    // The user has scribbled over the cell to erase it
    if (black > (size * size) / 4) {
        return Result::Clear;
    }

    beginX++;
    beginY++;
    endX--;
    endY--;
    if (beginX >= endX && beginY >= endY) {
        return Result::Empty;
    }

    // Square crop around the ink, kept inside the cell
    const int width = std::max(endX - beginX, 1);
    const int height = std::max(endY - beginY, 1);
    const int edge = std::min(std::max(width, height), size);
    int left = beginX + width / 2 - edge / 2;
    int top = beginY + height / 2 - edge / 2;
    left = std::clamp(left, 0, size - edge);
    top = std::clamp(top, 0, size - edge);

    // Box-filter the crop down to 20x20 and pad it to 28x28, MNIST style:
    // white ink on black, letter centered with a 4 pixel border.
    dlib::matrix<unsigned char> input(28, 28);
    input = 0;
    const float scale = float(edge) / 20;
    for (int y = 0; y < 20; y++) {
        for (int x = 0; x < 20; x++) {
            const int sx0 = left + int(x * scale);
            const int sy0 = top + int(y * scale);
            const int sx1 = std::max(sx0 + 1, left + int((x + 1) * scale));
            const int sy1 = std::max(sy0 + 1, top + int((y + 1) * scale));
            int sum = 0;
            int count = 0;
            for (int sy = sy0; sy < sy1 && sy < size; sy++) {
                for (int sx = sx0; sx < sx1 && sx < size; sx++) {
                    sum += 255 - drawn[sy * size + sx];
                    count++;
                }
            }
            // The network was trained on transposed images
            input(x + 4, y + 4) = count ? sum / count : 0;
        }
    }

    const unsigned long prediction = m_net->net(input);
    *letter = char('A' + prediction);
    return Result::Letter;
}
