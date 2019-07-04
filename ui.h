#ifndef UI_H
#define UI_H 1

// display constants related to map scaling/implementation and the curses console
enum {
	// SEEX/SEEY is how far the player can see in the X/Y direction (at least, without scrolling).
	// each map segment will need to be at least this wide/high; map is at least 3x as wide/tall
	SEE = 12,	// for when it matters that they are explicitly equal
	SEEX = SEE,
	SEEY = SEE,
	VIEW = 2 * SEEX + 1,	// required curses view window height.  Default 25.  Fatcat graphical clients can resize as needed.
	// minimap
	MINIMAP_WIDTH_HEIGHT = 7,
	// status bar
	STATUS_BAR_HEIGHT = 4,
	// mon info
	MON_INFO_HEIGHT = 12,
	// the canonical view with the player-scale map had a 55-wide "control panel", for a total width of 25+55=80
	PANELX = 55,
	SCREEN_WIDTH = VIEW+PANELX
};

#endif

