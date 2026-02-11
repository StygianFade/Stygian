// jit_nodes_v2.c - Streamlined Node Editor Example
// Uses Stygian graph helpers to reduce boilerplate.

#define STYGIAN_IMPLEMENTATION
#include "../include/stygian.h"
#include "../widgets/stygian_widgets.h"
#include "../window/stygian_window.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#pragma comment(lib, "dwmapi.lib")
#endif

// ======================================================================
// DATA MODEL (Dynamic SoA)
// ======================================================================

int node_capacity = 0;
int node_count = 0;
float *node_x;
float *node_y;
float *node_w;
float *node_h;
int *node_type;
float *node_val_f;
int *node_val_b;
char *node_text;

#define NODE_TEXT_SIZE 64

int link_capacity = 0;
int link_count = 0;
int *link_from;
int *link_to;

static void resize_nodes(int new_cap) {
  if (new_cap < 1024)
    new_cap = 1024;
  node_x = (float *)realloc(node_x, new_cap * sizeof(float));
  node_y = (float *)realloc(node_y, new_cap * sizeof(float));
  node_w = (float *)realloc(node_w, new_cap * sizeof(float));
  node_h = (float *)realloc(node_h, new_cap * sizeof(float));
  node_type = (int *)realloc(node_type, new_cap * sizeof(int));
  node_val_f = (float *)realloc(node_val_f, new_cap * sizeof(float));
  node_val_b = (int *)realloc(node_val_b, new_cap * sizeof(int));
  node_text =
      (char *)realloc(node_text, new_cap * NODE_TEXT_SIZE * sizeof(char));
  node_capacity = new_cap;
}

static void resize_links(int new_cap) {
  if (new_cap < 1024)
    new_cap = 1024;
  link_from = (int *)realloc(link_from, new_cap * sizeof(int));
  link_to = (int *)realloc(link_to, new_cap * sizeof(int));
  link_capacity = new_cap;
}

static int create_node(float x, float y, int type) {
  if (node_count >= node_capacity)
    resize_nodes(node_capacity * 2);

  int idx = node_count++;
  node_x[idx] = x;
  node_y[idx] = y;
  node_w[idx] = 160.0f;
  node_h[idx] = 100.0f;
  node_type[idx] = type;

  node_val_f[idx] = 0.5f;
  node_val_b[idx] = 1;
  memset(node_text + (idx * NODE_TEXT_SIZE), 0, NODE_TEXT_SIZE);
  snprintf(node_text + (idx * NODE_TEXT_SIZE), NODE_TEXT_SIZE, "Node %d", idx);
  return idx;
}

static void create_link(int from, int to) {
  if (from == to)
    return;
  if (link_count >= link_capacity)
    resize_links(link_capacity * 2);
  int idx = link_count++;
  link_from[idx] = from;
  link_to[idx] = to;
}

// ======================================================================
// CONTEXT MENU STATE
// ======================================================================

static bool ctx_menu_open = false;
static float ctx_menu_x = 0;
static float ctx_menu_y = 0;
static float ctx_create_x = 0;
static float ctx_create_y = 0;

// Dragging state for wire creation
static bool drag_link_active = false;
static int drag_link_start_node = -1;

// ======================================================================
// MAIN
// ======================================================================

int main(int argc, char **argv) {
  StygianWindow *win =
      stygian_window_create_simple(1280, 720, "Stygian Node Editor v2");
  StygianConfig conf = {0};
  conf.backend = STYGIAN_BACKEND_OPENGL;
  conf.window = win;
  conf.max_elements = 65536;
  conf.glyph_feature_flags = STYGIAN_GLYPH_FEATURE_DEFAULT;
  StygianContext *ctx = stygian_create(&conf);

  resize_nodes(1024);
  resize_links(1024);

  create_node(100, 300, 0);
  create_node(400, 300, 1);
  create_node(700, 300, 2);
  create_link(0, 1);

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
    graph.y = 40.0f;
    graph.h = (float)win_h - 40.0f;

    stygian_begin_frame(ctx, win_w, win_h);

    // ------------------------------------------------------------------
    // Toolbar
    // ------------------------------------------------------------------
    stygian_panel_begin(ctx, 0, 0, (float)win_w, 40);
    stygian_rect(ctx, 0, 0, (float)win_w, 40, 0.2f, 0.2f, 0.2f, 1.0f);

    char stats[128];
    snprintf(stats, 128, "Nodes: %d | Links: %d", node_count, link_count);
    stygian_text(ctx, 0, stats, 10, 8, 18, 1, 1, 1, 0.8f);

    if (stygian_button(ctx, 0, graph.snap_enabled ? "Snap: On" : "Snap: Off",
                       300, 5, 100, 30)) {
      stygian_graph_set_snap(&graph, !graph.snap_enabled, graph.snap_size);
    }

    if (stygian_button(
            ctx, 0,
            graph.wire_style == STYGIAN_WIRE_SMOOTH ? "Wire: Smooth"
                                                   : "Wire: Sharp",
            410, 5, 120, 30)) {
      stygian_graph_set_wire_style(
          &graph, graph.wire_style == STYGIAN_WIRE_SMOOTH ? STYGIAN_WIRE_SHARP
                                                          : STYGIAN_WIRE_SMOOTH);
    }

    stygian_panel_end(ctx);

    // ------------------------------------------------------------------
    // Input / Context Menu
    // ------------------------------------------------------------------
    int mx, my;
    stygian_mouse_pos(win, &mx, &my);

    if (stygian_mouse_down(win, STYGIAN_MOUSE_RIGHT)) {
      if (!ctx_menu_open) {
        ctx_menu_open = true;
        ctx_menu_x = (float)mx;
        ctx_menu_y = (float)my;
        stygian_graph_screen_to_world(&graph, (float)mx, (float)my,
                                      &ctx_create_x, &ctx_create_y);
        stygian_graph_snap_pos(&graph, &ctx_create_x, &ctx_create_y);
      }
    }

    // ------------------------------------------------------------------
    // Graph rendering
    // ------------------------------------------------------------------
    StygianNodeBuffers buffers;
    buffers.x = node_x;
    buffers.y = node_y;
    buffers.w = node_w;
    buffers.h = node_h;
    buffers.type_id = node_type;

    stygian_node_graph_begin(ctx, &graph, &buffers, node_count);

    // Links (Layer 1: wires)
    stygian_layer_begin(ctx);
    float line_col[] = {0.8f, 0.8f, 0.8f, 0.8f};
    for (int i = 0; i < link_count; i++) {
      int a = link_from[i];
      int b = link_to[i];
      if (a >= node_count || b >= node_count)
        continue;

      float ax = node_x[a];
      float ay = node_y[a];
      float bx = node_x[b];
      float by = node_y[b];

      if (stygian_graph_link_visible_bezier(&graph, ax, ay, bx, by,
                                            200.0f / graph.zoom)) {
        float aw = node_w[a];
        float sax_world = ax + aw;
        float say_world = ay + 48.0f;
        float sbx_world = bx;
        float sby_world = by + 48.0f;

        float sax = 0.0f;
        float say = 0.0f;
        float sbx = 0.0f;
        float sby = 0.0f;
        stygian_graph_world_to_screen(&graph, sax_world, say_world, &sax,
                                      &say);
        stygian_graph_world_to_screen(&graph, sbx_world, sby_world, &sbx,
                                      &sby);
        stygian_graph_link(ctx, &graph, sax, say, sbx, sby, 4.0f * graph.zoom,
                           line_col);
      }
    }

    bool drag_link_connected = false;
    if (drag_link_active) {
      float ax = node_x[drag_link_start_node];
      float ay = node_y[drag_link_start_node];
      float aw = node_w[drag_link_start_node];
      float sax_world = ax + aw;
      float say_world = ay + 48.0f;
      float sax = 0.0f;
      float say = 0.0f;
      stygian_graph_world_to_screen(&graph, sax_world, say_world, &sax, &say);
      float dcol[] = {1, 0.8f, 0.2f, 1.0f};
      stygian_graph_link(ctx, &graph, sax, say, (float)mx, (float)my,
                         4.0f * graph.zoom, dcol);
    }
    stygian_layer_end(ctx);

    // Nodes (Layer 2: bodies + widgets)
    stygian_layer_begin(ctx);
    int idx;
    while (stygian_node_graph_next(ctx, &graph, &idx)) {
      float lx = 0.0f;
      float ly = 0.0f;
      float lw = 0.0f;
      float lh = 0.0f;
      stygian_graph_node_screen_rect(&graph, node_x[idx], node_y[idx],
                                     node_w[idx], node_h[idx], &lx, &ly, &lw,
                                     &lh);

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

      stygian_rect_rounded(ctx, lx, ly, lw, lh, r * 0.5f, g * 0.5f, b * 0.5f,
                           1.0f, 8.0f);
      stygian_rect_rounded(ctx, lx, ly, lw, 24 * graph.zoom, r, g, b, 1.0f,
                           8.0f);

      float content_y = ly + 40 * graph.zoom;
      float content_x = lx + 20 * graph.zoom;
      float content_w = lw - 40 * graph.zoom;

      if (node_type[idx] == 0) {
        bool val = (node_val_b[idx] != 0);
        uint32_t stable_id = (idx * 0x9e3779b9u) ^ 1;
        if (stygian_checkbox(ctx, stable_id, "Enabled", content_x, content_y,
                             &val)) {
          node_val_b[idx] = val ? 1 : 0;
        }
      }

      if (node_type[idx] == 1) {
        uint32_t stable_id = (idx * 0x9e3779b9u) ^ 2;
        float val = node_val_f[idx];
        if (stygian_slider(ctx, content_x, content_y, content_w,
                           20 * graph.zoom, &val, 0.0f, 1.0f)) {
          node_val_f[idx] = val;
        }
      } else if (node_type[idx] == 2) {
        char *buf = node_text + (idx * NODE_TEXT_SIZE);
        stygian_text_input(ctx, 0, content_x, content_y, content_w,
                           20 * graph.zoom, buf, NODE_TEXT_SIZE);
      }

      float p_size = 0.0f;
      float px_in = 0.0f;
      float py_in = 0.0f;
      stygian_graph_pin_rect_screen(&graph, node_x[idx], node_y[idx],
                                    node_w[idx], false, &px_in, &py_in,
                                    &p_size, &p_size);
      stygian_rect_rounded(ctx, px_in, py_in, p_size, p_size, 0.5f, 0.5f, 0.5f,
                           1.0f, 4.0f);

      if (drag_link_active && !stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
        if (stygian_graph_pin_hit_test(&graph, node_x[idx], node_y[idx],
                                       node_w[idx], false, (float)mx,
                                       (float)my)) {
          if (drag_link_start_node != idx) {
            for (int k = 0; k < link_count; k++) {
              if (link_to[k] == idx) {
                link_from[k] = link_from[link_count - 1];
                link_to[k] = link_to[link_count - 1];
                link_count--;
                k--;
              }
            }
            bool exists = false;
            if (drag_link_start_node == idx)
              exists = true;
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

      if (!drag_link_active && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
        if (stygian_graph_pin_hit_test(&graph, node_x[idx], node_y[idx],
                                       node_w[idx], false, (float)mx,
                                       (float)my)) {
          for (int k = 0; k < link_count; k++) {
            if (link_to[k] == idx) {
              drag_link_start_node = link_from[k];
              drag_link_active = true;
              link_from[k] = link_from[link_count - 1];
              link_to[k] = link_to[link_count - 1];
              link_count--;
              break;
            }
          }
        }
      }

      float px_out = 0.0f;
      float py_out = 0.0f;
      float p_size_out = 0.0f;
      stygian_graph_pin_rect_screen(&graph, node_x[idx], node_y[idx],
                                    node_w[idx], true, &px_out, &py_out,
                                    &p_size_out, &p_size_out);
      stygian_rect_rounded(ctx, px_out, py_out, p_size_out, p_size_out, 0.8f,
                           0.8f, 0.8f, 1.0f, 4.0f);

      if (!drag_link_active && stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
        if (stygian_graph_pin_hit_test(&graph, node_x[idx], node_y[idx],
                                       node_w[idx], true, (float)mx,
                                       (float)my)) {
          drag_link_active = true;
          drag_link_start_node = idx;
        }
      }
    }
    stygian_layer_end(ctx);

    // Labels (Layer 3: text only)
    stygian_layer_begin(ctx);
    for (int i = 0; i < node_count; i++) {
      float lx = 0.0f;
      float ly = 0.0f;
      float lw = 0.0f;
      float lh = 0.0f;
      stygian_graph_node_screen_rect(&graph, node_x[i], node_y[i], node_w[i],
                                     node_h[i], &lx, &ly, &lw, &lh);

      if (lw < 5.0f)
        continue;

      char title[64];
      snprintf(title, 64, "Node %d", i);
      stygian_text(ctx, 0, title, lx + 10, ly + 5, 16 * graph.zoom, 1, 1, 1,
                   1);

      if (node_type[i] == 1) {
        float content_x = lx + 20 * graph.zoom;
        float content_y = ly + 40 * graph.zoom;
        float content_w = lw - 40 * graph.zoom;
        char val_str[32];
        snprintf(val_str, 32, "%.2f", node_val_f[i]);
        stygian_text(ctx, 0, val_str, content_x + content_w + 5, content_y + 2,
                     16 * graph.zoom, 1, 1, 1, 0.8f);
      }
    }
    stygian_layer_end(ctx);

    stygian_node_graph_end(ctx, &graph);

    if (drag_link_active && !stygian_mouse_down(win, STYGIAN_MOUSE_LEFT) &&
        !drag_link_connected) {
      drag_link_active = false;
    }

    // Context Menu
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

      if (stygian_mouse_down(win, STYGIAN_MOUSE_LEFT)) {
        bool inside = (mx >= ctx_menu_x && mx <= ctx_menu_x + mw &&
                       my >= ctx_menu_y && my <= ctx_menu_y + mh);
        if (!inside)
          ctx_menu_open = false;
      }
    }

    stygian_end_frame(ctx);
    stygian_window_swap_buffers(win);
  }

  stygian_destroy(ctx);
  stygian_window_destroy(win);
  return 0;
}
