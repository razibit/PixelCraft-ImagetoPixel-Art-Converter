# PixelCraft

PixelCraft opens a PNG and shows the original beside a blocky version. The walkthrough beneath the images follows one outlined area from its source pixels to the color that fills it.

## Run on Windows

Double-click `run.bat` and press Enter at the console prompt for the bundled `sample.png`. To start with another PNG, type its path. Quoted paths work. The app accepts images up to 4096 × 4096 pixels.

To rebuild after a source edit, run `build.bat`. It uses C++17, OpenGL, and FreeGLUT. This computer has MinGW in `C:\mingw64`. On another computer, make the FreeGLUT headers and linker library available to g++, and put its DLL on PATH or beside the executable.

## Try the worked example

1. The sample opens with 16 × 16 blocks and Average sampling. A gold outline marks the same block on both images. **Source block** enlarges that patch. Click a pixel in it to read its original PNG values, including the blend onto white where the pixel has transparency.
2. Click **Next** to reach **Sample color**. The example at (160, 96) contains 256 pixels. Its red sum is 42,216, so the red output is 164 after integer division by 256. The complete average is **RGB (164, 125, 119)**.
3. Click a pixel or use **Previous Pixel** and **Next Pixel** to inspect its addition. The right column keeps the finished average visible. Blocks larger than 16 pixels on either side offer tile buttons. Pixel steps still follow full image rows across tile boundaries.
4. Click **Sample** in the toolbar to try Center on the same block. The marked source pixel at (168, 104) gives **RGB (186, 99, 85)**. Click **Sample** again to return to Average.
5. Continue through **Color mode**, **Palette**, and **Fill block**. Try a different mode or palette using the toolbar. The last stage shows the enlarged source patch, the final color, and the block among its neighbors.

Click either main image to explore another block. Use the five stage names to jump directly to a calculation. Opening a different PNG starts a fresh example with the current processing settings.

## Controls

| Control | Action |
|---|---|
| Toolbar − / + or keyboard `-` / `+` (`=` also works) | Change block size from 1 to 128 source pixels. The block containing the same source location stays selected. |
| Sample / `A` | Switch between the average and the center source pixel. |
| Mode / `C` | Cycle full color, grayscale, and black-and-white. |
| Palette / `P` | Cycle Off, Retro 64, and Game Boy. |
| Grid / `G` | Toggle output grid lines. |
| Open / `O` | Choose a replacement PNG. A failed load keeps the current image. |
| Save / `S` | Write the artwork at its original dimensions to `pixelcraft-output.png` in the working folder. Repeated saves replace that file. |
| Esc, `Q`, or the window close button | Quit. |

## Code and materials

- `PixelCraft.cpp` loads the PNG, handles FreeGLUT events, draws both images and the walkthrough, and exports the artwork.
- `BlockProcessing.h` contains the block bounds, white blending, sampling, color conversion, and palette math shared by the display and export.
- `VIVA_GUIDE.md` explains the calculations and source reading order. `DEMO_SCRIPT.md` gives a short demonstration using the controls. The existing slides and silent recording show the earlier version.
- `third_party/stb_image.h` and `stb_image_write.h` are by Sean Barrett and contributors. Their licenses are included in the headers. `sample.png` is the original illustration supplied with this project.
