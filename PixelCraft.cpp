#define NOMINMAX
#define STBI_ONLY_PNG
#define STBI_MAX_DIMENSIONS 4096
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <windows.h>
#include <commdlg.h>
#include <GL/freeglut.h>
#include "BlockProcessing.h"
#include <cmath>
#include <iostream>
#include <string>
#include <utility>

using namespace std;

Image image;
Settings settings;
vector<RGB> artwork;
Point anchor;                         // The source location last picked on either image.
Block selected{0, 0, 1, 1};
BlockResult result;
vector<Totals> prefix;                // Running RGB totals for this block alone.
int inspectedPixel = 0, stage = 0;
bool showGrid = false;
string status;

const array<string, 5> stages{"Source block", "Sample color", "Color mode", "Palette", "Fill block"};
const array<string, 3> channels{"R", "G", "B"};
const array<string, 3> colorNames{"Full color", "Grayscale", "B&W"};
const array<string, 3> paletteNames{"Off", "Retro 64", "Game Boy"};
constexpr int tileSize = 16;

string pointText(Point p) {
    return "(" + to_string(p.x) + ", " + to_string(p.y) + ")";
}
string rgbText(RGB color) {
    return "(" + to_string(color[0]) + ", " + to_string(color[1]) + ", " + to_string(color[2]) + ")";
}
string dimensions(int width, int height) {
    return to_string(width) + " x " + to_string(height);
}

void refreshSelection(Point preferredPixel) {
    selected = blockAt(image, settings.blockSize, anchor);
    Point p = selected.contains(preferredPixel) ? preferredPixel : anchor;
    inspectedPixel = (p.y - selected.y) * selected.width + p.x - selected.x;
    result = calculateBlock(image, selected, settings, &prefix);
}
void chooseBlock(Point p) {
    anchor = p;
    Block block = blockAt(image, settings.blockSize, anchor);
    refreshSelection({block.x, block.y});
}

string readPath() {
    cout << "\nPNG path (Enter for sample.png): " << flush;
    string path;
    if (!getline(cin, path)) return "";
    if (path.empty()) return "sample.png";
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"')
        path = path.substr(1, path.size() - 2);
    return path;
}

string chooseImagePath() {
    char path[32768] = {};
    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFilter = "PNG images (*.png)\0*.png\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = sizeof(path);
    dialog.lpstrTitle = "Open PNG image";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameA(&dialog) ? path : "";
}

bool loadImage(const string& path) {
    int width, height, components;
    if (!stbi_info(path.c_str(), &width, &height, &components)) {
        cerr << "Could not read PNG (maximum 4096 x 4096): " << path << "\n";
        return false;
    }
    if (width > 4096 || height > 4096) {
        cerr << "Image must be at most 4096 x 4096 pixels.\n";
        return false;
    }
    unsigned char* bytes = stbi_load(path.c_str(), &width, &height, &components, 4);
    if (!bytes) {
        cerr << "Could not decode PNG: " << path << "\n";
        return false;
    }
    Image next;
    next.width = width;
    next.height = height;
    next.rgb.resize(width * height);
    bool transparent = false;
    for (size_t i = 0; i < next.rgb.size(); ++i) {
        const unsigned char* pixel = bytes + 4 * i;
        if (pixel[3] < 255) transparent = true;
        for (int channel = 0; channel < 3; ++channel)
            next.rgb[i][channel] = onWhite(pixel[channel], pixel[3]);
    }
    if (transparent) next.rgba.assign(bytes, bytes + 4 * next.rgb.size());
    stbi_image_free(bytes);

    image = move(next);
    artwork = makeArtwork(image, settings);
    stage = 0;
    status.clear();
    chooseBlock(examplePoint(image, settings.blockSize));
    cout << "Loaded " << path << " (" << width << " x " << height << ")\n";
    return true;
}

bool saveImage() {
    bool ok = stbi_write_png("pixelcraft-output.png", image.width, image.height, 3,
                             artwork.data(), image.width * 3) != 0;
    status = ok ? "Saved pixelcraft-output.png" : "Save failed. Check folder permissions.";
    cout << status << "\n";
    return ok;
}

// All drawing and hit testing use top-left window coordinates.
struct Rect {
    float x, y, w, h;
    bool contains(float px, float py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
};

constexpr RGB background{19, 24, 31}, surface{31, 39, 48}, well{14, 19, 25};
constexpr RGB lineColor{66, 80, 92}, ink{241, 244, 246}, muted{177, 191, 201};
constexpr RGB teal{119, 224, 202}, gold{255, 209, 100}, darkInk{13, 37, 35};
constexpr float margin = 18, gap = 12;
float viewWidth = 1200, viewHeight = 760, viewScale = 1;
Rect originalPanel{}, outputPanel{};

Rect fit(Rect area, int width, int height) {
    float scale = min(area.w / width, area.h / height);
    float w = width * scale, h = height * scale;
    return {area.x + (area.w - w) / 2, area.y + (area.h - h) / 2, w, h};
}
void vertices(Rect box) {
    glVertex2f(box.x, box.y);
    glVertex2f(box.x + box.w, box.y);
    glVertex2f(box.x + box.w, box.y + box.h);
    glVertex2f(box.x, box.y + box.h);
}
void fill(Rect box, RGB color) {
    glColor3ubv(color.data());
    glBegin(GL_QUADS);
    vertices(box);
    glEnd();
}
void outline(Rect box, RGB color, float width = 1) {
    glLineWidth(max(1.0f, width * viewScale));
    glColor3ubv(color.data());
    glBegin(GL_LINE_LOOP);
    vertices(box);
    glEnd();
    glLineWidth(1);
}
void mark(Rect box) {
    // The dark rim and light inner line are visible on bright and dark artwork.
    outline(box, well, 5);
    outline(box, gold, 2);
    if (min(box.w, box.h) < 5)
        outline({box.x - 3, box.y - 3, box.w + 6, box.h + 6}, gold);
}

void* fittingFont(const string& value, bool large, float room) {
    auto chars = reinterpret_cast<const unsigned char*>(value.c_str());
    if (large && viewScale >= 0.91f && glutBitmapLength(GLUT_BITMAP_HELVETICA_18, chars) <= room * viewScale)
        return GLUT_BITMAP_HELVETICA_18;
    if (glutBitmapLength(GLUT_BITMAP_HELVETICA_12, chars) <= room * viewScale)
        return GLUT_BITMAP_HELVETICA_12;
    return GLUT_BITMAP_TIMES_ROMAN_10;
}
float textWidth(const string& value, bool large = false, float room = 10000) {
    auto chars = reinterpret_cast<const unsigned char*>(value.c_str());
    return min(room, glutBitmapLength(fittingFont(value, large, room), chars) / viewScale);
}
void label(float x, float baseline, const string& value, RGB color = ink, bool large = false,
           float room = 10000) {
    if (value.empty()) return;
    auto chars = reinterpret_cast<const unsigned char*>(value.c_str());
    glColor3ubv(color.data());
    void* font = fittingFont(value, large, room);
    if (viewScale >= 0.62f && glutBitmapLength(font, chars) <= room * viewScale) {
        glRasterPos2f(round(x), round(baseline));
        glutBitmapString(font, chars);
    } else {
        // Preserve navigation even in a window too small for the fixed bitmap fonts.
        float width = glutStrokeLengthf(GLUT_STROKE_ROMAN, chars);
        if (width == 0) return;
        glEnable(GL_LINE_SMOOTH);
        glPushMatrix();
        glTranslatef(x, baseline, 0);
        glScalef(textWidth(value, large, room) / width, large ? -0.139f : -0.093f, 1);
        glutStrokeString(GLUT_STROKE_ROMAN, chars);
        glPopMatrix();
        glDisable(GL_LINE_SMOOTH);
    }
}
void centered(Rect box, const string& value, RGB color = ink) {
    float room = box.w - 6;
    label(box.x + (box.w - textWidth(value, false, room)) / 2, box.y + box.h / 2 + 4,
          value, color, false, room);
}

enum class Action { Stage, Size, Sampling, Mode, Palette, Grid, Open, Save, Pixel, Tile, ShowBlend };
struct Button { Rect box; Action action; int value; };
vector<Button> buttons;
struct PatchTarget { Rect box{}; Block pixels{0, 0, 1, 1}; bool enabled = false; };
PatchTarget patchTarget;

void button(Rect box, const string& title, Action action, int value = 0,
            bool active = false, bool enabled = true) {
    fill(box, active ? teal : surface);
    outline(box, active ? teal : lineColor);
    centered(box, title, active ? darkInk : enabled ? ink : RGB{125, 139, 149});
    if (enabled) buttons.push_back({box, action, value});
}

float walkTop() { return viewHeight - 302; }

void drawToolbar() {
    label(margin, 29, "PixelCraft", ink, true);
    float x = 118;
    auto control = [&](float width, const string& name, Action action, int value = 0, bool active = false) {
        button({x, 9, width, 28}, name, action, value, active);
        x += width + 5;
    };
    label(x, 27, "Block", muted);
    x += 36;
    control(24, "-", Action::Size, -1);
    centered({x, 9, 42, 28}, to_string(settings.blockSize) + " px");
    x += 47;
    control(24, "+", Action::Size, 1);
    x += 5;
    control(132, settings.average ? "Sample  Average [A]" : "Sample  Center [A]", Action::Sampling);
    control(128, "Mode  " + colorNames[int(settings.color)] + " [C]", Action::Mode);
    control(126, "Palette  " + paletteNames[int(settings.palette)] + " [P]", Action::Palette);
    control(65, "Grid [G]", Action::Grid, 0, showGrid);
    x = max(x + 4, viewWidth - margin - 142);
    control(67, "Open [O]", Action::Open);
    control(70, "Save [S]", Action::Save);
}

void drawRaster(Rect panel, const vector<RGB>& pixels) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glRasterPos2f(panel.x, panel.y);
    glPixelZoom(panel.w / image.width * viewScale, -panel.h / image.height * viewScale);
    glDrawPixels(image.width, image.height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glPixelZoom(1, 1);
}
Rect imageRegion(Rect panel, Block block) {
    float sx = panel.w / image.width, sy = panel.h / image.height;
    return {panel.x + block.x * sx, panel.y + block.y * sy, block.width * sx, block.height * sy};
}
void drawPixelArt() {
    int size = settings.blockSize;
    if (size == 1 || outputPanel.w * size / image.width < 1) {
        drawRaster(outputPanel, artwork);
    } else {
        glBegin(GL_QUADS);
        for (int y = 0; y < image.height; y += size) {
            for (int x = 0; x < image.width; x += size) {
                glColor3ubv(artwork[y * image.width + x].data());
                vertices(imageRegion(outputPanel, blockAt(image, size, {x, y})));
            }
        }
        glEnd();
    }
    if (showGrid) {
        glColor4ub(0, 0, 0, 95);
        glBegin(GL_LINES);
        for (int x = 0; x < image.width; x += size) {
            float at = outputPanel.x + x * outputPanel.w / image.width;
            glVertex2f(at, outputPanel.y); glVertex2f(at, outputPanel.y + outputPanel.h);
        }
        for (int y = 0; y < image.height; y += size) {
            float at = outputPanel.y + y * outputPanel.h / image.height;
            glVertex2f(outputPanel.x, at); glVertex2f(outputPanel.x + outputPanel.w, at);
        }
        glEnd();
    }
}

void drawComparison() {
    float width = (viewWidth - 2 * margin - gap) / 2;
    for (int side = 0; side < 2; ++side) {
        Rect frame{margin + side * (width + gap), 75, width, walkTop() - 86};
        string info = side == 0 ? dimensions(image.width, image.height) + " px" :
            dimensions((image.width + settings.blockSize - 1) / settings.blockSize,
                       (image.height + settings.blockSize - 1) / settings.blockSize) + " blocks";
        label(frame.x, 64, side == 0 ? "Original image" : "Pixel art", ink, true);
        label(frame.x + frame.w - textWidth(info), 63, info, muted);
        fill(frame, well);
        outline(frame, lineColor);
        Rect panel = fit({frame.x + 5, frame.y + 5, frame.w - 10, frame.h - 10}, image.width, image.height);
        if (side == 0) {
            originalPanel = panel;
            drawRaster(panel, image.rgb);
        } else {
            outputPanel = panel;
            drawPixelArt();
        }
        mark(imageRegion(panel, selected));
        if (side == 0 && stage == 1 && !settings.average) {
            Point center = selected.center();
            mark(imageRegion(panel, {center.x, center.y, 1, 1}));
        }
    }
}

// The teaching panel uses the same three columns at each stage.
Rect columnAt(int index) {
    float available = viewWidth - 2 * margin - 2 * gap;
    float first = clamp(available * 0.25f, 218.0f, 310.0f);
    float second = available * 0.333f;
    array<float, 3> widths{first, second, available - first - second};
    float x = margin;
    for (int i = 0; i < index; ++i) x += widths[i] + gap;
    return {x, walkTop() + 81, widths[index], 207};
}
void heading(Rect col, const string& title) {
    label(col.x + 13, col.y + 24, title, teal, true, col.w - 26);
}
void row(Rect col, float offset, const string& value, RGB color = ink, bool large = false) {
    label(col.x + 13, col.y + offset, value, color, large, col.w - 26);
}
void colorChip(Rect col, RGB color, float y = 42) {
    Rect chip{col.x + 13, col.y + y, col.w - 26, 43};
    fill(chip, color);
    outline(chip, lineColor);
    row(col, y + 68, "RGB " + rgbText(color), ink, true);
}

array<int, 4> pngPixel(Point p) {
    const RGB& rgb = image.at(p.x, p.y);
    if (image.rgba.empty()) return {rgb[0], rgb[1], rgb[2], 255};
    const unsigned char* raw = image.rgba.data() + 4 * (p.y * image.width + p.x);
    return {raw[0], raw[1], raw[2], raw[3]};
}
void drawPngPixel(Rect col) {
    Point p = selected.pixel(inspectedPixel);
    auto raw = pngPixel(p);
    const RGB& visible = image.at(p.x, p.y);
    heading(col, "PNG pixel " + pointText(p));
    row(col, 49, "RGBA  (" + to_string(raw[0]) + ", " + to_string(raw[1]) + ", " +
        to_string(raw[2]) + ", " + to_string(raw[3]) + ")");
    if (raw[3] == 255) {
        row(col, 72, "Opacity is 255 / 255. The source RGB passes through.", muted);
    } else {
        int alpha = raw[3];
        row(col, 72, "White share = 255 - " + to_string(alpha) + " = " + to_string(255 - alpha) +
            ". Add 127 to round.", muted);
        for (int c = 0; c < 3; ++c)
            row(col, 96 + 19 * c, channels[c] + "   (" + to_string(raw[c]) + " x " + to_string(alpha) +
                " + 255 x " + to_string(255 - alpha) + " + 127) / 255 = " + to_string(visible[c]));
    }
    fill({col.x + 13, col.y + 158, 26, 22}, visible);
    label(col.x + 48, col.y + 174, "Visible RGB  " + rgbText(visible));
}

int tileColumns() { return (selected.width + tileSize - 1) / tileSize; }
int tileCount() { return tileColumns() * ((selected.height + tileSize - 1) / tileSize); }
int currentTile() {
    Point p = selected.pixel(inspectedPixel);
    return (p.y - selected.y) / tileSize * tileColumns() + (p.x - selected.x) / tileSize;
}
Block tileAt(int index) {
    int dx = (index % tileColumns()) * tileSize;
    int dy = (index / tileColumns()) * tileSize;
    return {selected.x + dx, selected.y + dy, min(tileSize, selected.width - dx),
            min(tileSize, selected.height - dy)};
}
void drawPatch(Rect col, Block patch, bool clickable, bool marker, Point p, bool grid) {
    Rect box = fit({col.x + 13, col.y + 37, col.w - 26, 124}, patch.width, patch.height);
    float cell = box.w / patch.width;
    glBegin(GL_QUADS);
    for (int dy = 0; dy < patch.height; ++dy) {
        for (int dx = 0; dx < patch.width; ++dx) {
            glColor3ubv(image.at(patch.x + dx, patch.y + dy).data());
            vertices({box.x + dx * cell, box.y + dy * cell, cell, cell});
        }
    }
    glEnd();
    if (grid && cell >= 5) {
        glColor4ub(12, 19, 25, 60);
        glBegin(GL_LINES);
        for (int i = 1; i < patch.width; ++i) {
            float x = box.x + i * cell;
            glVertex2f(x, box.y); glVertex2f(x, box.y + box.h);
        }
        for (int i = 1; i < patch.height; ++i) {
            float y = box.y + i * cell;
            glVertex2f(box.x, y); glVertex2f(box.x + box.w, y);
        }
        glEnd();
    }
    outline(box, lineColor);
    if (marker && patch.contains(p))
        mark({box.x + (p.x - patch.x) * cell, box.y + (p.y - patch.y) * cell, cell, cell});
    if (clickable) patchTarget = {box, patch, true};
}

void drawSource(Rect patch, Rect bounds, Rect pixel) {
    heading(patch, "Source patch");
    drawPatch(patch, selected, true, true, selected.pixel(inspectedPixel), true);
    row(patch, 189, "Click a pixel to see its PNG values.", muted);
    heading(bounds, "Which pixels belong?");
    string size = to_string(settings.blockSize);
    row(bounds, 49, "Location " + pointText(anchor) + " on the original", muted);
    row(bounds, 72, "Left  =  floor(" + to_string(anchor.x) + " / " + size + ") x " + size + " = " + to_string(selected.x));
    row(bounds, 93, "Top  =  floor(" + to_string(anchor.y) + " / " + size + ") x " + size + " = " + to_string(selected.y));
    row(bounds, 117, "Bottom right  " + pointText(selected.pixel(selected.count() - 1)) + "  (inclusive)");
    row(bounds, 149, dimensions(selected.width, selected.height) + " = " +
        to_string(selected.count()) + (selected.count() == 1 ? " pixel" : " pixels"), ink, true);
    bool edge = selected.width < settings.blockSize || selected.height < settings.blockSize;
    row(bounds, 178, edge ? "The block stops at the image edge." : "The outlines mark these source coordinates.", muted);
    drawPngPixel(pixel);
}

void drawSample(Rect patch, Rect detail, Rect sample) {
    bool average = settings.average;
    Point p = average ? selected.pixel(inspectedPixel) : selected.center();
    int tiles = tileCount();
    heading(patch, average ? "Contributing pixels" : "Center pixel");
    drawPatch(patch, average ? tileAt(currentTile()) : selected, average, true, p, true);
    if (average && tiles > 1) {
        int tile = currentTile();
        float y = patch.y + 172;
        button({patch.x + 8, y, 65, 25}, "Prev tile", Action::Tile, -1, false, tile > 0);
        centered({patch.x + 75, y, patch.w - 150, 25}, to_string(tile + 1) + " / " + to_string(tiles));
        button({patch.x + patch.w - 73, y, 65, 25}, "Next tile", Action::Tile, 1, false, tile + 1 < tiles);
    } else {
        row(patch, 189, average ? "Click a pixel or step in reading order." : "The outlined pixel supplies the color.", muted);
    }

    const RGB& rgb = image.at(p.x, p.y);
    if (average) {
        heading(detail, "Add pixel " + to_string(inspectedPixel + 1) + " of " + to_string(selected.count()));
        row(detail, 50, "Source " + pointText(p) + "    RGB " + rgbText(rgb), muted);
        for (int c = 0; c < 3; ++c)
            row(detail, 77 + 21 * c, channels[c] + "    " + to_string(prefix[inspectedPixel][c]) + " + " +
                to_string(rgb[c]) + " = " + to_string(prefix[inspectedPixel + 1][c]));
        button({detail.x + 13, detail.y + 133, 99, 26}, "Previous Pixel", Action::Pixel, -1,
               false, inspectedPixel > 0);
        button({detail.x + 119, detail.y + 133, 88, 26}, "Next Pixel", Action::Pixel, 1,
               false, inspectedPixel + 1 < selected.count());
        heading(sample, "Average the whole block");
        for (int c = 0; c < 3; ++c)
            row(sample, 53 + 21 * c, channels[c] + "    " + to_string(result.totals[c]) + " / " +
                to_string(selected.count()) + " = " + to_string(result.sampled[c]));
        row(sample, 118, "Integer division drops the remainder.", muted);
        colorChip(sample, result.sampled, 132);
    } else {
        heading(detail, "Locate the center");
        row(detail, 59, "x = " + to_string(selected.x) + " + floor(" + to_string(selected.width) +
            " / 2) = " + to_string(p.x));
        row(detail, 85, "y = " + to_string(selected.y) + " + floor(" + to_string(selected.height) +
            " / 2) = " + to_string(p.y));
        row(detail, 124, "Read the source RGB at " + pointText(p) + ".", muted);
        heading(sample, "Sampled color");
        colorChip(sample, result.sampled);
        row(sample, 150, "One source pixel supplies this block.");
    }
    if (pngPixel(p)[3] < 255)
        button({detail.x + 13, detail.y + 170, 155, 25}, "Show PNG white blend", Action::ShowBlend);
}

void drawGrayMath(Rect col, RGB input, int gray) {
    row(col, 50, "Weighted brightness", muted);
    row(col, 75, "77 x " + to_string(input[0]) + " + 150 x " + to_string(input[1]) +
        " + 29 x " + to_string(input[2]) + " + 128");
    row(col, 99, to_string(grayNumerator(input)) + " / 256 = " + to_string(gray));
}

void drawColorMode(Rect input, Rect math, Rect output) {
    heading(input, "Sampled color");
    colorChip(input, result.sampled);
    heading(math, colorNames[int(settings.color)]);
    if (settings.color == ColorMode::Color) {
        row(math, 54, "Carry each channel forward.", muted);
        for (int c = 0; c < 3; ++c)
            row(math, 83 + 23 * c, channels[c] + "    " + to_string(result.sampled[c]) +
                "  ->  " + to_string(result.afterMode[c]));
    } else {
        drawGrayMath(math, result.sampled, result.gray);
        if (settings.color == ColorMode::BlackWhite)
            row(math, 140, to_string(result.gray) + (result.gray >= 128 ? " >= 128 gives white (255)." :
                                                                          " < 128 gives black (0)."));
        else
            row(math, 140, "Use this brightness in all three channels.");
    }
    heading(output, "After color mode");
    colorChip(output, result.afterMode);
    row(output, 150, "This RGB goes into the palette stage.", muted);
}

void drawPalette(Rect input, Rect math, Rect output) {
    heading(input, "After color mode");
    colorChip(input, result.afterMode);
    heading(math, paletteNames[int(settings.palette)] + " palette");
    if (settings.palette == Palette::Retro64) {
        row(math, 50, "Channel / 64 gives a bin from 0 to 3.", muted);
        for (int c = 0; c < 3; ++c)
            row(math, 80 + 24 * c, channels[c] + "   " + to_string(result.afterMode[c]) + " / 64 = " +
                to_string(result.retroBin[c]) + "     " + to_string(result.retroBin[c]) +
                " x 85 = " + to_string(result.finalColor[c]));
        row(math, 163, "4 levels per channel give 64 colors.", muted);
    } else if (settings.palette == Palette::GameBoy) {
        drawGrayMath(math, result.afterMode, result.paletteGray);
        int low = result.shade * 64;
        row(math, 139, to_string(result.paletteGray) + " falls in " + to_string(low) + " to " +
            to_string(low + 63) + ".");
        row(math, 165, "Choose shade " + to_string(result.shade + 1) + " of 4.");
    } else {
        row(math, 56, "The palette is off.");
        row(math, 82, "Pass the color-mode RGB straight through.", muted);
    }
    heading(output, "Block color");
    colorChip(output, result.finalColor);
    if (settings.palette == Palette::GameBoy) {
        float size = min(50.0f, (output.w - 45) / 4);
        for (int i = 0; i < 4; ++i) {
            Rect shade{output.x + 13 + i * (size + 5), output.y + 136, size, 29};
            fill(shade, greenShades[i]);
            if (i == result.shade) mark(shade);
        }
        row(output, 189, "Darkest to lightest. The outline marks the chosen shade.", muted);
    }
}

void drawFill(Rect patch, Rect output, Rect neighbors) {
    heading(patch, "Original patch");
    drawPatch(patch, selected, false, false, {}, false);
    row(patch, 189, dimensions(selected.width, selected.height) + " source pixels", muted);
    heading(output, "Fill with one color");
    Rect block = fit({output.x + 13, output.y + 37, output.w - 26, 124}, selected.width, selected.height);
    fill(block, result.finalColor);
    outline(block, lineColor);
    row(output, 180, "RGB " + rgbText(result.finalColor) + " fills " + to_string(selected.count()) +
        (selected.count() == 1 ? " pixel." : " pixels."));
    row(output, 198, "The saved PNG uses this same fill.", muted);
    heading(neighbors, "Where the block lands");
    float cell = 36, x = neighbors.x + 14, y = neighbors.y + 41;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int bx = selected.x + dx * settings.blockSize, by = selected.y + dy * settings.blockSize;
            Rect tile{x + (dx + 1) * cell, y + (dy + 1) * cell, cell, cell};
            bool inside = bx >= 0 && by >= 0 && bx < image.width && by < image.height;
            fill(tile, inside ? artwork[by * image.width + bx] : well);
            outline(tile, lineColor);
        }
    }
    mark({x + cell, y + cell, cell, cell});
    row(neighbors, 180, "The center is the selected block in the pixel art.", muted);
}

void drawWalkthrough() {
    float top = walkTop();
    label(margin, top + 22, "How one block gets its color", ink, true);
    label(margin + 263, top + 22, status.empty() ? "Click either image to choose a block." : status, muted);
    label(viewWidth - margin - 230, top + 22, "Step " + to_string(stage + 1) + " of 5", muted);
    button({viewWidth - margin - 154, top + 2, 72, 27}, "< Back", Action::Stage, stage - 1, false, stage > 0);
    button({viewWidth - margin - 76, top + 2, 76, 27}, "Next >", Action::Stage, stage + 1, false, stage < 4);
    float tabWidth = (viewWidth - 2 * margin - 4 * 5) / 5;
    for (int i = 0; i < 5; ++i)
        button({margin + i * (tabWidth + 5), top + 39, tabWidth, 31},
               to_string(i + 1) + "   " + stages[i], Action::Stage, i, stage == i);
    Rect left = columnAt(0), middle = columnAt(1), right = columnAt(2);
    for (Rect col : {left, middle, right}) fill(col, surface);
    switch (stage) {
        case 0: drawSource(left, middle, right); break;
        case 1: drawSample(left, middle, right); break;
        case 2: drawColorMode(left, middle, right); break;
        case 3: drawPalette(left, middle, right); break;
        case 4: drawFill(left, middle, right); break;
    }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    buttons.clear();
    patchTarget.enabled = false;
    drawToolbar();
    drawComparison();
    drawWalkthrough();
    glutSwapBuffers();
}

void act(Action action, int amount = 0) {
    Point oldPixel = selected.pixel(inspectedPixel);
    status.clear();
    switch (action) {
        case Action::Stage:
            stage = clamp(amount, 0, 4);
            return;
        case Action::Pixel:
            inspectedPixel = clamp(inspectedPixel + amount, 0, selected.count() - 1);
            return;
        case Action::Tile: {
            Block tile = tileAt(clamp(currentTile() + amount, 0, tileCount() - 1));
            inspectedPixel = (tile.y - selected.y) * selected.width + tile.x - selected.x;
            return;
        }
        case Action::ShowBlend:
            if (!settings.average) {
                Point p = selected.center();
                inspectedPixel = (p.y - selected.y) * selected.width + p.x - selected.x;
            }
            stage = 0;
            return;
        case Action::Grid:
            showGrid = !showGrid;
            return;
        case Action::Save:
            saveImage();
            return;
        case Action::Open: {
            string path = chooseImagePath();
            if (!path.empty() && !loadImage(path)) status = "Open failed. Previous image kept.";
            return;
        }
        case Action::Size:
            settings.blockSize = clamp(settings.blockSize + amount, 1, 128);
            break;
        case Action::Sampling:
            settings.average = !settings.average;
            break;
        case Action::Mode:
            settings.color = static_cast<ColorMode>((int(settings.color) + 1) % 3);
            break;
        case Action::Palette:
            settings.palette = static_cast<Palette>((int(settings.palette) + 1) % 3);
            break;
    }
    // Only processing settings invalidate the artwork. A step, click or resize is cheap.
    artwork = makeArtwork(image, settings);
    refreshSelection(oldPixel);
}

void keyboard(unsigned char key, int, int) {
    switch (key) {
        case '+': case '=': act(Action::Size, 1); break;
        case '-': case '_': act(Action::Size, -1); break;
        case 'a': case 'A': act(Action::Sampling); break;
        case 'c': case 'C': act(Action::Mode); break;
        case 'p': case 'P': act(Action::Palette); break;
        case 'g': case 'G': act(Action::Grid); break;
        case 'o': case 'O': act(Action::Open); break;
        case 's': case 'S': act(Action::Save); break;
        case 27: case 'q': case 'Q': glutLeaveMainLoop(); return;
        default: return;
    }
    glutPostRedisplay();
}

Point pointIn(Rect area, Block pixels, float x, float y) {
    int dx = clamp(int((x - area.x) * pixels.width / area.w), 0, pixels.width - 1);
    int dy = clamp(int((y - area.y) * pixels.height / area.h), 0, pixels.height - 1);
    return {pixels.x + dx, pixels.y + dy};
}
void mouse(int key, int state, int screenX, int screenY) {
    if (key != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;
    float x = screenX / viewScale, y = screenY / viewScale;
    for (Button control : buttons) {
        if (control.box.contains(x, y)) {
            act(control.action, control.value);
            glutPostRedisplay();
            return;
        }
    }
    if (patchTarget.enabled && patchTarget.box.contains(x, y)) {
        Point p = pointIn(patchTarget.box, patchTarget.pixels, x, y);
        inspectedPixel = (p.y - selected.y) * selected.width + p.x - selected.x;
    } else {
        for (Rect panel : {originalPanel, outputPanel}) {
            if (panel.contains(x, y)) {
                chooseBlock(pointIn(panel, {0, 0, image.width, image.height}, x, y));
                status.clear();
                break;
            }
        }
    }
    glutPostRedisplay();
}

void reshape(int w, int h) {
    w = max(w, 1);
    h = max(h, 1);
    // Fit the controls as well as the images when the window gets smaller.
    viewScale = min({1.0f, w / 960.0f, h / 610.0f});
    viewWidth = w / viewScale;
    viewHeight = h / viewScale;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, viewWidth, viewHeight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    cout << "PixelCraft - PNG input only.\n";
    while (image.rgb.empty()) {
        string path = readPath();
        if (path.empty()) return 1;
        loadImage(path);
    }
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(min(1200, glutGet(GLUT_SCREEN_WIDTH) - 36),
                       min(760, glutGet(GLUT_SCREEN_HEIGHT) - 85));
    glutCreateWindow("PixelCraft");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glClearColor(background[0] / 255.f, background[1] / 255.f, background[2] / 255.f, 1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutMainLoop();
    return 0;
}
