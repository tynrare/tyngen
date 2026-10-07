# Pixelprocess 
 standalone collections of piped executables for processing ai generated images into pixel art.

- Each pipline stage represended by it own source file and executable
- Each stage takes one or more images, and zero or more text data files.
- Each stage outputs one or more images, and one or more text data files
- First input/output image is main processing unit. E.g. magenta removal or lattice fit
- Second input/output image artifact is debug view
- Text files contains all detailed metadata possible + global mutable state
- This is reimplementation of process-ai-pixel with C and opencv (and any required additional libraries). Keep dependencies list minimal and lightweight.
- Reimplementation has to be applied with care. Lots of features in process-ai-pixel a wrong or implemented badly.
- No compatability, API, or anything from process-ai-pixel needs to be preserved except core idea and some fine-tunings
- This is full replacement of process-ai-pixel for aigen ui shell
- This should introduce some new features for ui: such reacher debug views, overlays; and fine-tuned processings runs for each individual sprite or/and stage
- Mostly this expected to be processing unit implementation, with minor UI wiring, decoupling ui from old process-ai-pixel

# stages

Some of stages required in loose order:

- Blob detection, blob ownershit, spritesheet cells detection
- Per-sprite lattice cell build, pixel reconstruction
- Magenta cleanup
- Spritesheet rebuild 

New version should process each sprite individually after blobs detection, and build spritesheet back after processing. This approach will be better for lattice construction and faster for per-sprite adaptations