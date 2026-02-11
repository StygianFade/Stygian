#include "../include/stygian_clipboard.h"
#include "../window/stygian_window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>

#define mkdir(dir, mode) _mkdir(dir)
#endif

// Max history items to keep in memory (simple ring buffer)
#define MAX_HISTORY 32

// We need a place to store state. StygianContext is opaque.
// Ideally StygianContext would have a metadata slot or we use a global manager
// for now. For robust design, we'll attach it to the Window userdata or a
// static singleton if single-window. Given "Phase 10" nature, let's use a
// static manager for simplicity in this demo.

typedef struct {
  char *history[MAX_HISTORY];
  int head; // Points to next write
  int count;
} ClipboardManager;

static ClipboardManager g_clipboard = {0};

static void ensure_directory(const char *path) {
  struct stat st = {0};
  if (stat(path, &st) == -1) {
    mkdir(path, 0700);
  }
}

void stygian_clipboard_push(StygianContext *ctx, const char *text,
                            const char *metadata) {
  if (!text)
    return;

  // 1. Write to OS Clipboard (Sync)
  StygianWindow *win = stygian_get_window(ctx);
  if (win) {
    stygian_clipboard_write(win, text);
  }

  // 2. Write to Memory History
  int slot = g_clipboard.head;
  if (g_clipboard.history[slot]) {
    free(g_clipboard.history[slot]);
  }
  g_clipboard.history[slot] = _strdup(text);
  g_clipboard.head = (g_clipboard.head + 1) % MAX_HISTORY;
  if (g_clipboard.count < MAX_HISTORY)
    g_clipboard.count++;

  // 3. Write to Disk Artifact (.stygian/clipboard_history/)
  ensure_directory(".stygian");
  ensure_directory(".stygian/clipboard_history");

  // Generate filename: clip_TIMESTAMP_HASH.txt
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  // Simple hash to avoid name collisions in same second
  unsigned long hash = 5381;
  const char *p = text;
  int c;
  while ((c = *p++))
    hash = ((hash << 5) + hash) + c;

  char filename[512];
  snprintf(filename, sizeof(filename),
           ".stygian/clipboard_history/clip_%04d%02d%02d_%02d%02d%02d_%lx.txt",
           t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min,
           t->tm_sec, hash);

  FILE *f = fopen(filename, "w");
  if (f) {
    // Write Metadata Header?
    if (metadata) {
      fprintf(f, "# METADATA: %s\n", metadata);
    }
    fprintf(f, "%s", text);
    fclose(f);
    printf("[Clipboard] Saved artifact: %s\n", filename);
  }
}

char *stygian_clipboard_pop(StygianContext *ctx) {
  // Read from OS first (source of truth for external pastes)
  StygianWindow *win = stygian_get_window(ctx);
  if (win) {
    return stygian_clipboard_read(win);
  }
  return NULL;
}

int stygian_clipboard_history_count(StygianContext *ctx) {
  return g_clipboard.count;
}

const char *stygian_clipboard_history_get(StygianContext *ctx, int index) {
  if (index < 0 || index >= g_clipboard.count)
    return NULL;
  // Calculate actual index from head
  // head points to next write. oldest is at (head - count + MAX) % MAX
  int start =
      (g_clipboard.head - g_clipboard.count + MAX_HISTORY) % MAX_HISTORY;
  int actual = (start + index) % MAX_HISTORY;
  return g_clipboard.history[actual];
}
