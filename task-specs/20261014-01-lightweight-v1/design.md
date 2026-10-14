# Implementation Design

## Current State

The application now builds with actual Qt 5.12.12 / MinGW 7.3.0 x64. Interface, Control and Application layers and independent core/UI test targets are implemented. Gyan FFmpeg/ffprobe 9.0.2 essentials was downloaded and validated. Tests use real tool processes and isolated settings/fixtures.

## Architecture and Files

- `VideoConversion.pro`: Qt core/gui/widgets, C++17 and target configuration, three layer includes only.
- `Application/Application.pri`: `main.cpp`, `main_window.h/.cpp/.ui`, resources if used. Entry point assembles lower layers; windows use Control for business actions.
- `Control/Control.pri`: `conversion_controller.h/.cpp`, `conversion_types.h`, `output_file_manager.h/.cpp`, `settings_store.h/.cpp`.
- `Interface/Interface.pri`: `ffmpeg_service.h/.cpp`, `ffmpeg_arguments.h/.cpp`, `progress_parser.h/.cpp`, `media_types.h`.
- `tests/tests.pro` and focused Qt Test sources for arguments, progress and controller/file lifecycle. Reuse actual production code, not duplicated logic.
- `scripts/build_release.ps1`, `scripts/package_release.ps1`: preferred-kit build and deploy, verified tool copies and package inventory.
- `tools/README.md`: actual FFmpeg source/version/encoder/redistribution notes during execution. Binaries may be local dependencies or release assets depending on size; do not accidentally commit large binaries.
- `.gitignore` may be updated for build/cache/package paths; `README.md` becomes the actual product manual during execution.

Dependencies flow Application → Control → Interface. Controller types depend on media value types only as needed. Interface never uses Control/UI. Each layer owns a responsibility-commented `.pri`, `$$PWD` include path and complete source/header/form lists. Explicit QObject ownership and GUI-thread-only UI access apply.

## Process API and Ownership

Controller provides conceptual operations add/remove/clear/start/cancel-current/stop-all and signals for items, progress, logs, batch state and summary. Stable identifiers and run generations accompany async requests/results. Only the controller commits task state and schedules subsequent work.

FFmpeg service owns async QProcess instances for tool readiness, probe, thumbnail, convert and output verification. It receives programs/argument lists and media settings; no shell concatenation. Separate channels distinguish `-progress pipe:1` from stderr logs. Partial lines/records persist across reads; buffers/logs are bounded. Inspect/thumbnail jobs are serial or otherwise bounded, not one process per dropped file.

Settings store uses QSettings with validated enum/default values. Controller snapshots settings at batch start. Output manager owns the temp/final path transaction; UI receives actionable diagnostics rather than implementing file policy.

## Implemented Encoding Defaults

Use MP4/MKV, libx264 `fast` with CRF 18/23/28, yuv420p, AAC 128 kb/s when audio exists; MP4 adds faststart. Default MP4/standard/original. Map the first non-attached video and first optional audio; omit subtitles, extra tracks and attachments. Validate actual build capabilities before implementation relies on them.

Fit display-oriented dimensions into landscape 1920×1080 or portrait 1080×1920 for 1080p, and corresponding 720p bounds; preserve aspect, avoid upscaling and satisfy even-dimension encoding constraints. Resolve sample-aspect/rotation handling against actual FFmpeg outputs. Unsupported HDR/special media receives an explicit limitation rather than silent color-loss claims. Validate exact filter/argument syntax with real tools before treating it as implemented.

## Lifecycle and Files

Task states: inspecting, waiting, converting, verifying, cancelling, succeeded, failed, cancelled. Batch states: idle, running, stopping. Media-inspection rejection is visible and does not prevent other eligible tasks. Wait for outstanding inspection before starting a batch. Only waiting tasks start; retry uses remove/re-add in idle state.

Cancellation policy is recorded before asking a process to stop. Stop All disallows new scheduling first. Async stop followed by bounded forced termination is necessary on Windows; do not assume terminate succeeds. Suggested initial timeouts: probe/thumbnail 15 seconds, result probe 20 seconds, stop escalation 2 seconds. No short fixed total-conversion timeout. Late results are ignored by identifier/generation; exactly one terminal result per run.

Closing also covers inspection processes, not only conversion. Choosing exit waits asynchronously until stop/cleanup settles, while keeping QObject lifetimes valid. Never block UI in waitForFinished/ReadyRead.

Create unique temporary files in the chosen output directory with the appropriate container suffix or explicit muxer. Before start, validate source existence, eligible streams and writable destination. Resolve normalized/canonical source path protection. Candidate final path is stem_converted plus collision numbering. Success requires normal exit/nonempty file and ffprobe validation; verify codecs, streams, bounded resolution and duration when known. Initial duration tolerance may be max(2 seconds, 2% of input duration), to be confirmed against samples and stream timestamps. Commit using no-overwrite same-directory rename, retrying collision allocation if necessary; any other commit failure is a failure with owned-temp cleanup. Never roll back already committed results during later cancellation.

Known-duration progress is monotonic and capped below completion until verification/commit. Unknown duration is indeterminate. FFmpeg reports absolute output times: normalize units and timestamps before deriving percentages. Parser tests protect fragmentation/record boundaries, not just regular stderr text.

## Documentation and Verification

The Chinese integrated plan and its overview are the reviewable user artifact. Root `会话总结.md` is the only maintained session summary. These task-specs are execution records, not additional conversation summaries. Reconcile all affected documents with actual implementation and evidence.

Verify AC-01..10 with targeted Qt tests and real FFmpeg/media/UI/package cases. Exercise all 18 output combinations with short fixtures, plus cancellation/races/paths/collisions. Playback and deployment require real environment evidence; static or mocked checks do not establish them. Day-one milestones include builds and a testable package; day two includes fixes; day three includes final materials. No functional reduction is silently allowed to meet the time estimate.

## Implemented Details and Evidence Limits

Revision 2 changes the default window from 1120x820 to 980x820, retaining minimum 880x720. Use a QTabWidget with conversion and log pages; move the existing body layout into the conversion page and the existing persistent logView/copy action into the log page. Cap queueGroup at 620 and retain the settings scroll area (300..340 wide). Use Qt right-elision and disable list horizontal scrolling. Move Open Output into the shared action row and View Logs beside the shared notice; all progress/summary/cancel/stop/start widgets stay outside the tab widget. No new source files or .pri changes are needed; uic is regenerated by the build.

The controller owns bounded session logs and emits logAdded regardless of the selected page; the window owns both pages and receives diagnostics/progress through existing context-bound connections on the GUI thread. Switching pages never constructs another controller/service, changes state or resets logs. Native tests will verify imports/errors/copy, hidden-page accumulation, minimum geometry, long-name elision, active-conversion switching/cancellation and existing guarded close. Rebuild the release app and UI target, inspect new screenshots and check affected C++ formatting. Prior core tests remain valid because Control/Interface are untouched.

Long-name verification exposed that QListWidget's natural row width can exceed the viewport even with horizontal scrolling disabled. A window-local, list-owned QStyledItemDelegate retains the standard row height but requests zero natural width, letting the view allocate viewport width and Qt render right-elision. It uses the existing source file and ownership tree; no new layer/interface or file-list change.

Native Windows GUI tests cover close choices. Qt 5.12 offscreen QMessageBox rendering crashed and is excluded from GUI acceptance. Cleanup failures retain residual paths in OutputFileManager and task/log diagnostics, then release the current transaction slot so the next task can continue. Request identifiers are monotonic; per-operation QProcess instances and a coalesced event-loop pump prevent stale callbacks and recursive synchronous failures. shutdownReady is emitted once.

Packaging includes manuals, screenshots, summary, task evidence, actual Qt/MinGW/FFmpeg license files, tool provider README, version output and SHA256 inventory. Main application startup and native tests with PATH restricted to Windows used package dependencies; this does not substitute for a separate clean target.
