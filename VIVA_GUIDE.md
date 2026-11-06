# PixelCraft viva guide

## One-minute explanation

PixelCraft loads a PNG and displays it alongside a blocky version. It walks through the source image one block at a time, samples a pixel near the middle of each block, and fills that block with the sampled RGB color. The user can change block size, show the grid, click a block to inspect its source position and color, and save the result. OpenGL draws the visual elements; FreeGLUT manages the window and input.

## Reading order and function connections

1. **Global state and `Image`:** `width`, `height`, and `rgb` describe the image. Each pixel uses three adjacent bytes in row-major order: red, green, then blue. The index is `(y * width + x) * 3`. Other variables track the window, block size, grid, selection and panels.
2. **`main`:** execution starts here. It asks for an image, initializes FreeGLUT, creates a double-buffered RGB window, registers callbacks and enters `glutMainLoop`. The loop calls our functions when events occur. Closing the window or pressing Escape returns from the loop and ends the program.
3. **`readPath` and `loadImage`:** the first reads a console path; the second asks stb_image for RGBA pixels, blends each against white using its alpha value, and stores RGB. It builds a temporary image first, so a failed replacement does not destroy the current one. `main` and `keyboard` call these functions.
4. **`samplePosition` and `colorAt`:** the first finds the center of the actual block, including a smaller block at an edge; the second returns its RGB bytes. Drawing, inspecting and saving use these same helpers.
5. **`reshape` and `layout`:** `reshape` sets the viewport and an orthographic projection with the origin at the top left. `layout` calculates two fitted panels while preserving image proportions. FreeGLUT calls `reshape`; `display` calls `layout`.
6. **`display`, `drawOriginal`, and `drawPixelArt`:** FreeGLUT calls `display` when the window needs repainting. It clears the frame, writes labels, calculates the panels, and calls the drawing functions. `glDrawPixels` shows the original with `glPixelZoom` for scaling and vertical orientation. Nested loops choose each block color; `glBegin(GL_QUADS)`, vertices, and `glEnd` draw rectangles. `glutSwapBuffers` presents the completed frame without showing intermediate drawing.
7. **`mouse` and `drawInspector`:** a click inside the result maps screen coordinates back to source coordinates and then to a block. The inspector displays that block's sample coordinates and RGB values and draws its 3×3 block neighborhood. Clicks outside the result do nothing.
8. **`keyboard`:** FreeGLUT calls this on a key press. It adjusts the size, grid, or image, calls `saveImage`, or exits. `glutPostRedisplay` requests a fresh frame after changes.
9. **`saveImage`:** fills a new RGB buffer using the same block samples, then `stbi_write_png` writes it. The on-screen grid and labels are never copied into this buffer.

`rectangle` supplies four vertices, and `text` uses FreeGLUT bitmap characters. `glColor3ubv` supplies a block's RGB color; `glBegin`/`glEnd` group primitives; `glVertex2f` specifies points. `glViewport`, `glOrtho`, and the matrix calls map our 2D coordinates to the window. `glPixelStorei` tells OpenGL that RGB rows are tightly packed.

## Questions a teacher may ask

**Why does the result look blocky?** Each rectangular area is filled with only one color instead of showing every original pixel.

**What happens at the image edge?** The last block can be smaller. Its sample is chosen from the center of that remaining area, so it stays inside the image.

**Does this use Bresenham or dithering?** No. It uses loops, arithmetic, and OpenGL rectangles. It does not implement a named drawing or dithering algorithm.

**What is the CGIP connection?** RGB image representation and spatial sampling are image concepts; OpenGL drawing and coordinate mapping are graphics concepts. This alone does not prove a particular course outcome is satisfied.

**Why can detail disappear?** One sampled color cannot represent all the colors in a block, especially when blocks are large.

**What changed from the submitted proposal?** Palette reduction, grayscale, JPEG input, full-image zoom/panning, and algorithm visualization are excluded; optional dithering is excluded too. Ask the teacher to approve the simpler scope.

## Quick demonstration

Run `run.bat`, press Enter for the sample, use `+` and `-`, toggle `G`, click a block, press `S`, and open `pixelcraft-output.png`. Explain that the position and RGB refer to the sampled source pixel. Mention that the export has no grid.
