# PixelCraft

A small C++ and OpenGL program that turns images (PNG, JPG, JPEG, BMP) into colored blocks. It shows the original and the result side by side.

## Run on this computer

Double-click `run.bat`, then press Enter at the console prompt to open `sample.png`. To use another image at startup, type its full path (PNG, JPG, BMP) and press Enter. Paths in double quotes work too. Keep the console open while the application runs.

If the program is not built yet, `run.bat` builds it. To build manually, run `build.bat` from this folder. The build requires a Windows C++17 compiler and FreeGLUT headers, library and DLL. On this computer these are supplied by `C:\mingw64`; `C:\mingw64\bin` must be on PATH. A different computer needs the same compiler/FreeGLUT setup, including `libfreeglut.dll` on PATH or beside the executable.

## Controls

| Control | Action |
|---|---|
| `+` or `=` / `-` | Increase / decrease block size (1–128 source pixels) |
| `G` | Show or hide the block grid |
| `A` | Toggle sampling mode: Center pixel (nearest) vs. Box Average (area mean) |
| `P` | Cycle palette modes: Off -> Retro 64-Color (Uniform Quantization) -> Game Boy (4-shade green) |
| `C` | Cycle through color, grayscale, and black-and-white (128 threshold) modes |
| `[` / `]` | Decrease / increase image intensity (20% to 250%) |
| `V` or Space | Run animated step-by-step pixelation simulation with scanline beam |
| Click result | Inspect the sampled source position and RGB color; the small 3×3 view shows neighboring blocks |
| `O` | Choose another image (PNG, JPG, JPEG, BMP) in the file picker |
| `S` | Save `pixelcraft-output.png` in this folder, replacing any earlier output |
| Esc or `Q` | Quit |

The exported PNG has the input dimensions and does not include the grid or inspector. Transparent input is composited against white. Maximum input dimensions are 4096 × 4096; for smooth display, use a smaller image or a larger block size. If opening another file fails, the previous image stays loaded. If startup loading fails, enter another path or press Ctrl+C to quit.

## How it works

The image is stored as rows of RGB bytes. The program visits rectangular blocks using two loops. For each block, it can either sample the source pixel nearest its center or compute the average RGB color of all pixels in the block (Box filter downsampling), and draws a rectangle of that color. Partial blocks use the actual remaining area. The color can further be adjusted for intensity, converted to grayscale/binary, or quantized using a Retro 64-color palette or a classic Game Boy 4-shade green palette. The interactive simulation mode (`V`/Space) visualizes how blocks are traversed and rendered row by row. The same sampling, intensity, color mode, and palette rules fill the saved PNG. OpenGL draws the images, rectangles, grid, scanline, and labels; FreeGLUT handles the window, timer, and input.

This implements core Image Processing and Computer Graphics concepts: spatial downsampling (Center vs. Box Average filtering), color quantization (2-bit uniform quantization and luminance palette mapping), intensity adjustment, algorithm visualization / progressive simulation, and color conversions (weighted grayscale and thresholded binarization). It does not include dithering or full-image zoom and panning.

## Files and credits

- `PixelCraft.cpp`: application code.
- `build.bat`, `run.bat`: build and launch commands.
- `sample.png`: original sample illustration supplied with this project.
- `VIVA_GUIDE.md`, `DEMO_SCRIPT.md`, `PixelCraft_Slides.pptx`: presentation materials.
- `PixelCraft_Demo_Silent.mp4`: a short window-only visual recording; add your own narration. It does not show the save action.
- `third_party/stb_image.h` and `stb_image_write.h`: image input/output by Sean Barrett and contributors, used under the licenses included in those headers. FreeGLUT is a separate dependency.
