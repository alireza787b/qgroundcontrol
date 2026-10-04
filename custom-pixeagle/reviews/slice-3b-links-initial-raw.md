# Raw simulated operator review — dashboard and help links

2026-09-26. Independent AI review from private-display screenshots, not a human
operator study. No host desktop capture or interaction. Reviewed 04-pixeagle-off,
05-help, 06-enabled, 07-dashboard-address, 08-custom-dashboard and 09-invalid-address.
Scope is these 1440×900 settings captures; no claim about phone/touch, high DPI,
compact layout or the Fly View Options shortcut.

- With PixEagle off, the page is sparse. Enable PixEagle is the first actionable
  choice and help remains available without enabling the integration. I do not
  see an intrusive launch prompt or a large group of new controls.
- Dashboard is visible near the section title when enabled. Its short label is
  useful: the pictured window icon alone could be mistaken for video/display.
  I would retain the label instead of optimizing this to an unexplained icon.
- The browser tooltip makes the outcome clear. The shortcut does not look like
  it changes camera mode, target or aircraft behavior.
- Help and links expands into one paragraph and two ordinary buttons. The two
  destinations are understandable. Documentation is the more useful first choice;
  Repository remains secondary. Text is readable and doesn't collide or truncate.
- Help's setup paragraph mixes sign-in, ports, HTTPS and aircraft association.
  It is somewhat dense, but acceptable as optional disclosure; I would not put it
  permanently into Fly View or add another mandatory onboarding step.
- Dashboard address is closed by default, so normal users do not have to decide
  between backend and dashboard URLs. When opened, its port/protocol rule and
  separate browser sign-in are explained in direct language.
- The checkbox presentation of Help and links / Dashboard address is mildly
  ambiguous: it looks like enabling a feature, although it reveals details.
  This follows other existing disclosure controls on this page, so it is a small
  future consistency improvement rather than a reason to add more UI now.
- Save and Use default are clear, with enough room around them. The saved custom
  reverse-proxy URL is fully visible in the provided screenshot. Invalid-address
  feedback is placed immediately below the related controls and explains what
  input is accepted. The prior valid destination should remain active on rejection.
- The automatic address uses gray placeholder text. This signals no override,
  but is less legible than the saved black text and is not copyable by selection.
  A browser-failure instruction to copy it would therefore be misleading.
- The enabled page is already fairly tall from sign-in and existing explanations.
  The new collapsed sections avoid a substantial new burden; expanded help/address
  naturally requires scrolling. No new layout blocker is evident in these images.

Overall: suitable to hand back with the user's accepted tracker workflow. Preserve
simple defaults and avoid discovery scans or another setup wizard. The specific
copy-address fallback should be corrected in wording; other points are observations,
not requested extra polish or claims of operator acceptance.
