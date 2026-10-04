a. create simple desktop-only raylib app
b. main.c and lib.c/h files for source; vs/fs files for shaders; build.sh, run.sh for application; Clean build in same folder (cache for build is allowed in subfolder). My raylib at ~/code/raylib
c. add simple enet bind, fetch commands periodically. add mcp configuration. accept enet commands same way raylib accepts keys inputs. Configure to codex agent access. Always mirror all controls into agent access.
d. Read /home/x/Pictures/aigen. Especcialy aigen0.sh, aigen1.sh
e. Build GUI interface for/instead aigen cli. Maintain active workspace folder as launch argument, defaulting to `assets`
f. Keep interface minimal. Implement tabs: 
	f.1. prompt view/edit. Reading/editing external file
	f.2. prompt references. Open/drag-n-drop, exclude/hide/delete; image full-size view
	f.3. generation output. Including every debug/extra steps as separated subcontrols
h. Image view - simple thumbnails mode with size scaling + fullsize view with scaling/panning.
g. Simplest interface, only absolute required elements. Right click to confirm, left to cansel. Dublicate onkeyboard controls.