# Raw UX addendum — existing Help and About conventions

2026-09-26. Independent AI review, not a human study. This recommendation follows
read-only inspection of local QGC/PixEagle source, rather than inventing a new
attribution convention. The parent agent separately researched official QGC/Qt
web references; this addendum makes only the local-source claims below.

Evidence:

- QGC `src/AppSettings/HelpSettings.qml` groups descriptively labeled User Guide
  and community links and opens them externally.
- PixEagle `dashboard/src/components/NavigationDrawer.js` already uses an **About
  PixEagle** dialog, project link and **Close** action. Its live version/runtime
  fields come from versionInfo and should not be copied with guessed local values.
- PixEagle `LICENSE` explicitly states **Copyright 2024-2025 Alireza Ghaderi** and
  Apache License, Version 2.0. This supports the exact copyright attribution, not
  an automatically updated year or an invented separate creator credit.

Recommendation: replace generic Help and links with collapsed **Connection help**
for the local setup instructions and a **Documentation** action. Add a modest
**About PixEagle** footer button that opens native `QGCPopupDialog`: brief project
description, exact verified copyright attribution, **Source code**, **License**,
and ordinary **Close**. This separates practical setup help from project identity
and legal/source information using familiar application conventions.

The About action should work when PixEagle integration is disabled; it reads
static project information and launches no backend request. Do not replace the
stock QGC About dialog or conflate the PixEagle project's license with QGC's.
Do not present a build/backend version unless the actual value and its component
are known. No new system/about fetch or discovery feature is needed for this task.

Keep the Dashboard shortcut near the page title and its optional Web dashboard
settings separate. No new always-visible cluster of repository/creator/license
buttons is necessary on the normal connection form.

Disposition: this is clearer and more conventional than a catch-all Help and links
section. It fits the user's requested small full-page refinement and does not add
another project slice or require new product architecture.
