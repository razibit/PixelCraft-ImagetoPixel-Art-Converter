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

using namespace std;

// Pixels are stored row by row, from the top left, with three bytes per pixel.
struct Image { int width = 0, height = 0; vector<unsigned char> rgb; };
Image image;

int windowWidth = 1200, windowHeight = 760, blockSize = 16;
bool showGrid = false;
bool useAverageSampling = false;
enum class ColorMode { Original, Grayscale, BlackAndWhite };
ColorMode colorMode = ColorMode::Original;
enum class PaletteMode { Off, Retro64, GameBoy };
PaletteMode paletteMode = PaletteMode::Off;
int selectedX = -1, selectedY = -1;

string message = "Click a block to inspect it.";

struct Panel { float x, y, width, height, scale; };
Panel originalPanel{}, pixelPanel{};

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
    dialog.lpstrFilter = "PNG images (*.png)\0*.png\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = sizeof(path);
    dialog.lpstrTitle = "Open PNG image";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameA(&dialog) ? path : "";
}

bool loadImage(const string& path) {
    int w, h, channels;

    // Reject oversized images before decoding them into memory.
    if (!stbi_info(path.c_str(), &w, &h, &channels)) {
        cerr << "Could not open PNG: " << path << "\n";
        return false;
    }
    if (w > 4096 || h > 4096) {
        cerr << "Image must be at most 4096 x 4096 pixels.\n";
        return false;
    }

    unsigned char* rgba = stbi_load(path.c_str(), &w, &h, &channels, 4);

    if (!rgba) {
        cerr << "Could not open PNG: " << path << "\n";
        return false; 
    }

    // Bound memory and drawing time for unusually large images.
    if (w > 4096 || h > 4096) {
        cerr << "Image must be at most 4096 x 4096 pixels.\n";
        stbi_image_free(rgba); return false;
    }

    Image next; 
    next.width = w; 
    next.height = h;

    next.rgb.resize(static_cast<size_t>(w) * h * 3);

    for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
        unsigned a = rgba[i * 4 + 3];
        for (int c = 0; c < 3; ++c)
            next.rgb[i * 3 + c] = static_cast<unsigned char>((rgba[i * 4 + c] * a + 255 * (255 - a) + 127) / 255);
    }

    stbi_image_free(rgba);

    image = move(next);
    selectedX = selectedY = -1;

    message = "Image loaded. Click a block to inspect it.";
    cout << "Loaded " << path << " (" << w << " x " << h << ")\n";
    
    return true;
}

// A partial block at the right or bottom uses the center of its actual area.
void samplePosition(int bx, int by, int& sx, int& sy) {
    int right = min(bx + blockSize, image.width);
    int bottom = min(by + blockSize, image.height);
    sx = bx + (right - bx) / 2; sy = by + (bottom - by) / 2;
}

const unsigned char* colorAt(int x, int y) {
    return &image.rgb[(static_cast<size_t>(y) * image.width + x) * 3];
}

void applyPalette(unsigned char rgb[3]) {
    if (paletteMode == PaletteMode::Retro64) {
        rgb[0] = static_cast<unsigned char>((rgb[0] / 64) * 85);
        rgb[1] = static_cast<unsigned char>((rgb[1] / 64) * 85);
        rgb[2] = static_cast<unsigned char>((rgb[2] / 64) * 85);
    } else if (paletteMode == PaletteMode::GameBoy) {
        unsigned gray = (77u * rgb[0] + 150u * rgb[1] + 29u * rgb[2] + 128u) / 256u;
        if (gray < 64) {
            rgb[0] = 15; rgb[1] = 56; rgb[2] = 15;
        } else if (gray < 128) {
            rgb[0] = 48; rgb[1] = 98; rgb[2] = 48;
        } else if (gray < 192) {
            rgb[0] = 139; rgb[1] = 172; rgb[2] = 15;
        } else {
            rgb[0] = 155; rgb[1] = 188; rgb[2] = 15;
        }
    }
}

void filteredColor(const unsigned char* c, unsigned char out[3]) {
    if (colorMode == ColorMode::Original) {
        out[0] = c[0]; out[1] = c[1]; out[2] = c[2];
    } else {
        unsigned gray = (77u * c[0] + 150u * c[1] + 29u * c[2] + 128u) / 256u;
        if (colorMode == ColorMode::BlackAndWhite) gray = gray >= 128u ? 255u : 0u;
        out[0] = out[1] = out[2] = static_cast<unsigned char>(gray);
    }
    applyPalette(out);
}

void getBlockColor(int bx, int by, unsigned char out[3]) {
    int right = min(bx + blockSize, image.width);
    int bottom = min(by + blockSize, image.height);

    if (useAverageSampling) {
        unsigned long sumR = 0, sumG = 0, sumB = 0;
        int count = 0;
        for (int y = by; y < bottom; ++y) {
            for (int x = bx; x < right; ++x) {
                const unsigned char* p = colorAt(x, y);
                sumR += p[0];
                sumG += p[1];
                sumB += p[2];
                count++;
            }
        }
        if (count > 0) {
            unsigned char raw[3] = {
                static_cast<unsigned char>(sumR / count),
                static_cast<unsigned char>(sumG / count),
                static_cast<unsigned char>(sumB / count)
            };
            filteredColor(raw, out);
        } else {
            out[0] = out[1] = out[2] = 0;
        }
    } else {
        int sx, sy;
        samplePosition(bx, by, sx, sy);
        filteredColor(colorAt(sx, sy), out);
    }
}

void rectangle(float x, float y, float w, float h) {
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
}

void text(float x, float y, const string& value, void* font = GLUT_BITMAP_HELVETICA_18) {
    glRasterPos2f(x, y);
    for (unsigned char c : value) glutBitmapCharacter(font, c);
}

void layout() {
    float panelWidth = max(1.0f, (windowWidth - 90.0f) / 2.0f);
    float panelHeight = max(1.0f, windowHeight - 300.0f);
    float scale = min(panelWidth / image.width, panelHeight / image.height);
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
            unsigned char c[3];
            getBlockColor(x, y, c);
            glColor3ubv(c);

            rectangle(pixelPanel.x + x * pixelPanel.scale, pixelPanel.y + y * pixelPanel.scale,
                (min(x + blockSize, image.width) - x) * pixelPanel.scale,
                (min(y + blockSize, image.height) - y) * pixelPanel.scale);
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

    if (selectedX < 0) { text(30, top + 25, message);
        return; 
    }

    unsigned char displayed[3];
    getBlockColor(selectedX, selectedY, displayed);

    ostringstream label;
    if (useAverageSampling) {
        label << "Block: (" << selectedX << ", " << selectedY << ") [Average]   RGB: (" << int(displayed[0]) << ", " << int(displayed[1]) << ", " << int(displayed[2]) << ")";
    } else {
        int sx, sy; samplePosition(selectedX, selectedY, sx, sy);
        label << "Sample: (" << sx << ", " << sy << ") [Center]   RGB: (" << int(displayed[0]) << ", " << int(displayed[1]) << ", " << int(displayed[2]) << ")";
    }
    text(30, top + 25, label.str());
    text(30, top + 52, "Zoom: selected block and its neighbors", GLUT_BITMAP_HELVETICA_12);
    // A 3 x 3 neighborhood of pixel-art blocks, with the selected block outlined.
    float left = 410, cell = 35;

    glBegin(GL_QUADS);
    // Iterate over 3x3 neighborhood around selected block (-1, 0, +1 in both directions)
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            // Calculate pixel coordinates for this neighbor
            int x = selectedX + dx * blockSize;
            int y = selectedY + dy * blockSize;

            // Skip if coordinates are outside image bounds
            if (x < 0 || y < 0 || x >= image.width || y >= image.height) {
                continue;
            }

            // Sample the color at this position
            unsigned char c[3];
            getBlockColor(x, y, c);
            glColor3ubv(c);

            // Draw a cell for this neighbor in the 3x3 grid
            // (dx + 1) and (dy + 1) convert -1..1 range to 0..2 for grid positioning
            rectangle(left + (dx + 1) * cell,
                      top + (dy + 1) * cell,
                      cell,
                      cell);
        }
    }
    glEnd();

    glColor3f(1, 1, 1); 
    glLineWidth(2); 

    glBegin(GL_LINE_LOOP);
    rectangle(left + cell, top + cell, cell, cell); 
    glEnd();
    
    glLineWidth(1);
}

void display() {
    glClearColor(0.08f, 0.10f, 0.14f, 1); 
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(0.95f, 0.96f, 1); 

    text(30, 30, "PixelCraft - Original and pixel art");

    ostringstream controls; 
    controls << "[+/-] Block: " << blockSize << "   [G] Grid   [A] Sample: "
             << (useAverageSampling ? "Average" : "Center")
             << "   [P] Palette: ";
    if (paletteMode == PaletteMode::Off) controls << "Off";
    else if (paletteMode == PaletteMode::Retro64) controls << "Retro 64";
    else controls << "Game Boy";
    controls << "   [C] Mode: ";
    if (colorMode == ColorMode::Original) controls << "Color";
    else if (colorMode == ColorMode::Grayscale) controls << "Grayscale";
    else controls << "B&W";
    controls << "   [O] Open   [S] Save PNG   [Esc] Quit";
    text(30, 57, controls.str(), GLUT_BITMAP_HELVETICA_12);

    if (!image.rgb.empty()) { 
        layout(); 
        drawOriginal(); 
        drawPixelArt(); 
        drawInspector(); }
    glutSwapBuffers();
}

bool saveImage() {
    vector<unsigned char> result(image.rgb.size());
    for (int y = 0; y < image.height; y += blockSize)
        for (int x = 0; x < image.width; x += blockSize) {
            unsigned char converted[3];
            getBlockColor(x, y, converted);

            for (int py = y; py < min(y + blockSize, image.height); ++py)
                for (int px = x; px < min(x + blockSize, image.width); ++px)
                    for (int k = 0; k < 3; ++k) result[(static_cast<size_t>(py) * image.width + px) * 3 + k] = converted[k];
        }
        
    bool ok = stbi_write_png("pixelcraft-output.png", image.width, image.height, 3, result.data(), image.width * 3) != 0;
    message = ok ? "Saved pixelcraft-output.png (overwrites earlier output)." : "Save failed; check folder permissions.";
    cout << message << "\n"; selectedX = selectedY = -1; return ok;
}

void keyboard(unsigned char key, int, int) {
    switch (key) {
    case '+': case '=': blockSize = min(blockSize + 1, 128); selectedX = selectedY = -1; break;
    case '-': case '_': blockSize = max(blockSize - 1, 1); selectedX = selectedY = -1; break;
    case 'g': case 'G': showGrid = !showGrid; break;
    case 'a': case 'A':
        useAverageSampling = !useAverageSampling;
        message = useAverageSampling ? "Sampling: Average (Box Filter)" : "Sampling: Center Pixel";
        selectedX = selectedY = -1;
        break;
    case 'p': case 'P':
        if (paletteMode == PaletteMode::Off) {
            paletteMode = PaletteMode::Retro64;
            message = "Palette: Retro 64-Color (Uniform Quantization)";
        } else if (paletteMode == PaletteMode::Retro64) {
            paletteMode = PaletteMode::GameBoy;
            message = "Palette: Classic Game Boy 4-Shade";
        } else {
            paletteMode = PaletteMode::Off;
            message = "Palette: Off (Full Color)";
        }
        selectedX = selectedY = -1;
        break;
    case 'c': case 'C':
        if (colorMode == ColorMode::Original) colorMode = ColorMode::Grayscale;
        else if (colorMode == ColorMode::Grayscale) colorMode = ColorMode::BlackAndWhite;
        else colorMode = ColorMode::Original;
        selectedX = selectedY = -1;
        break;
    case 'o': case 'O': { string path = chooseImagePath(); if (!path.empty() && !loadImage(path)) message = "Open failed; previous image kept. See console."; break; }
    case 's': case 'S': saveImage(); break;
    case 27: case 'q': case 'Q': glutLeaveMainLoop(); 
    return;
    }
    glutPostRedisplay();
}

void mouse(int button, int state, int x, int y) {
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN || image.rgb.empty()) return;

    if (x < pixelPanel.x || y < pixelPanel.y || x >= pixelPanel.x + pixelPanel.width || y >= pixelPanel.y + pixelPanel.height) return;
    int px = min(image.width - 1, static_cast<int>((x - pixelPanel.x) / pixelPanel.scale));
    int py = min(image.height - 1, static_cast<int>((y - pixelPanel.y) / pixelPanel.scale));

    selectedX = (px / blockSize) * blockSize; 
    selectedY = (py / blockSize) * blockSize;

    glutPostRedisplay();
}

void reshape(int w, int h) {
    windowWidth = max(w, 1); 
    windowHeight = max(h, 1);

    glViewport(0, 0, windowWidth, windowHeight);

    glMatrixMode(GL_PROJECTION); 
    glLoadIdentity(); 
    glOrtho(0, windowWidth, windowHeight, 0, -1, 1);

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
    glutInitWindowSize(windowWidth, windowHeight); 

    glutCreateWindow("PixelCraft");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

    glutDisplayFunc(display); 
    glutReshapeFunc(reshape); 
    glutKeyboardFunc(keyboard); 
    glutMouseFunc(mouse);

    glutMainLoop(); 
    return 0;
}
