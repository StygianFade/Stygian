# Window and Widgets Reference

This page summarizes the supporting APIs used by app loops.

## Window API (`window/stygian_window.h`)

Lifecycle:
- create, create_simple, from_native, destroy

Event loop:
- poll_event, wait_event, wait_event_timeout, process_events

State:
- size, position, title, focus, close, minimize/maximize/restore

Graphics context hooks:
- OpenGL context create/make_current/swap/vsync/proc lookup
- Vulkan surface extension/surface creation helpers

DPI and IO helpers:
- dpi scale, framebuffer size, clipboard, cursor

## Input API (`window/stygian_input.h`)

Event types include:
- key, char, mouse, scroll, resize, tick, focus/blur, close

`STYGIAN_EVENT_TICK` is used for timer-driven evaluation while preserving DDI causality.

## Widgets API (`widgets/stygian_widgets.h`)

Main widgets:
- button, slider, text input, text area, checkbox, radio
- scrollbar, panel, context menu, modal, tooltip

Event impact flags:
- `POINTER_ONLY`
- `MUTATED_STATE`
- `REQUEST_REPAINT`
- `REQUEST_EVAL`

Diagnostics widget:
- `StygianPerfWidget` supports graph cadence control, stress mode, and visibility toggles.
