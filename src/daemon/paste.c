#include "paste.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <cJSON.h>

static int cmd_exists(const char *name) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "command -v %s >/dev/null 2>&1", name);
    return system(cmd) == 0;
}

compositor_t paste_detect_compositor(void) {
    if (cmd_exists("hyprctl"))  return WM_HYPRLAND;
    if (cmd_exists("swaymsg"))  return WM_SWAY;
    return WM_GENERIC;
}

/* Run a command and capture stdout. Caller frees. */
static char *capture_cmd(const char *cmd) {
    FILE *f = popen(cmd, "r");
    if (!f) return NULL;

    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    pclose(f);
    buf[n] = '\0';

    if (n == 0) return NULL;
    return strdup(buf);
}

/* Extract .class from hyprctl activewindow -j output */
static char *parse_hyprland_class(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return NULL;

    cJSON *cls = cJSON_GetObjectItemCaseSensitive(root, "class");
    char *result = NULL;
    if (cJSON_IsString(cls) && cls->valuestring[0])
        result = strdup(cls->valuestring);

    cJSON_Delete(root);
    return result;
}

/* Walk swaymsg -t get_tree output to find focused window's app_id */
static char *find_sway_focused(cJSON *node) {
    if (!node) return NULL;

    cJSON *focused = cJSON_GetObjectItemCaseSensitive(node, "focused");
    if (cJSON_IsTrue(focused)) {
        cJSON *app_id = cJSON_GetObjectItemCaseSensitive(node, "app_id");
        if (cJSON_IsString(app_id) && app_id->valuestring[0])
            return strdup(app_id->valuestring);
    }

    /* Recurse into nodes array */
    cJSON *nodes = cJSON_GetObjectItemCaseSensitive(node, "nodes");
    if (nodes) {
        cJSON *child;
        cJSON_ArrayForEach(child, nodes) {
            char *result = find_sway_focused(child);
            if (result) return result;
        }
    }

    /* Also check floating_nodes */
    cJSON *floating = cJSON_GetObjectItemCaseSensitive(node, "floating_nodes");
    if (floating) {
        cJSON *child;
        cJSON_ArrayForEach(child, floating) {
            char *result = find_sway_focused(child);
            if (result) return result;
        }
    }

    return NULL;
}

static char *parse_sway_class(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return NULL;

    char *result = find_sway_focused(root);
    cJSON_Delete(root);
    return result;
}

char *paste_get_window_class(compositor_t wm) {
    char *json = NULL;
    char *result = NULL;

    switch (wm) {
    case WM_HYPRLAND:
        json = capture_cmd("hyprctl activewindow -j 2>/dev/null");
        if (json) result = parse_hyprland_class(json);
        break;
    case WM_SWAY:
        json = capture_cmd("swaymsg -t get_tree 2>/dev/null");
        if (json) result = parse_sway_class(json);
        break;
    case WM_GENERIC:
        break;
    }

    free(json);
    return result;
}

static int is_terminal(const char *wclass) {
    if (!wclass || !wclass[0]) return 0;
    static const char *terminals[] = {
        "com.mitchellh.ghostty",
        "kitty",
        "Alacritty",
        "foot",
        "org.wezfurlong.wezterm",
        NULL,
    };
    for (const char **t = terminals; *t; t++) {
        if (strcmp(wclass, *t) == 0) return 1;
    }
    return 0;
}

void paste_text(const char *text, const char *wclass) {
    pid_t pid = fork();
    if (pid == 0) {
        execlp("wl-copy", "wl-copy", text, NULL);
        _exit(1);
    }
    if (pid > 0) waitpid(pid, NULL, 0);

    usleep(50000); /* 50ms for clipboard to propagate */

    if (is_terminal(wclass)) {
        pid = fork();
        if (pid == 0) {
            execlp("ydotool", "ydotool", "key",
                   "29:1", "42:1", "47:1", "47:0", "42:0", "29:0", NULL);
            _exit(1);
        }
    } else {
        pid = fork();
        if (pid == 0) {
            execlp("ydotool", "ydotool", "key",
                   "29:1", "47:1", "47:0", "29:0", NULL);
            _exit(1);
        }
    }
    if (pid > 0) waitpid(pid, NULL, 0);
}

void notify(const char *title, const char *body, int timeout_ms, int critical) {
    /* Double-fork to avoid zombie processes */
    pid_t pid = fork();
    if (pid == 0) {
        if (fork() == 0) {
            char timeout_str[32];
            snprintf(timeout_str, sizeof(timeout_str), "%d", timeout_ms);
            if (critical) {
                execlp("notify-send", "notify-send",
                       "-u", "critical", "-t", timeout_str, title, body, NULL);
            } else {
                execlp("notify-send", "notify-send",
                       "-t", timeout_str, title, body, NULL);
            }
            _exit(1);
        }
        _exit(0);
    }
    if (pid > 0) waitpid(pid, NULL, 0);
}
