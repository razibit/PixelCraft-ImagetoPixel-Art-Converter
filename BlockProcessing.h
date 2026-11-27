#pragma once

#include <algorithm>
#include <array>
#include <vector>

// RGB is also the on-disk row format passed to the PNG writer and OpenGL.
using RGB = std::array<unsigned char, 3>;
using Totals = std::array<int, 3>;
static_assert(sizeof(RGB) == 3, "RGB rows must be tightly packed");

struct Point {
    int x = 0, y = 0;
};

struct Block {
    int x, y, width, height;
    int count() const { return width * height; }
    Point center() const { return {x + width / 2, y + height / 2}; }
    Point pixel(int index) const { return {x + index % width, y + index / width}; }
    bool contains(Point p) const {
        return p.x >= x && p.x < x + width && p.y >= y && p.y < y + height;
    }
};

struct Image {
    int width = 0, height = 0;
    std::vector<RGB> rgb;                  // Top-left origin, already blended onto white.
    std::vector<unsigned char> rgba;       // Original PNG values if the image uses transparency.
    const RGB& at(int x, int y) const { return rgb[y * width + x]; }
};

enum class ColorMode { Color, Grayscale, BlackWhite };
enum class Palette { Off, Retro64, GameBoy };

struct Settings {
    int blockSize = 16;
    bool average = true;
    ColorMode color = ColorMode::Color;
    Palette palette = Palette::Off;
};

inline unsigned char onWhite(int channel, int alpha) {
    return static_cast<unsigned char>((channel * alpha + 255 * (255 - alpha) + 127) / 255);
}

inline Block blockAt(const Image& image, int size, Point point) {
    int x = point.x / size * size;
    int y = point.y / size * size;
    return {x, y, std::min(size, image.width - x), std::min(size, image.height - y)};
}

inline int grayNumerator(RGB color) {
    return 77 * color[0] + 150 * color[1] + 29 * color[2] + 128;
}

inline constexpr std::array<RGB, 4> greenShades = {{
    {15, 56, 15}, {48, 98, 48}, {139, 172, 15}, {155, 188, 15}
}};

struct BlockResult {
    Totals totals{};
    RGB sampled{}, afterMode{}, finalColor{};
    int gray = 0, paletteGray = 0, shade = 0;
    Totals retroBin{};
};

// Only the selected Average block requests a prefix list. Entry 0 is the
// total before the first pixel. Entry i + 1 is the total after pixel i.
inline BlockResult calculateBlock(const Image& image, Block block, const Settings& settings,
                                  std::vector<Totals>* prefix = nullptr) {
    BlockResult result;
    if (prefix) prefix->clear();
    if (settings.average) {
        if (prefix) {
            prefix->reserve(block.count() + 1);
            prefix->push_back(result.totals);
        }
        for (int y = block.y; y < block.y + block.height; ++y) {
            for (int x = block.x; x < block.x + block.width; ++x) {
                const RGB& pixel = image.at(x, y);
                for (int channel = 0; channel < 3; ++channel)
                    result.totals[channel] += pixel[channel];
                if (prefix) prefix->push_back(result.totals);
            }
        }
        for (int channel = 0; channel < 3; ++channel)
            result.sampled[channel] = static_cast<unsigned char>(result.totals[channel] / block.count());
    } else {
        Point center = block.center();
        result.sampled = image.at(center.x, center.y);
    }

    result.afterMode = result.sampled;
    if (settings.color != ColorMode::Color) {
        result.gray = grayNumerator(result.sampled) / 256;
        int level = result.gray;
        if (settings.color == ColorMode::BlackWhite) level = result.gray >= 128 ? 255 : 0;
        result.afterMode.fill(static_cast<unsigned char>(level));
    }

    result.finalColor = result.afterMode;
    if (settings.palette == Palette::Retro64) {
        for (int channel = 0; channel < 3; ++channel) {
            result.retroBin[channel] = result.afterMode[channel] / 64;
            result.finalColor[channel] = static_cast<unsigned char>(result.retroBin[channel] * 85);
        }
    } else if (settings.palette == Palette::GameBoy) {
        result.paletteGray = grayNumerator(result.afterMode) / 256;
        result.shade = result.paletteGray / 64;
        result.finalColor = greenShades[result.shade];
    }
    return result;
}

inline std::vector<RGB> makeArtwork(const Image& image, const Settings& settings) {
    std::vector<RGB> pixels(image.rgb.size());
    for (int y = 0; y < image.height; y += settings.blockSize) {
        for (int x = 0; x < image.width; x += settings.blockSize) {
            Block block = blockAt(image, settings.blockSize, {x, y});
            RGB color = calculateBlock(image, block, settings).finalColor;
            for (int row = y; row < y + block.height; ++row)
                std::fill_n(pixels.begin() + row * image.width + x, block.width, color);
        }
    }
    return pixels;
}

// A mixed block makes the opening example useful. Ties use image reading order.
inline Point examplePoint(const Image& image, int size) {
    Point middle{image.width / 2, image.height / 2};
    if (size == 1) return middle;
    Block central = blockAt(image, size, middle);
    Point best = middle;
    int bestDistance = image.width * image.width + image.height * image.height + 1;
    for (int y = 0; y < image.height; y += size) {
        for (int x = 0; x < image.width; x += size) {
            int dx = x - central.x, dy = y - central.y;
            int distance = dx * dx + dy * dy;
            if (distance >= bestDistance) continue;
            Block block = blockAt(image, size, {x, y});
            const RGB& first = image.at(x, y);
            bool mixed = false;
            for (int row = y; row < y + block.height && !mixed; ++row)
                for (int col = x; col < x + block.width && !mixed; ++col)
                    mixed = image.at(col, row) != first;
            if (mixed) {
                best = block.center();
                bestDistance = distance;
                if (distance == 0) return best;
            }
        }
    }
    return best;
}
