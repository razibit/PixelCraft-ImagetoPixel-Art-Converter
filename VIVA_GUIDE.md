# PixelCraft viva guide

On the opening screen, point to the two gold outlines. Each encloses the same part of the image. The left contains up to 16 × 16 individual pixels. The right fills that area with one RGB color. The panel underneath shows where that color came from.

## Follow the sample roof block

| Stage | What to show |
|---|---|
| Source block | The block runs from (160, 96) to (175, 111), inclusive. It contains 16 × 16 = 256 pixels. Coordinates start at the top left. Click the enlarged patch for the selected pixel's PNG values. |
| Sample color | Red, green, and blue sum to 42,216, 32,166, and 30,493. Divide each by 256 using integer division to get (164, 125, 119). Click a pixel to see how it contributes. Switch to Center to inspect (168, 104), which gives (186, 99, 85). |
| Color mode | Return to Average. Full color carries (164, 125, 119) forward. Grayscale computes (77 × 164 + 150 × 125 + 29 × 119 + 128) / 256 = 136. Black-and-white compares 136 to 128 and assigns 255 to all three channels. |
| Palette | With Average and Full color, Retro 64 maps (164, 125, 119) to (170, 85, 85). Game Boy computes brightness 136 and selects its third shade, (139, 172, 15), from the 128–191 range. |
| Fill block | The program writes the final RGB into each of the 256 output positions. The outlined center of the 3 × 3 neighborhood shows where that color lands. |

## Read the source in this order

1. Start in `main()` in `PixelCraft.cpp`. It reads the startup path, calls `loadImage`, creates a double-buffered RGB window and registers the display, resize, keyboard and mouse callbacks. `glutMainLoop` waits for those events. Esc or Q calls `glutLeaveMainLoop`. Closing the window also returns control to `main`.
2. In `BlockProcessing.h`, look at `Image`, `Point`, `Block`, and `Settings`. `image.rgb[y * width + x]` is one source RGB triplet. A block records its origin and actual width and height. `blockAt` snaps a source coordinate to its block and clips the right and bottom edges.
3. Return to `loadImage`. It checks the dimensions, asks stb_image for four channels, then calls `onWhite` for each red, green, or blue byte. For channel value c and opacity a, the visible value is `(c*a + 255*(255-a) + 127) / 255` with integer division. Zero opacity gives white. The program keeps raw RGBA when an image uses transparency so the walkthrough can explain the blend for an individual pixel.
4. Read `calculateBlock`. Average accumulates each RGB channel in row order and divides by the actual pixel count. Center reads the pixel at `(x + width/2, y + height/2)`. Color mode runs next, then the palette. The returned `BlockResult` contains the input and output of each of these steps. For the selected Average block, an optional list records totals before and after each source pixel.
5. Read `makeArtwork`. For each block it calls `calculateBlock` and writes the final RGB across the corresponding output rows. Changing a processing setting rebuilds this buffer. `saveImage` gives the same buffer to the PNG writer, so the exported image contains only the artwork.
6. Look at `chooseBlock` and `refreshSelection`. The anchor remembers a source location, which keeps the two outlines on the same subject when block size changes. A separate pixel index selects a row in the running totals. `examplePoint` favors a mixed-color block near the image center on load.
7. Read `reshape`, `display`, and `drawComparison`. The viewport and orthographic projection give OpenGL a top-left coordinate system. A fitted rectangle preserves each image's proportions. The source uses tightly packed RGB rows and negative vertical pixel zoom. The result normally uses colored OpenGL quads. For single-pixel blocks or blocks smaller than a display pixel, it draws the same artwork buffer directly. Selection outlines and grid lines are drawn afterward.
8. Follow `drawWalkthrough` into its five stage functions. `mouse` maps either image to source coordinates and the detail patch to a pixel index. The rectangles used to draw the buttons also define their click targets. Buttons and keyboard shortcuts call `act`. `glutPostRedisplay` requests a frame and `glutSwapBuffers` presents the completed drawing.

## Questions to practice

**Why do the partial totals change when I click a different pixel?** Each pixel has a position in row order. The walkthrough shows the sum before that position, the selected RGB, and the sum after it. The completed average still uses every pixel in the block.

**What happens at the border?** If only 3 columns and 5 rows remain, the block contains 15 pixels. Average divides by 15 and Center uses the center of those 3 × 5 pixels. The saved image keeps its original dimensions.

**Why do full color and a palette both have stages?** They act in order. Grayscale computes weighted brightness, and black-and-white applies a threshold of 128. Retro 64 reduces each incoming channel to 0, 85, 170, or 255. Game Boy maps incoming brightness into one of four green shades. The source patches and equations show the order for any combination.

**Which CGIP concepts can I demonstrate?** Spatial sampling, RGB and opacity, grayscale, thresholding, color quantization, and mapping source coordinates to OpenGL primitives. A useful demonstration is to switch between Center and Average on the same high-contrast edge and explain the two results.
