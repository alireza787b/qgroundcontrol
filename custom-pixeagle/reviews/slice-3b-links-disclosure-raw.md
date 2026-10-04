# Raw simulated UX consultation — disclosure semantics

2026-09-26. User feedback specifically rejects checkbox-looking disclosure items.
Independent AI consultation; not a real operator study. Earlier operator-links-raw.md
is preserved unchanged. Read local QGC SectionHeader.qml and SettingsGroupLayout.qml,
relevant usage locations, and Qt UI design skill principles. Also reviewed private
1024×768 app-window captures 10-compact-address and 11-compact-scrolled. The screenshot
canvas remains larger than the app, so black outside its window is not a product pane.

**Warning: a checked disclosure looks like a persisted enabled preference.** Help,
Dashboard address and Connection details change what content is visible; their current
checkbox shape gives the wrong expectation. The user's discomfort is consistent with
that mismatch, rather than a request to restyle every real boolean setting.

Concrete recommendation: use existing QGC **SectionHeader** for all three disclosures.
It displays a clickable text heading, separator and collapsed arrow without a visual
checkbox indicator. Use `checked: false`, full layout width and subordinate content
`visible: header.checked`. This reuses the pattern already found in QGCFileDialog,
ParameterEditor and PlanView, avoiding another menu/dialog or a custom accordion.
Override SectionHeader's default ClickFocus with StrongFocus and provide a visible
focus border so keyboard users can reach and operate it.

Keep **Enable PixEagle**, **Show PixEagle video in Fly View** and **Tap video to select
targets** as the existing boolean controls. They change persistent behavior and should
continue to look selected when true. Do not use a disclosure heading as an enable switch.
Keep Dashboard as a plainly labeled browser action near the page title, with its
secondary Fly View Options access; Help/diagnostics/address remain below normal tasks.

SettingsGroupLayout groups related rows but is not itself a disclosure component;
adding bordered groups would consume space without solving the semantic issue. A menu
is suitable for actions/links but cumbersome for editing an address and reading errors.
An extra dialog would introduce navigation cost for a small optional field. Neither is
needed for this focused change.

The compact captures show vertical scrolling and readable controls without horizontal
collision. The address controls become accessible after scrolling. Keeping disclosures
closed by default remains useful. This does not establish touch target sizing, screen
reader output, font scaling or translated layout acceptance on Android.

SectionHeader internally inherits CheckBox despite its correct visual presentation;
consider an explicit Accessible.Button role/name if needed, but preserve keyboard
Space activation and test whatever override is selected. Do not claim accessibility
acceptance from screenshot review alone.
