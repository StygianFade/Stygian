# Stygian Widget Taxonomy - 200+ Components

## Vision Statement

**Stygian will be the most comprehensive immediate-mode UI library ever created**, combining:
- **ImGui's simplicity** (immediate-mode, zero-state widgets)
- **Web-tech breadth** (shadcn/ui, GitHub Primer, Ant Design feature parity)
- **D3.js/Plotly visualization power** (100+ chart types)
- **DAW-level audio UI** (knobs, faders, waveforms, spectrum analyzers)
- **VSCode extensibility** (panels, tree views, webviews)
- **Social media richness** (feeds, timelines, chat, messaging)
- **Node editor capabilities** (React Flow, Rete.js style graphs)
- **Sports analytics** (football stats, heat maps, player tracking)
- **C-level performance** (GPU SDF rendering, single draw call, zero allocations)

## Current Shipping Baseline (Implemented)

The following core widgets are currently implemented and validated in demos/tests:
- Button (`stygian_button`, `stygian_button_ex`)
- Slider (`stygian_slider`, `stygian_slider_ex`)
- Checkbox and radio
- Text input and multiline text area (selection/copy/paste/scroll)
- Vertical scrollbar (`stygian_scrollbar_v`) for reusable scroll regions
- Overlay primitives:
  - Tooltip (`stygian_tooltip`)
  - Context menu (`stygian_context_menu_*`)
  - Modal (`stygian_modal_*`)
- Panel begin/end (clip-scoped)
- Node graph helpers (smooth/sharp wires, snap/grid helpers)
- Diagnostics/perf panel (`stygian_perf_widget`) with graph windowing and
  optional memory/glyph/TRIAD stats

Validation harnesses:
- `examples/widgets_full_test.c`
- `examples/widgets_stress_harness.c`

Shipping hardening status is focused on these primitives first, then category-by-category expansion.

---

## Research Summary

| Domain | Source | Component Count |
|--------|--------|----------------|
| **ImGui** | Immediate-mode GUI | ~54 core widgets |
| **GitHub Primer** | Design system | 50+ components |
| **shadcn/ui** | React/Tailwind | 59 official |
| **Ant Design** | Enterprise UI | 50+ components |
| **D3.js** | Data visualization | 100+ chart types |
| **Plotly** | Scientific charts | 40+ chart types |
| **VSCode** | Editor UI | 15+ contribution points |
| **DAW/Audio** | Signal processing | 20+ specialized widgets |
| **Node Editors** | React Flow, Rete.js | 10+ node-specific widgets |
| **Social Media** | Feeds, chat, timeline | 15+ specialized widgets |

**Stygian Target: 200+ widgets across 15 categories**

---

## Category 1: Core Input & Interaction (60 widgets)

### Buttons (12)
- [ ] Button (primary, secondary, tertiary)
- [ ] SmallButton, LargeButton
- [ ] IconButton, ImageButton
- [ ] ArrowButton (up, down, left, right)
- [ ] InvisibleButton (hit testing)
- [ ] ToggleButton, SplitButton
- [ ] FloatButton (FAB)
- [ ] ButtonGroup (horizontal, vertical)

### Checkboxes & Radio (6)
- [ ] Checkbox, CheckboxFlags
- [ ] CheckboxGroup
- [ ] RadioButton, RadioGroup
- [ ] Switch / Toggle

### Sliders & Ranges (20)
- [ ] SliderInt, SliderInt2, SliderInt3, SliderInt4
- [ ] SliderFloat, SliderFloat2, SliderFloat3, SliderFloat4
- [ ] SliderAngle
- [ ] VSliderInt, VSliderFloat (vertical)
- [ ] RangeSlider (dual-handle)
- [ ] CircularSlider (radial)
- [ ] Knob (rotary, DAW-style)

### Drags & Spinners (14)
- [ ] DragInt, DragInt2, DragInt3, DragInt4
- [ ] DragFloat, DragFloat2, DragFloat3, DragFloat4
- [ ] DragIntRange2, DragFloatRange2
- [ ] DragAngle
- [ ] Spinner (increment/decrement)

### Rating & Segmented (8)
- [ ] Rating (stars)
- [ ] SegmentedControl
- [ ] Stepper (iOS-style)
- [ ] PinInput (OTP/code)
- [ ] Dial (rotary value)
- [ ] Joystick (2D input)
- [ ] Touchpad (gesture area)
- [ ] GestureZone

---

## Category 2: Text & Data Entry (25 widgets)

### Text Inputs (15)
- [ ] InputText, InputTextMultiline
- [ ] InputTextWithHint
- [ ] PasswordInput (masked)
- [ ] SearchInput (with icon)
- [ ] NumberInput
- [ ] InputInt, InputInt2, InputInt3, InputInt4
- [ ] InputFloat, InputFloat2, InputFloat3, InputFloat4
- [ ] InputDouble
- [ ] InputScalar, InputScalarN

### Advanced Text (10)
- [ ] AutoComplete
- [ ] TagInput (chip input)
- [ ] Mentions (@-mentions)
- [ ] CodeEditor (syntax highlighting)
- [ ] MarkdownEditor
- [ ] RichTextEditor (WYSIWYG)
- [ ] TextArea (resizable)
- [ ] FormattedInput (masks, patterns)
- [ ] CurrencyInput
- [ ] PhoneInput

---

## Category 3: Selectors & Pickers (18 widgets)

### Dropdowns & Lists (8)
- [ ] Combo, ListBox
- [ ] MultiSelect
- [ ] Selectable
- [ ] BeginListBox/EndListBox
- [ ] Cascader (hierarchical select)
- [ ] Transfer (dual-list)
- [ ] SelectPanel

### Pickers (10)
- [ ] DatePicker, TimePicker, DateTimePicker
- [ ] DateRangePicker, TimeRangePicker
- [ ] ColorPicker3, ColorPicker4
- [ ] ColorEdit3, ColorEdit4
- [ ] ColorButton
- [ ] FilePicker / FileUpload
- [ ] ImagePicker
- [ ] FontPicker
- [ ] IconPicker
- [ ] EmojiPicker

---

## Category 4: Data Display & Tables (20 widgets)

### Tables & Grids (8)
- [ ] BeginTable/EndTable (full table API)
- [ ] DataGrid (advanced, sortable, filterable)
- [ ] VirtualTable (large datasets)
- [ ] EditableTable (inline editing)
- [ ] PivotTable
- [ ] Spreadsheet (Excel-like)
- [ ] DataTable (GitHub Primer style)
- [ ] TreeTable (hierarchical data)

### Lists & Collections (12)
- [ ] List (simple, virtualized)
- [ ] VirtualList (infinite scroll)
- [ ] Timeline
- [ ] Feed (social media style)
- [ ] Card, CardGrid
- [ ] Masonry (Pinterest layout)
- [ ] Gallery, ImageGallery
- [ ] Carousel / Slider
- [ ] Kanban (drag-drop board)
- [ ] Calendar (month, week, day views)
- [ ] Agenda (event list)
- [ ] ActivityLog

---

## Category 5: Trees & Hierarchies (8 widgets)

- [ ] TreeNode, TreePop
- [ ] TreeView (full tree API)
- [ ] CollapsingHeader
- [ ] SetNextItemOpen
- [ ] FileTree (VSCode-style)
- [ ] FolderTree
- [ ] Accordion / Collapse
- [ ] NestedList

---

## Category 6: Navigation & Menus (18 widgets)

### Menus (8)
- [ ] BeginMenu/EndMenu, MenuItem
- [ ] BeginMenuBar/EndMenuBar
- [ ] ContextMenu (right-click)
- [ ] DropdownMenu
- [ ] ActionMenu (GitHub style)
- [ ] CommandPalette (Cmd+K)
- [ ] QuickPick (VSCode style)
- [ ] Spotlight (macOS style)

### Navigation (10)
- [ ] BeginTabBar/EndTabBar
- [ ] BeginTabItem/EndTabItem
- [ ] Tabs (horizontal, vertical)
- [ ] Breadcrumb
- [ ] Pagination
- [ ] Steps / Stepper
- [ ] Anchor (table of contents)
- [ ] NavList (GitHub style)
- [ ] Sidebar, BottomNavigation
- [ ] UnderlineNav, UnderlinePanels

---

## Category 7: Overlays & Feedback (20 widgets)

### Modals & Dialogs (8)
- [ ] Modal / Dialog
- [ ] ConfirmDialog
- [ ] Popconfirm
- [ ] Drawer (side panel)
- [ ] Sheet (bottom sheet)
- [ ] Popover
- [ ] Tooltip
- [ ] AnchoredOverlay

### Notifications (12)
- [ ] Alert / Banner
- [ ] Toast / Notification
- [ ] Snackbar
- [ ] InlineMessage
- [ ] Message (floating)
- [ ] Progress (linear)
- [ ] ProgressCircle (circular)
- [ ] ProgressBar
- [ ] Spinner / Loader
- [ ] Skeleton (loading placeholder)
- [ ] Backdrop
- [ ] Watermark

---

## Category 8: Layout & Containers (15 widgets)

- [ ] Panel / Container
- [ ] Box, Stack (VStack, HStack)
- [ ] Grid, Flex
- [ ] Splitter (resizable divider)
- [ ] SplitPageLayout
- [ ] PageLayout, PageHeader
- [ ] ScrollArea, ScrollView
- [ ] Divider / Separator
- [ ] Spacer, Dummy
- [ ] Blankslate (empty state)
- [ ] Affix (sticky)
- [ ] BackTop (scroll to top)
- [ ] Resizable (resize handle)
- [ ] DockSpace (docking layout)
- [ ] Workspace (multi-panel)

---

## Category 9: Charts & Data Visualization (40 widgets)

### Basic Charts (10)
- [ ] LineChart, AreaChart
- [ ] BarChart (vertical, horizontal)
- [ ] StackedBarChart
- [ ] GroupedBarChart
- [ ] PieChart, DonutChart
- [ ] ScatterPlot, BubbleChart
- [ ] Histogram

### Statistical Charts (10)
- [ ] BoxPlot, ViolinPlot
- [ ] Heatmap, Correlogram
- [ ] DensityPlot (1D, 2D)
- [ ] ContourPlot
- [ ] Candlestick (financial)
- [ ] Waterfall
- [ ] FunnelChart
- [ ] RadarChart, PolarChart
- [ ] ParallelCoordinates

### Advanced Visualizations (10)
- [ ] Treemap, Sunburst
- [ ] CirclePacking
- [ ] SankeyDiagram
- [ ] ChordDiagram
- [ ] ForceDirectedGraph
- [ ] NetworkGraph
- [ ] Dendrogram
- [ ] Icicle
- [ ] Streamgraph
- [ ] RidgePlot

### Time Series & Geo (10)
- [ ] TimeSeriesChart
- [ ] Sparkline
- [ ] Gauge, Indicator (KPI)
- [ ] ChoroplethMap
- [ ] BubbleMap, HexbinMap
- [ ] HeatMap (geographic)
- [ ] FlowMap
- [ ] VectorField
- [ ] Wordcloud
- [ ] PlotLines, PlotHistogram (ImGui style)

---

## Category 10: Node Editors & Graphs (12 widgets)

- [ ] NodeGraph (full editor)
- [ ] Node (draggable)
- [ ] NodeSocket (input/output)
- [ ] NodeConnection (edge/wire)
- [ ] NodeCanvas (background grid)
- [ ] Minimap (node overview)
- [ ] NodePalette (node library)
- [ ] NodeInspector (properties)
- [ ] FlowChart
- [ ] StateMachine (visual)
- [ ] BehaviorTree
- [ ] Blueprint (UE4-style)

---

## Category 11: Audio & Signal Processing (15 widgets)

### DAW Controls (8)
- [ ] Knob (rotary, skeuomorphic)
- [ ] Fader (vertical, horizontal)
- [ ] VUMeter, PeakMeter
- [ ] LevelMeter (multi-channel)
- [ ] TransportControls (play, stop, record)
- [ ] WaveformDisplay
- [ ] SpectrogramDisplay
- [ ] SpectrumAnalyzer

### Audio Editing (7)
- [ ] AudioTimeline
- [ ] MixerChannel
- [ ] EQCurve (graphical EQ)
- [ ] CompressorCurve
- [ ] EnvelopeEditor (ADSR)
- [ ] PianoRoll (MIDI)
- [ ] SampleEditor

---

## Category 12: Social Media & Collaboration (15 widgets)

### Feeds & Timelines (5)
- [ ] Feed (infinite scroll)
- [ ] Timeline (chronological)
- [ ] ActivityFeed
- [ ] NewsFeed
- [ ] MasonryFeed

### Chat & Messaging (10)
- [ ] ChatBubble, MessageBubble
- [ ] ChatInput
- [ ] ChatTimeline
- [ ] TypingIndicator
- [ ] ReadReceipt
- [ ] ReactionPicker (emoji)
- [ ] ThreadView
- [ ] ChannelList
- [ ] UserList
- [ ] PresenceIndicator

---

## Category 13: Sports & Analytics (12 widgets)

### Football/Soccer Specific (8)
- [ ] PitchHeatmap (player positions)
- [ ] PassNetwork
- [ ] ShotMap
- [ ] PlayerTrails (movement)
- [ ] FormationDiagram
- [ ] MatchTimeline
- [ ] LeagueTable
- [ ] PlayerStatsCard

### General Sports (4)
- [ ] ScoreWidget
- [ ] LiveScoreboard
- [ ] StandingsTable
- [ ] H2HComparison (head-to-head)

---

## Category 14: Editor & IDE Components (15 widgets)

### VSCode-style (10)
- [ ] CodeEditor (full featured)
- [ ] SyntaxHighlighter
- [ ] LineNumbers
- [ ] Minimap (code overview)
- [ ] Breadcrumb (file path)
- [ ] FileExplorer (tree)
- [ ] SearchPanel
- [ ] ReplacePanel
- [ ] TerminalEmulator
- [ ] DebugPanel

### Code Features (5)
- [ ] CodeLens (inline info)
- [ ] InlayHints
- [ ] Gutter (line decorations)
- [ ] DiffViewer (side-by-side)
- [ ] BlameView (git annotations)

---

## Category 15: Specialized & Advanced (17 widgets)

### Media (5)
- [ ] VideoPlayer
- [ ] AudioPlayer
- [ ] ImageViewer (zoom, pan)
- [ ] PDFViewer
- [ ] 3DViewer (model preview)

### Utilities (12)
- [ ] QRCode, Barcode
- [ ] Tour (onboarding)
- [ ] Spotlight (feature highlight)
- [ ] Hotkey (keyboard shortcut display)
- [ ] Badge, Tag, Chip
- [ ] Avatar, AvatarStack
- [ ] StateLabel (GitHub style)
- [ ] CounterLabel
- [ ] CircleBadge
- [ ] Token (removable tag)
- [ ] Truncate (text overflow)
- [ ] RelativeTime (time ago)

---

## Total Widget Count: **207 widgets**

---

## Implementation Phases

### Phase 1: ImGui Core Parity (54 widgets)
All ImGui widgets for immediate usability

### Phase 2: Web-Tech Essentials (30 widgets)
shadcn/ui, GitHub Primer core components

### Phase 3: Data Visualization (40 widgets)
D3.js/Plotly chart types

### Phase 4: Specialized Domains (40 widgets)
Node editors, audio, social media, sports

### Phase 5: Editor & Advanced (43 widgets)
VSCode features, media, utilities

---

## Design Principles

### 1. Immediate-Mode API
```c
// Simple, stateless
if (stygian_button(ctx, "Click", x, y, w, h)) { ... }

// Advanced, with state
StygianButton btn = {...};
if (stygian_button_ex(ctx, &btn, &style)) { ... }
```

### 2. GPU-Accelerated SDF Rendering
- All widgets rendered as SDFs
- Smooth animations via distance field interpolation
- Single draw call per frame
- Metaball blending for organic transitions

### 3. Zero-Allocation Hot Path
- Arena allocators for per-frame memory
- Pool allocators for persistent elements
- No malloc/free in render loop

### 4. Tachyon Integration
- Custom allocator hooks
- Threading model (lock-free queues)
- Signal processing integration (IEC)

---

## File Organization

```
widgets/
├── core/           # Buttons, checkboxes, sliders (60)
├── text/           # Text inputs, editors (25)
├── selectors/      # Pickers, dropdowns (18)
├── data/           # Tables, lists, cards (20)
├── trees/          # Trees, hierarchies (8)
├── navigation/     # Menus, tabs, breadcrumbs (18)
├── overlays/       # Modals, tooltips, notifications (20)
├── layout/         # Containers, grids, splitters (15)
├── charts/         # All chart types (40)
├── nodes/          # Node editors, graphs (12)
├── audio/          # DAW controls, waveforms (15)
├── social/         # Feeds, chat, messaging (15)
├── sports/         # Football stats, analytics (12)
├── editor/         # Code editor, IDE features (15)
└── specialized/    # Media, utilities (17)
```

---

**Stygian will be the most comprehensive UI library ever created in C, rivaling web frameworks while delivering native performance.**
