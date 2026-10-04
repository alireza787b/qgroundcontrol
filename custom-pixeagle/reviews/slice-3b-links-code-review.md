# Focused independent code review — dashboard links

2026-09-26. Read-only review of dashboard URL additions in PixEagleManager.h/.cc,
the corresponding Settings controls and the Fly View Options shortcut. Reviewed
new manager tests for intended behavior; did not independently run them.

## Finding

Minor user-facing mismatch in `PixEagleSettings.qml::openDashboard`: failure text
says “Copy the dashboard address below.” With the default address, the field's
`text` is empty and the effective URL exists only in `placeholderText`; placeholder
text cannot be selected/copied. The disclosure may also be closed. Reword the
failure to ask the user to check Dashboard address, or explicitly expose a selectable
effective URL. Rewording is sufficient for this small feedback task.

## Reviewed properties

- Explicit overrides accept HTTP/HTTPS, require a host, and reject user info,
  query, fragment, invalid/zero ports and decoded path backslashes. No browser
  credential or QGC session is appended to outgoing links.
- Defaults derive protocol and host from the already validated client endpoint,
  replace the port with 3040 and remove any API path prefix. This is a documented
  convention, not frontend discovery; reverse-proxy deployments need an override.
- Overrides persist by canonical backend endpoint hash, including API port/prefix.
  Empty or rejected overrides leave the current valid destination intact; clear
  restores the default. Changing the dashboard URL does not reset authentication.
- Active client/endpoint changes emit dashboardUrlChanged. Settings clears the
  old edit text/error through that signal, reducing cross-vehicle edit confusion.
- The settings shortcut is hidden while integration is off and disabled with no
  endpoint. Fly View exposes its secondary shortcut inside Options and cancels a
  pending pointer gesture before opening the external browser.
- Links use an ordinary external browser; no embedded web engine or new polling,
  port scan or discovery traffic is introduced.

No blocking URL construction, credential leakage, per-endpoint persistence or
routing defect identified in the examined code. This focused review does not
qualify Android/Windows browser launching or other unexamined integration code.
