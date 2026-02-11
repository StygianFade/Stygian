// jit_nodes.c - Interactive Node Editor & Stress Test
// Demonstrates Spatial JIT Architecture with User Interaction.

#define STYGIAN_IMPLEMENTATION
#include "../include/stygian.h"
#include "../widgets/stygian_widgets.h"
#include "../window/stygian_window.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Enable DWM/Transparency if on Windows
#ifdef _WIN32
#pragma comment(lib, "dwmapi.lib")
#endif

// ============================================================================
// DATA MODEL (Dynamic SoA)
// ============================================================================

int node_capacity = 0;
int node_count = 0;
float *node_x;
float *node_y;
float *node_w;
float *node_h;
int *node_type;    // 0=Logic, 1=Data, 2=Math
float *node_val_f; // For sliders (0.0 - 1.0)
int *node_val_b;   // For checkboxes (0 or 1)
char *node_text;   // Flat buffer for text inputs (fixed size per node)

#define NODE_TEXT_SIZE 64 // Bytes per node text buffer

int link_capacity = 0;
int link_count = 0;
int *link_from; // Node ID
int *link_to;   // Node ID

// Realloc helper
void resize_nodes(int new_cap) {
  if (new_cap < 1024)
    new_cap = 1024;
  printf("[Graph] Resizing Nodes: %d -> %d\n", node_capacity, new_cap);

  node_x = (float *)realloc(node_x, new_cap * sizeof(float));
  node_y = (float *)realloc(node_y, new_cap * sizeof(float));
  node_w = (float *)realloc(node_w, new_cap * sizeof(float));
  node_h = (float *)realloc(node_h, new_cap * sizeof(float));
  node_type = (int *)realloc(node_type, new_cap * sizeof(int));

  // New columns
  node_val_f = (float *)realloc(node_val_f, new_cap * sizeof(float));
  node_val_b = (int *)realloc(node_val_b, new_cap * sizeof(int));
  node_text =
      (char *)realloc(node_text, new_cap * NODE_TEXT_SIZE * sizeof(char));

  node_capacity = new_cap;
}

void resize_links(int new_cap) {
  if (new_cap < 1024)
    new_cap = 1024;
  printf("[Graph] Resizing Links: %d -> %d\n", link_capacity, new_cap);

  link_from = (int *)realloc(link_from, new_cap * sizeof(int));
  link_to = (int *)realloc(link_to, new_cap * sizeof(int));
  link_capacity = new_cap;
}

int create_node(float x, float y, int type) {
  if (node_count >= node_capacity)
    resize_nodes(node_capacity * 2);

  int idx = node_count++;
  node_x[idx] = x;
  node_y[idx] = y;
  node_w[idx] = 160.0f;
  node_h[idx] = 100.0f;
  node_type[idx] = type;

  // Defaults
  node_val_f[idx] = 0.5f;
  node_val_b[idx] = 1; // Checked by default
  memset(node_text + (idx * NODE_TEXT_SIZE), 0, NODE_TEXT_SIZE);
  snprintf(node_text + (idx * NODE_TEXT_SIZE), NODE_TEXT_SIZE, "Node %d", idx);

  return idx;
}

void create_link(int from, int to) {
  if (from == to)
    return; // No loops
  if (link_count >= link_capacity)
    resize_links(link_capacity * 2);

  int idx = link_count++;
  link_from[idx] = from;
  link_to[idx] = to;
  printf("[Graph] Linked %d -> %d\n", from, to);
}

// Stress Test: Add 10,000 nodes
void stress_test_add_10k() {
  for (int i = 0; i < 10000; i++) {
    float ax = ((float)rand() / RAND_MAX) * 10000.0f - 5000.0f;
    float ay = ((float)rand() / RAND_MAX) * 10000.0f - 5000.0f;
    create_node(ax, ay, rand() % 3);
  }
}

// ============================================================================
// INTERACTION STATE
// ============================================================================

bool ctx_menu_open = false;
float ctx_menu_x = 0;
float ctx_menu_y = 0;
// Where in world space the menu was opened
float ctx_create_x = 0;
float ctx_create_y = 0;

int drag_link_start_node = -1;
bool drag_link_active = false;

// ============================================================================
// FILE I/O (.sty)
// ============================================================================

typedef struct {
  char magic[4]; // "STGY"
  int version;   // 1
  int node_count;
  int link_count;
} StyHeader;

typedef struct {
  float x, y, w, h;
  int type;
  float val_f;
  int val_b;
  char text[NODE_TEXT_SIZE];
} StyNode;

typedef struct {
  int from;
  int to;
} StyLink;

void save_graph(const char *filename) {
  FILE *f = fopen(filename, "wb");
  if (!f) {
    printf("[IO] Failed to save %s\n", filename);
    return;
  }

  StyHeader header = {{'S', 'T', 'G', 'Y'}, 1, node_count, link_count};
  fwrite(&header, sizeof(StyHeader), 1, f);

  // Write Nodes
  for (int i = 0; i < node_count; i++) {
    StyNode n;
    n.x = node_x[i];
    n.y = node_y[i];
    n.w = node_w[i];
    n.h = node_h[i];
    n.type = node_type[i];
    n.val_f = node_val_f[i];
    n.val_b = node_val_b[i];
    memcpy(n.text, node_text + (i * NODE_TEXT_SIZE), NODE_TEXT_SIZE);
    fwrite(&n, sizeof(StyNode), 1, f);
  }

  // Write Links
  for (int i = 0; i < link_count; i++) {
    StyLink l;
    l.from = link_from[i];
    l.to = link_to[i];
    fwrite(&l, sizeof(StyLink), 1, f);
  }

  fclose(f);
  printf("[IO] Saved %d nodes, %d links to %s\n", node_count, link_count,
         filename);
}

void load_graph(const char *filename) {
  FILE *f = fopen(filename, "rb");
  if (!f) {
    printf("[IO] Failed to load %s\n", filename);
    return;
  }

  StyHeader header;
  fread(&header, sizeof(StyHeader), 1, f);
  if (memcmp(header.magic, "STGY", 4) != 0) {
    printf("[IO] Invalid file format\n");
    fclose(f);
    return;
  }

  // Clear current
  node_count = 0;
  link_count = 0;
  resize_nodes(header.node_count + 128);
  resize_links(header.link_count + 128);

  // Read Nodes
  for (int i = 0; i < header.node_count; i++) {
    StyNode n;
    fread(&n, sizeof(StyNode), 1, f);

    int idx = i;
    node_x[idx] = n.x;
    node_y[idx] = n.y;
    node_w[idx] = n.w;
    node_h[idx] = n.h;
    node_type[idx] = n.type;
    node_val_f[idx] = n.val_f;
    node_val_b[idx] = n.val_b;
    memcpy(node_text + (idx * NODE_TEXT_SIZE), n.text, NODE_TEXT_SIZE);
  }
  node_count = header.node_count;

  // Read Links
  for (int i = 0; i < header.link_count; i++) {
    StyLink l;
    fread(&l, sizeof(StyLink), 1, f);
    link_from[i] = l.from;
    link_to[i] = l.to;
  }
  link_count = header.link_count;

  fclose(f);
  printf("[IO] Loaded %d nodes, %d links from %s\n", node_count, link_count,
         filename);
}

// ============================================================================
// MAIN EDITOR
// ============================================================================

int main(int argc, char **argv) {
  StygianWindow *win = stygian_window_create_simple(
      1280, 720, "Stygian Node Editor (Rich Components)");
  StygianConfig conf = {0};
  conf.backend = STYGIAN_BACKEND_OPENGL;
  conf.window = win;
  conf.max_elements =
      65536; // Increased from default 16384 for large text areas
  conf.glyph_feature_flags = STYGIAN_GLYPH_FEATURE_DEFAULT;
  StygianContext *ctx = stygian_create(&conf);

  // Initial Capacity
  resize_nodes(1024);
  resize_links(1024);

  // Create a few starter nodes
  create_node(100, 300, 0);
  create_node(400, 300, 1);
  create_node(700, 300, 2);
  create_link(0, 1);

  // Graph View State
  StygianGraphState graph = {0};
  graph.w = 1280;
  graph.h = 720;
  graph.zoom = 1.0f;
  graph.pin_y_offset = 48.0f;
  graph.pin_size = 16.0f;
  stygian_graph_set_snap(&graph, true, 32.0f);
  stygian_graph_set_wire_style(&graph, STYGIAN_WIRE_SMOOTH);

  while (!stygian_window_should_close(win)) {
    StygianEvent ev;
    stygian_widgets_begin_frame(ctx);
    while (stygian_window_poll_event(win, &ev)) {
      stygian_widgets_process_event(ctx, &ev);
    }

    int win_w, win_h;
    stygian_window_get_size(win, &win_w, &win_h);
    graph.w = (float)win_w;
    // Fix: Toolbar Occlusion - offset graph below toolbar
    graph.y = 40.0f;
    graph.h = (float)win_h - 40.0f;

    stygian_begin_frame(ctx, win_w, win_h);

    // -------------------------------------------------------------
    // TOP TOOLBAR
    // -------------------------------------------------------------
    stygian_panel_begin(ctx, 0, 0, (float)win_w, 40);
    stygian_rect(ctx, 0, 0, (float)win_w, 40, 0.2f, 0.2f, 0.2f, 1.0f);

    // Note: font=0 means "use default font" - stygian_text handles NULL font
    // internally
    if (stygian_button(ctx, 0, "Current: 3 Nodes", 10, 5, 200, 30)) {
      // Info button
    }

    if (stygian_button(ctx, 0, "+10k Stress Test", 220, 5, 150, 30)) {
      stress_test_add_10k();
    }

    if (stygian_button(ctx, 0, "Clear All", 380, 5, 100, 30)) {
      node_count = 0;
      link_count = 0;
    }

    if (stygian_button(ctx, 0, graph.snap_enabled ? "Snap: On" : "Snap: Off",
                       490, 5, 100, 30)) {
      stygian_graph_set_snap(&graph, !graph.snap_enabled, graph.snap_size);
    }

    if (stygian_button(
            ctx, 0,
            graph.wire_style == STYGIAN_WIRE_SMOOTH ? "Wire: Smooth"
                                                   : "Wire: Sharp",
            600, 5, 120, 30)) {
      stygian_graph_set_wire_style(
          &graph, graph.wire_style == STYGIAN_WIRE_SMOOTH ? STYGIAN_WIRE_SHARP
                                                          : STYGIAN_WIRE_SMOOTH);
    }

    stygian_panel_end(ctx);

    // DEBUG: Raw Red Rect at original button location to verify coordinates
    stygian_rect(ctx, 500, 5, 100, 30, 1.0f, 0.0f, 0.0f, 1.0f);

    // DEBUG: Move buttons out to main area for visibility check
    if (stygian_button(ctx, 0, "SAVE (Debug)", 10, 80, 150, 40)) {
      save_graph("graph.sty");
      printf("Save Clicked\n");
    }

    if (stygian_button(ctx, 0, "LOAD (Debug)", 170, 80, 150, 40)) {
      load_graph("graph.sty");
      printf("Load Clicked\n");
    }

    // -------------------------------------------------------------
    // GRAPH INTERACTION LAYER
    // -------------------------------------------------------------

    int mx, my;
    stygian_mouse_pos(win, &mx, &my);

    // Context Menu Trigger (Right Click)
    if (stygian_mouse_down(win, STYGIAN_MOUSE_RIGHT)) {
      if (!ctx_menu_open) {
        ctx_menu_open = true;
        ctx_menu_x = (float)mx;
        ctx_menu_y = (float)my;

        // Store World Position for creation
        stygian_graph_screen_to_world(&graph, (float)mx, (float)my,
                                      &ctx_create_x, &ctx_create_y);
        stygian_graph_snap_pos(&graph, &ctx_create_x, &ctx_create_y);
      }
    }

    // Cancel menu on click outside
    if (ctx_menu_open && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
      // Simple check: if not clicking menu rect (handled inside menu logic)
      // But immediate mode... we handle it by checking if a button was clicked
      // later
    }

    // -------------------------------------------------------------
    // GRAPH RENDERING
    // -------------------------------------------------------------

    StygianNodeBuffers buffers;
    buffers.x = node_x;
    buffers.y = node_y;
    buffers.w = node_w;
    buffers.h = node_h;
    buffers.type_id = node_type;

    stygian_node_graph_begin(ctx, &graph, &buffers, node_count);

    // Links
    // --- Render Visible Links ---
    float line_col[] = {0.8f, 0.8f, 0.8f, 0.8f}; // Higher alpha

    for (int i = 0; i < link_count; i++) {
      int a = link_from[i];
      int b = link_to[i];
      if (a >= node_count || b >= node_count)
        continue;

      float ax = node_x[a];
      float ay = node_y[a];
      float bx = node_x[b];
      float by = node_y[b];

      // Check AABB (link endpoints in world space)
      if (stygian_graph_link_visible_bezier(&graph, ax, ay, bx, by,
                                            200.0f / graph.zoom)) {

        // Calculate Pin Centers (World Space -> Screen Space)
        // Output Pin (Source Node A): Right Side
        // Input Pin (Target Node B): Left Side

        float aw = node_w[a]; // Node A Width
        // Pin offset logic must match rendering logic below
        // Output Pin is at: NodeRight - 8
        // Pin Size is 16
        // Center = NodeRight - 8 + 8 = NodeRight

        // Let's match exactly the rendering logic:
        // PxOut = lx + lw - 8*zoom
        // Center = PxOut + 8*zoom = lx + lw

        // PxIn = lx - 8*zoom
        // Center = PxIn + 8*zoom = lx

        // Y offset: ly + 40*zoom + 8*zoom = ly + 48*zoom

        // World Space Centers
        float sax_world = ax + aw;    // Right edge
        float say_world = ay + 48.0f; // Pin Y Center

        float sbx_world = bx;         // Left edge
        float sby_world = by + 48.0f; // Pin Y Center

        float sax = 0.0f;
        float say = 0.0f;
        float sbx = 0.0f;
        float sby = 0.0f;
        stygian_graph_world_to_screen(&graph, sax_world, say_world, &sax,
                                      &say);
        stygian_graph_world_to_screen(&graph, sbx_world, sby_world, &sbx,
                                      &sby);

        stygian_graph_link(ctx, &graph, sax, say, sbx, sby,
                           4.0f * graph.zoom, line_col);
      }
    }

    // Drag Link Line
    bool drag_link_connected = false;
    if (drag_link_active) {
      float ax = node_x[drag_link_start_node];
      float ay = node_y[drag_link_start_node];
      float aw = node_w[drag_link_start_node];

      // Output Pin Center
      float sax_world = ax + aw;
      float say_world = ay + 48.0f;

      float sax = 0.0f;
      float say = 0.0f;
      stygian_graph_world_to_screen(&graph, sax_world, say_world, &sax, &say);

      float dcol[] = {1, 0.8f, 0.2f, 1.0f};
      stygian_graph_link(ctx, &graph, sax, say, (float)mx, (float)my,
                         4.0f * graph.zoom, dcol);
    }

    // Nodes
    int idx;
    while (stygian_node_graph_next(ctx, &graph, &idx)) {
      // Calc Screen Position
      float lx = 0.0f;
      float ly = 0.0f;
      float lw = 0.0f;
      float lh = 0.0f;
      stygian_graph_node_screen_rect(&graph, node_x[idx], node_y[idx],
                                     node_w[idx], node_h[idx], &lx, &ly, &lw,
                                     &lh);

      // LOD
      if (lw < 5.0f)
        continue;

      char title[64];
      snprintf(title, 64, "Node %d", idx);
      float r = 0.2f, g = 0.2f, b = 0.2f;
      if (node_type[idx] == 0)
        b = 0.5f;
      if (node_type[idx] == 1)
        r = 0.5f;
      if (node_type[idx] == 2)
        g = 0.5f;

      // Base Node
      stygian_rect_rounded(ctx, lx, ly, lw, lh, r * 0.5f, g * 0.5f, b * 0.5f,
                           1.0f, 8.0f);
      stygian_rect_rounded(ctx, lx, ly, lw, 24 * graph.zoom, r, g, b, 1.0f,
                           8.0f);
      stygian_text(ctx, 0, title, lx + 10, ly + 5, 16 * graph.zoom, 1, 1, 1, 1);

      // --- Rich Components ---
      float content_y = ly + 40 * graph.zoom;
      float content_x = lx + 20 * graph.zoom;
      float content_w = lw - 40 * graph.zoom;

      if (node_type[idx] == 0) { // Logic Node (Checkbox)
        bool val = (node_val_b[idx] != 0);
        // Fix: Use index-based ID to ensure stability during zoom/pan
        // (prevents ghost clicks due to stack address reuse)
        uint32_t stable_id = (idx * 0x9e3779b9u) ^ 1;
        if (stygian_checkbox(ctx, stable_id, "Enabled", content_x, content_y,
                             &val)) {
          node_val_b[idx] = val ? 1 : 0;
        }
      }

      if (node_type[idx] == 1) { // Data Node (Slider)
        // Fix: Use index-based ID to ensure stability during zoom/pan
        uint32_t stable_id = (idx * 0x9e3779b9u) ^ 2;
        float val = node_val_f[idx];

        if (stygian_slider(ctx, content_x, content_y, content_w,
                           20 * graph.zoom, &val, 0.0f, 1.0f)) {
          node_val_f[idx] = val;
        }
        // Display Value
        char val_str[32];
        snprintf(val_str, 32, "%.2f", val);
        stygian_text(ctx, 0, val_str, content_x + content_w + 5, content_y + 2,
                     16 * graph.zoom, 1, 1, 1, 0.8f);
      } else if (node_type[idx] == 2) { // Math Node (Text Input)
        char *buf = node_text + (idx * NODE_TEXT_SIZE);
        stygian_text_input(ctx, 0, content_x, content_y, content_w,
                           20 * graph.zoom, buf, NODE_TEXT_SIZE);
      }

      // Pins (Interactive)
      float p_size = 0.0f;
      float px_in = 0.0f;
      float py_pin = 0.0f;
      stygian_graph_pin_rect_screen(&graph, node_x[idx], node_y[idx],
                                    node_w[idx], false, &px_in, &py_pin,
                                    &p_size, &p_size);

      // Draw Input Pin
      stygian_rect_rounded(ctx, px_in, py_pin, p_size, p_size, 0.5f, 0.5f, 0.5f,
                           1.0f, 4.0f);

      // Check Drop (Connect)
      if (drag_link_active && !stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
        // Mouse Up check - Hit test Center
        // Simple radius check better than rect? No, rect is fine.
        if (stygian_graph_pin_hit_test(&graph, node_x[idx], node_y[idx],
                                       node_w[idx], false, (float)mx,
                                       (float)my)) {
          if (drag_link_start_node != idx) {
            // Remove existing links to this input (Single Input Logic)
            for (int k = 0; k < link_count; k++) {
              if (link_to[k] == idx) {
                // Swap with last and delete
                link_from[k] = link_from[link_count - 1];
                link_to[k] = link_to[link_count - 1];
                link_count--;
                k--;
              }
            }
            // Validate Link: No self-loops, No duplicates
            bool exists = false;
            // self-loop check
            if (drag_link_start_node == idx)
              exists = true;
            // duplicate check
            for (int k = 0; k < link_count; k++) {
              if (link_from[k] == drag_link_start_node && link_to[k] == idx) {
                exists = true;
                break;
              }
            }

            if (!exists) {
              create_link(drag_link_start_node, idx);
            }
            drag_link_connected = true;
            drag_link_active = false;
          }
        }
      }

      // Disconnect Logic (Drag from Input)
      if (!drag_link_active && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
        if (stygian_graph_pin_hit_test(&graph, node_x[idx], node_y[idx],
                                       node_w[idx], false, (float)mx,
                                       (float)my)) {
          // Find link connected to this input
          for (int k = 0; k < link_count; k++) {
            if (link_to[k] == idx) {
              // Found it. "Pick it up".
              drag_link_start_node = link_from[k]; // Valid Source
              drag_link_active = true;

              // Delete this link
              link_from[k] = link_from[link_count - 1];
              link_to[k] = link_to[link_count - 1];
              link_count--;
              break;
            }
          }
        }
      }

      // Output (Right)
      float px_out = 0.0f;
      float py_out = 0.0f;
      float p_size_out = 0.0f;
      stygian_graph_pin_rect_screen(&graph, node_x[idx], node_y[idx],
                                    node_w[idx], true, &px_out, &py_out,
                                    &p_size_out, &p_size_out);
      stygian_rect_rounded(ctx, px_out, py_pin, p_size, p_size, 0.8f, 0.8f,
                           0.8f, 1.0f, 4.0f);

      // Check Drag Start (Output)
      if (!drag_link_active && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
        if (stygian_graph_pin_hit_test(&graph, node_x[idx], node_y[idx],
                                       node_w[idx], true, (float)mx,
                                       (float)my)) {
          drag_link_active = true;
          drag_link_start_node = idx;
        }
      }
    }

    stygian_node_graph_end(ctx, &graph);

    // Drop in void (no valid target)
    if (drag_link_active && !stygian_mouse_down(win, STYGIAN_MOUSE_LEFT) &&
        !drag_link_connected) {
      drag_link_active = false;
    }

    // -------------------------------------------------------------
    // CONTEXT MENU (Overlay)
    // -------------------------------------------------------------
    if (ctx_menu_open) {
      float mw = 150;
      float mh = 110;
      stygian_rect_rounded(ctx, ctx_menu_x, ctx_menu_y, mw, mh, 0.15f, 0.15f,
                           0.15f, 1.0f, 4.0f);

      if (stygian_button(ctx, 0, "Create Logic", ctx_menu_x + 5, ctx_menu_y + 5,
                         140, 30)) {
        create_node(ctx_create_x, ctx_create_y, 0);
        ctx_menu_open = false;
      }
      if (stygian_button(ctx, 0, "Create Data", ctx_menu_x + 5, ctx_menu_y + 40,
                         140, 30)) {
        create_node(ctx_create_x, ctx_create_y, 1);
        ctx_menu_open = false;
      }
      if (stygian_button(ctx, 0, "Create Math", ctx_menu_x + 5, ctx_menu_y + 75,
                         140, 30)) {
        create_node(ctx_create_x, ctx_create_y, 2);
        ctx_menu_open = false;
      }

      // Close if clicked outside
      if (stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
        bool inside = (mx >= ctx_menu_x && mx <= ctx_menu_x + mw &&
                       my >= ctx_menu_y && my <= ctx_menu_y + mh);
        if (!inside)
          ctx_menu_open = false;
      }
    }

    // Stats
    char stats[128];
    snprintf(stats, 128, "Nodes: %d | Links: %d", node_count, link_count);
    stygian_text(ctx, 0, stats, 10, 50, 20, 1, 1, 1, 0.5f);

    stygian_end_frame(ctx);
    stygian_window_swap_buffers(win);
  }

  stygian_destroy(ctx);
  stygian_window_destroy(win);
  return 0;
}
