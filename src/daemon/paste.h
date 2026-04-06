#ifndef YAPPIE_PASTE_H
#define YAPPIE_PASTE_H

typedef enum {
    WM_HYPRLAND,
    WM_SWAY,
    WM_GENERIC,
} compositor_t;

/* Detect compositor at startup. */
compositor_t paste_detect_compositor(void);

/* Get focused window class. Returns allocated string or NULL. */
char *paste_get_window_class(compositor_t wm);

/* Copy text to clipboard and paste into the window identified by wclass. */
void paste_text(const char *text, const char *wclass);

/* Send a desktop notification. */
void notify(const char *title, const char *body, int timeout_ms, int critical);

#endif
