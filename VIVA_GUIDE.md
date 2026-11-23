# PixelCraft viva guide

## One-minute explanation

PixelCraft loads a PNG and displays it alongside a blocky version. It walks through the source image one block at a time, samples a pixel near the middle of each block, and fills that block with the sampled RGB color. The user can change block size, show the grid, click a block to inspect its source position and color, and save the result. OpenGL draws the visual elements; FreeGLUT manages the window and input.

## Reading order and function connections

1. **Global state and `Image`:** `width`, `height`, and `rgb` describe the image. Each pixel uses three adjacent bytes in row-major order: red, green, then blue. The index is `(y * width + x) * 3`. Other variables track the window, block size, grid, selection and panels.
2. **`main`:** execution starts here. It asks for an image, initializes FreeGLUT, creates a double-buffered RGB window, registers callbacks and enters `glutMainLoop`. The loop calls our functions when events occur. Closing the window or pressing Escape returns from the loop and ends the program.
3. **`readPath`, `chooseImagePath`, and `loadImage`:** startup reads a console path, while the open shortcut uses a Windows file picker. `loadImage` asks stb_image for RGBA pixels, blends each against white using its alpha value, and stores RGB. It builds a temporary image first, so a failed replacement does not destroy the current one.
4. **`samplePosition` and `colorAt`:** the first finds the center of the actual block, including a smaller block at an edge; the second returns its RGB bytes. Drawing, inspecting and saving use these same helpers.
5. **`reshape` and `layout`:** `reshape` sets the viewport and an orthographic projection with the origin at the top left. `layout` calculates two fitted panels while preserving image proportions. FreeGLUT calls `reshape`; `display` calls `layout`.
6. **`display`, `drawOriginal`, and `drawPixelArt`:** FreeGLUT calls `display` when the window needs repainting. It clears the frame, writes labels, calculates the panels, and calls the drawing functions. `glDrawPixels` shows the original with `glPixelZoom` for scaling and vertical orientation. Nested loops choose each block color; `glBegin(GL_QUADS)`, vertices, and `glEnd` draw rectangles. `glutSwapBuffers` presents the completed frame without showing intermediate drawing.
7. **`mouse` and `drawInspector`:** a click inside the result maps screen coordinates back to source coordinates and then to a block. The inspector displays that block's sample coordinates and RGB values and draws its 3×3 block neighborhood. Clicks outside the result do nothing.
8. **`keyboard`:** FreeGLUT calls this on a key press. It adjusts the size, grid, or image, calls `saveImage`, or exits. `glutPostRedisplay` requests a fresh frame after changes.
9. **`saveImage`:** fills a new RGB buffer using the same block samples, then `stbi_write_png` writes it. The on-screen grid and labels are never copied into this buffer.

`rectangle` supplies four vertices, and `text` uses FreeGLUT bitmap characters. `glColor3ubv` supplies a block's RGB color; `glBegin`/`glEnd` group primitives; `glVertex2f` specifies points. `glViewport`, `glOrtho`, and the matrix calls map our 2D coordinates to the window. `glPixelStorei` tells OpenGL that RGB rows are tightly packed.

## Questions a teacher may ask

**What is the difference between Center Sampling and Average Sampling (Box Filter)?**
- **Center sampling (`A` toggle off):** Picks the single pixel at the center of the block. It is very fast, but if the center hits a specular highlight or random noise, the whole block turns that color.
- **Average sampling (`A` toggle on):** Computes the mathematical mean of all pixels inside the block ($\sum \text{RGB} / N$). It acts as an area box downsampling filter, preserving the image's overall color tone and lighting much better.

**How does Color Quantization / Palette Reduction work (`P` key)?**
- **Retro 64-Color:** Uses uniform quantization. Each color channel ($R, G, B$) is reduced from 256 levels (8 bits) to 4 levels (2 bits) via integer division and scaling (`(c / 64) * 85`), producing a constrained $4 \times 4 \times 4 = 64$ color palette.
- **Game Boy 4-Shade Palette:** Computes luminance ($Y = 0.299R + 0.587G + 0.114B$) and maps the result into 4 authentic 1989 Nintendo Game Boy greenish tones (`(15,56,15)`, `(48,98,48)`, `(139,172,15)`, `(155,188,15)`).

**How does Intensity Adjustment work (`[` and `]` keys)?**
It applies point-processing luminance scaling ($P_{out} = \text{clamp}(P_{in} \times \text{intensity}, 0, 255)$), allowing interactive correction of under- or over-exposed images before or after spatial downsampling.

**How does Algorithm Simulation work (`V` / Space key)?**
It progressively traverses the image row by row using an OpenGL timer (`glutTimerFunc`), visualizing the spatial downsampling in real time with an animated scanline beam.

**What image formats are supported?**
Any standard image format—including PNG, JPEG, JPG, and BMP—is supported via `stb_image`.

**Why does the result look blocky?** Each rectangular area is filled with only one color instead of showing every original pixel.

**What happens at the image edge?** The last block can be smaller. Its sample is chosen from the center of that remaining area, so it stays inside the image.

**Does this use Bresenham or dithering?** No. It uses loops, arithmetic, and OpenGL rectangles. It does not implement a named drawing or dithering algorithm.

**What is the CGIP connection?** RGB image representation and spatial sampling are image concepts; OpenGL drawing and coordinate mapping are graphics concepts. This alone does not prove a particular course outcome is satisfied.

**Why can detail disappear?** One sampled color cannot represent all the colors in a block, especially when blocks are large.

**What changed from the submitted proposal?** JPEG/BMP input, palette reduction, weighted grayscale/binary conversion, intensity scaling, and algorithm simulation are now fully implemented as promised in the proposal! Only error-diffusion dithering and arbitrary zooming/panning remain excluded.

## Quick demonstration

Run `run.bat`, press Enter for the sample, use `+` and `-`, toggle `G`, press `A` to show Average vs. Center sampling, press `[` and `]` to adjust intensity, press `V` to trigger the progressive pixelation simulation, press `P` to cycle Retro 64-color and Game Boy palettes, press `C` to cycle color, grayscale, and black-and-white, click a block to inspect, press `S`, and open `pixelcraft-output.png`. The export uses the active sampling, intensity, and palette modes.
