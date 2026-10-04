# Final raw simulated UX addendum — native disclosure controls

2026-09-26. Independent AI screenshot/code inspection, not human acceptance.
Earlier raw feedback remains unchanged. Reviewed private-display captures
12-final-collapsed.png, 13-final-expanded.png and 14-final-help-keyboard.png.
Capture 15 was not yet present at this review point. No shared desktop used.

**Disposition: pass for this focused feedback change; no blocking finding.**

- Dashboard address and Help and links now read as expandable section headings,
  with native QGC separator/arrow presentation. They no longer look like persisted
  enable preferences. Enable PixEagle, video visibility and tap selection correctly
  retain their boolean checkboxes.
- The collapsed page stays compact. Dashboard remains easy to find next to the
  title, and its visible label clarifies the browser icon.
- Expanded address instructions, field and Save/Use default remain grouped and
  readable. Additional height is handled by scrolling rather than overlap.
- Expanded Help keeps the existing brief setup guidance together. Its lower link
  controls require scrolling in this capture; the scroll affordance is visible.
  No new dialog, setup wizard or large always-visible information block was added.
- The supplied keyboard capture shows the Help heading outlined/active with its
  content expanded. Code uses StrongFocus and an Expand/Collapse accessible name
  with Button role. This review does not independently establish keyboard action
  coverage, measured focus contrast or screen-reader behavior.
- The browser-failure wording now says to check Dashboard address rather than
  telling users to copy an unselectable automatic placeholder. That resolves the
  small specific code-review finding.

The existing recommendation is satisfied. Do not extend this small usability pass
into another redesign. Test execution, browser launch interception and platform
release qualification remain separate evidence from these screenshots.
