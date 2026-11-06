#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"
#include <GL/freeglut.h>
#include <windows.h>
#include <commdlg.h>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Pixels are stored row by row, from the top left, with three bytes per pixel.
struct Image { int width = 0, height = 0; std::vector<unsigned char> rgb; };
Image image;
int windowWidth = 1200, windowHeight = 760, blockSize = 16;
bool showGrid = false;
int selectedX = -1, selectedY = -1;
std::string message = "Click a block to inspect it.";
struct Panel { float x, y, width, height, scale; };
Panel originalPanel{}, pixelPanel{};

std::string readPath() {
    std::cout << "\nPNG path (Enter for sample.png): " << std::flush;
    std::string path;
    if (!std::getline(std::cin, path)) return "";
    if (path.empty()) return "sample.png";
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"')
        path = path.substr(1, path.size() - 2);
    return path;
}

std::string chooseImagePath() {
    char path[32768] = {};
    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter = "PNG images (*.png)\0*.png\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = sizeof(path);
    dialog.lpstrTitle = "Open PNG image";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameA(&dialog) ? path : "";
}

bool loadImage(const std::string& path) {
    int w, h, channels;
    unsigned char* rgba = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!rgba) { std::cerr << "Could not open PNG: " << path << "\n"; return false; }
    // Bound memory and drawing time for unusually large images.
    if (w > 4096 || h > 4096) {
        std::cerr << "Image must be at most 4096 x 4096 pixels.\n";
        stbi_image_free(rgba); return false;
    }
    Image next; next.width = w; next.height = h;
    next.rgb.resize(static_cast<size_t>(w) * h * 3);
    for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
        unsigned a = rgba[i * 4 + 3];
        for (int c = 0; c < 3; ++c)
            next.rgb[i * 3 + c] = static_cast<unsigned char>((rgba[i * 4 + c] * a + 255 * (255 - a) + 127) / 255);
    }
    stbi_image_free(rgba);
    image = std::move(next); selectedX = selectedY = -1;
    message = "Image loaded. Click a block to inspect it.";
    std::cout << "Loaded " << path << " (" << w << " x " << h << ")\n";
    return true;
}

// A partial block at the right or bottom uses the center of its actual area.
void samplePosition(int bx, int by, int& sx, int& sy) {
    int right = std::min(bx + blockSize, image.width);
    int bottom = std::min(by + blockSize, image.height);
    sx = bx + (right - bx) / 2; sy = by + (bottom - by) / 2;
}
const unsigned char* colorAt(int x, int y) {
    return &image.rgb[(static_cast<size_t>(y) * image.width + x) * 3];
}
void rectangle(float x, float y, float w, float h) {
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
}
void text(float x, float y, const std::string& value, void* font = GLUT_BITMAP_HELVETICA_18) {
    glRasterPos2f(x, y);
    for (unsigned char c : value) glutBitmapCharacter(font, c);
}
void layout() {
    float panelWidth = std::max(1.0f, (windowWidth - 90.0f) / 2.0f);
    float panelHeight = std::max(1.0f, windowHeight - 300.0f);
    float scale = std::min(panelWidth / image.width, panelHeight / image.height);
    float w = image.width * scale, h = image.height * scale;
    originalPanel = {30 + (panelWidth - w) / 2, 100 + (panelHeight - h) / 2, w, h, scale};
    pixelPanel = {60 + panelWidth + (panelWidth - w) / 2, 100 + (panelHeight - h) / 2, w, h, scale};
}
void drawOriginal() {
    // PNG rows run downward, so a negative vertical zoom displays them upright.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glRasterPos2f(originalPanel.x, originalPanel.y);
    glPixelZoom(originalPanel.scale, -originalPanel.scale);
    glDrawPixels(image.width, image.height, GL_RGB, GL_UNSIGNED_BYTE, image.rgb.data());
    glPixelZoom(1, 1);
}
void drawPixelArt() {
    glBegin(GL_QUADS);
    for (int y = 0; y < image.height; y += blockSize)
        for (int x = 0; x < image.width; x += blockSize) {
            int sx, sy; samplePosition(x, y, sx, sy);
            glColor3ubv(colorAt(sx, sy));
            rectangle(pixelPanel.x + x * pixelPanel.scale, pixelPanel.y + y * pixelPanel.scale,
                (std::min(x + blockSize, image.width) - x) * pixelPanel.scale,
                (std::min(y + blockSize, image.height) - y) * pixelPanel.scale);
        }
    glEnd();
    if (showGrid) {
        glColor3f(0.1f, 0.1f, 0.1f); glBegin(GL_LINES);
        for (int x = 0; x < image.width; x += blockSize) {
            float px = pixelPanel.x + x * pixelPanel.scale;
            glVertex2f(px, pixelPanel.y); glVertex2f(px, pixelPanel.y + pixelPanel.height);
        }
        for (int y = 0; y < image.height; y += blockSize) {
            float py = pixelPanel.y + y * pixelPanel.scale;
            glVertex2f(pixelPanel.x, py); glVertex2f(pixelPanel.x + pixelPanel.width, py);
        }
        glEnd();
    }
}
void drawInspector() {
    float top = static_cast<float>(windowHeight - 165);
    glColor3f(0.9f, 0.92f, 0.95f);
    if (selectedX < 0) { text(30, top + 25, message); return; }
    int sx, sy; samplePosition(selectedX, selectedY, sx, sy);
    const unsigned char* c = colorAt(sx, sy);
    std::ostringstream label;
    label << "Sample: (" << sx << ", " << sy << ")   RGB: (" << int(c[0]) << ", " << int(c[1]) << ", " << int(c[2]) << ")";
    text(30, top + 25, label.str());
    text(30, top + 52, "Zoom: selected block and its neighbors", GLUT_BITMAP_HELVETICA_12);
    // A 3 x 3 neighborhood of pixel-art blocks, with the selected block outlined.
    float left = 410, cell = 35;
    glBegin(GL_QUADS);
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
        int x = selectedX + dx * blockSize, y = selectedY + dy * blockSize;
        if (x < 0 || y < 0 || x >= image.width || y >= image.height) continue;
        int px, py; samplePosition(x, y, px, py); glColor3ubv(colorAt(px, py));
        rectangle(left + (dx + 1) * cell, top + (dy + 1) * cell, cell, cell);
    }
    glEnd();
    glColor3f(1, 1, 1); glLineWidth(2); glBegin(GL_LINE_LOOP);
    rectangle(left + cell, top + cell, cell, cell); glEnd(); glLineWidth(1);
}
void display() {
    glClearColor(0.08f, 0.10f, 0.14f, 1); glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(0.95f, 0.96f, 1); text(30, 30, "PixelCraft - Original and pixel art");
    std::ostringstream controls; controls << "[+/-] Block: " << blockSize << "    [G] Grid    [O] Open    [S] Save PNG    [Esc] Quit";
    text(30, 57, controls.str(), GLUT_BITMAP_HELVETICA_12);
    if (!image.rgb.empty()) { layout(); drawOriginal(); drawPixelArt(); drawInspector(); }
    glutSwapBuffers();
}
bool saveImage() {
    std::vector<unsigned char> result(image.rgb.size());
    for (int y = 0; y < image.height; y += blockSize)
        for (int x = 0; x < image.width; x += blockSize) {
            int sx, sy; samplePosition(x, y, sx, sy); const unsigned char* c = colorAt(sx, sy);
            for (int py = y; py < std::min(y + blockSize, image.height); ++py)
                for (int px = x; px < std::min(x + blockSize, image.width); ++px)
                    for (int k = 0; k < 3; ++k) result[(static_cast<size_t>(py) * image.width + px) * 3 + k] = c[k];
        }
    bool ok = stbi_write_png("pixelcraft-output.png", image.width, image.height, 3, result.data(), image.width * 3) != 0;
    message = ok ? "Saved pixelcraft-output.png (overwrites earlier output)." : "Save failed; check folder permissions.";
    std::cout << message << "\n"; selectedX = selectedY = -1; return ok;
}
void keyboard(unsigned char key, int, int) {
    switch (key) {
    case '+': case '=': blockSize = std::min(blockSize + 1, 128); selectedX = selectedY = -1; break;
    case '-': case '_': blockSize = std::max(blockSize - 1, 1); selectedX = selectedY = -1; break;
    case 'g': case 'G': showGrid = !showGrid; break;
    case 'o': case 'O': { std::string path = chooseImagePath(); if (!path.empty() && !loadImage(path)) message = "Open failed; previous image kept. See console."; break; }
    case 's': case 'S': saveImage(); break;
    case 27: case 'q': case 'Q': glutLeaveMainLoop(); return;
    }
    glutPostRedisplay();
}
void mouse(int button, int state, int x, int y) {
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN || image.rgb.empty()) return;
    if (x < pixelPanel.x || y < pixelPanel.y || x >= pixelPanel.x + pixelPanel.width || y >= pixelPanel.y + pixelPanel.height) return;
    int px = std::min(image.width - 1, static_cast<int>((x - pixelPanel.x) / pixelPanel.scale));
    int py = std::min(image.height - 1, static_cast<int>((y - pixelPanel.y) / pixelPanel.scale));
    selectedX = (px / blockSize) * blockSize; selectedY = (py / blockSize) * blockSize;
    glutPostRedisplay();
}
void reshape(int w, int h) {
    windowWidth = std::max(w, 1); windowHeight = std::max(h, 1);
    glViewport(0, 0, windowWidth, windowHeight);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, windowWidth, windowHeight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glutPostRedisplay();
}
int main(int argc, char** argv) {
    std::cout << "PixelCraft - PNG input only.\n";
    while (image.rgb.empty()) { std::string path = readPath(); if (path.empty()) return 1; loadImage(path); }
    glutInit(&argc, argv); glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(windowWidth, windowHeight); glutCreateWindow("PixelCraft");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glutDisplayFunc(display); glutReshapeFunc(reshape); glutKeyboardFunc(keyboard); glutMouseFunc(mouse);
    glutMainLoop(); return 0;
}
