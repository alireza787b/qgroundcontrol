# Slice 3b links and settings review disposition

The [accepted session review](slice-3b-accepted-session-raw.md) preserves the
user's local acceptance separately from simulated UI reviews.

| Finding/request | Decision |
| --- | --- |
| Quick dashboard access | Compact labeled browser action in Settings and Fly View Options; documented port-3040 default, per-backend full URL override. No service guessing or session forwarding. |
| Checkboxes resemble enabling Help/settings | Replaced non-boolean toggles with QGC SectionHeader for Web dashboard and Connection details. Only real preferences retain checkboxes. |
| Whole-page copy/hierarchy | Shorter Backend address and QGC vehicle labels; duplicate aircraft paragraph removed; Video and targeting groups the two operational preferences. Connection errors/association status remain visible. |
| Conventional About/help | Connection help uses QGC's standard popup/factory with setup guidance and documentation. About PixEagle uses the same native dialog, exact published copyright, Source code and License links. No invented creator claim, current year, backend version or new About API query. |
| Failed browser launch told user to copy a placeholder | Corrected to check the address under Web dashboard. |
| Keyboard focus | New actions/disclosures use StrongFocus and visible QGC palette outline; headings expose Expand/Collapse names. |
| Hardware next | Existing Topotek SIP UDP and RTSP path recovered from local checkpoint records. No hardware action in this task. |

Initial checkbox screenshots and raw interpretations remain archived. Final
private screenshots cover the revised page, About/help dialogs and 1024×768
scrolling. URL interception checks the handoff to the system browser, not the
availability of arbitrary custom websites. Native product controls and QGC
palette/dimensions are reused. No shared desktop capture/input was performed.

## Primary patterns and attribution

- QGC [HelpSettings.qml](https://api.qgroundcontrol.com/master/HelpSettings_8qml_source.html)
  and [Settings guide](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/settings_view/settings_view.html): descriptive documentation/support links.
- Qt [Menus example](https://doc.qt.io/qt-6/qtwidgets-mainwindows-menus-example.html): Help/About separation.
- Local QGC `SectionHeader.qml`, `QGCPopupDialog.qml` and
  `QGCPopupDialogFactory.qml`: native disclosure/dialog behavior and lifetime.
- PixEagle `NavigationDrawer.js`: existing About PixEagle / Project / Close.
- PixEagle [LICENSE](https://github.com/alireza787b/PixEagle/blob/main/LICENSE):
  copyright and Apache 2.0 attribution, not the license of the combined QGC app.

These informed this specific design; they do not establish user or platform
acceptance. At the user's request, no demo is opened for this checkpoint. The
latest page changes will receive user feedback at the next camera/gimbal
checkpoint described in [the next-slice plan](../SLICE-4-PLAN.md).

## Immutable review artifacts

Raw responses are preserved byte-for-byte. The full-page review contains a bare
example URL flagged by markdownlint MD034, and the accepted-session review has a
typos false positive within its audit run identifier. Product and authored-doc
lint is reported separately; these two raw-artifact findings remain recorded in
the checkpoint rather than editing the feedback or claiming all-file lint passed.
