# Implementation Design

Revision 3 is implemented under explicit user authorization. This design records actual range/playback ownership and conversion behavior; verification.md distinguishes local passing evidence from remaining human/independent-target acceptance.

## Current State

The application now builds with actual Qt 5.12.12 / MinGW 7.3.0 x64. Interface, Control and Application layers and independent core/UI test targets are implemented. Gyan FFmpeg/ffprobe 9.0.2 essentials was downloaded and validated. Tests use real tool processes and isolated settings/fixtures.

Revision 4 authorized implementation: retain that toolchain and replace only Qt Multimedia/DirectShow preview with libVLC 3.0.24 win64. The C ABI is resolved from matching DLLs with explicit signatures checked against the matching source headers, avoiding import-library/toolchain coupling. Official runtime/source archives are SHA256 verified. Deployment and regression results are maintained in verification.md.

## Architecture and Files

### Timeline interaction correction (2026-10-15)

Deployment follow-up: Creator's saved run environment contains only VIDEOCONVERSION_TOOLS and its executable lives under D:/code/build-VideoConversion-Desktop_Qt_5_12_12_MinGW_64_bit-Release/release. The playback adapter looks beside the EXE first, then at VIDEOCONVERSION_VLC. Deploy verified core DLLs/plugins/data and license/provenance into that actual output directory so the already-open IDE need not reload .pro.user. Document the environment-variable alternative for all configurations; leave IDE settings and global PATH untouched. Update the package default/name and copy distinct timeline/isolated evidence, verify runtime with only Windows PATH and no explicit tool/VLC variables, then remove old dist release artifacts within a checked absolute dist boundary. No C++ or build-layer change is needed for this follow-up.

Keep all changes in Application: PreviewDialog installs its lifetime-bound event filter only on positionSlider. Enabled left-button press/move/release maps pointer coordinates through QStyle groove/handle geometry, honoring horizontal inversion/RTL and clamping to the ends. Set sliderDown before moving; update sliderPosition before releasing so sliderReleased submits the final target through PreviewController::seek. valueChanged handles keyboard/wheel only outside a drag; existing QSignalBlocker prevents backend refresh feedback. Existing controller cancellation, native seek, GUI-thread ownership and close paths remain unchanged. The .ui increases hit height and assigns meaningful steps/accessibility name. No files or .pri lists are added; header/form changes require moc/uic rebuild. Native UI tests exercise actual VLC positions rather than slider appearance, plus the existing full UI regression.

- `VideoConversion.pro`: Qt core/gui/widgets, C++17 and target configuration, three layer includes only.
- `Application/Application.pri`: `main.cpp`, `main_window.h/.cpp/.ui`, resources if used. Entry point assembles lower layers; windows use Control for business actions.
- `Control/Control.pri`: `conversion_controller.h/.cpp`, `conversion_types.h`, `output_file_manager.h/.cpp`, `settings_store.h/.cpp`.
- `Interface/Interface.pri`: `ffmpeg_service.h/.cpp`, `ffmpeg_arguments.h/.cpp`, `progress_parser.h/.cpp`, `media_types.h`.
- `tests/tests.pro` and focused Qt Test sources for arguments, progress and controller/file lifecycle. Reuse actual production code, not duplicated logic.
- `scripts/build_release.ps1`, `scripts/package_release.ps1`: preferred-kit build and deploy, verified tool copies and package inventory.
- `tools/README.md`: actual FFmpeg source/version/encoder/redistribution notes during execution. Binaries may be local dependencies or release assets depending on size; do not accidentally commit large binaries.
- `.gitignore` may be updated for build/cache/package paths; `README.md` becomes the actual product manual during execution.

Dependencies flow Application → Control → Interface. Controller types depend on media value types only as needed. Interface never uses Control/UI. Each layer owns a responsibility-commented `.pri`, `$$PWD` include path and complete source/header/form lists. Explicit QObject ownership and GUI-thread-only UI access apply.

## Revision 4 Playback Adapter

Keep the existing preview dialog, preview-controller contract, range value and FFmpeg service. Interface owns the VLC instance and per-session media/player. Application binds a native QWidget/ HWND and owns all Qt controls; Control owns session generations, capabilities and selected-playback boundaries, without VLC types or widget operations. A 100ms GUI timer polls state/position and drives range boundaries; a 10s timer bounds loading. Initial decode is muted, paused on first Playing, then restores the saved volume. Ended keeps the known duration and seekability; end-of-file seek restarts the input muted and applies its pending seek when Playing resumes. Decoder-thread log callbacks copy bounded text, queue to the QObject and reject disposed generations; the limit is 100 warnings/errors plus a truncation notice. Teardown stops timers, unsets logging (VLC waits for in-flight callbacks), stops player before unbinding HWND, releases player/media, and releases the instance before unloading DLLs on service destruction.

Runtime preparation uses the official VLC 3.0.24 ZIP and matching source tarball, pinned SHA256 values and per-file comparison with the archive. package_release.ps1 requires provenance and verifies every runtime file before copying core DLLs, full plugins, hrtfs/lua and GPL/LGPL materials. VLC discovers plugins relative to its DLL directory; the obsolete --plugin-path option is excluded. Only VLC 3.x x64 is accepted because VLC 4 changes native API signatures. No system VLC, Qt upgrade or codec installation is introduced. Actual user MP4, seek/range/lifecycle and sanitized deployment evidence are required; human audible sound and independent-target acceptance remain explicit limits.

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

## Revision 3 Pre-change Evidence (historical)

- `ConversionTask` in `Control/conversion_types.h` contains source metadata/state, but no range. `ConversionSettings` is uniform encoding plus output directory; QSettings must not gain per-file range keys.
- `ConversionController::startBatch` snapshots uniform settings and waiting IDs. `beginConversion` passes full source/options to `conversionArguments`; the progress callback divides output seconds by source duration. `completeConversion` calls `validateOutput` against full source duration. All three paths need range-aware changes, not just the FFmpeg arguments.
- `Interface/ffmpeg_arguments.cpp` has no trim parameters. `validateOutput` allows `max(2 seconds, 2% of source duration)` for whole-file results; preserve that baseline for whole-file output and use a stricter explicit segment target.
- `Application/main.cpp` owns service/store/controller/window in dependency lifetime order. `MainWindow::refreshTasks` rebuilds list rows while preserving selection; use task IDs rather than row indexes or retained vector-element pointers for the dialog.
- `VideoConversion.pro` includes core/gui/widgets only. Qt Multimedia availability, Windows backend codecs, media seek accuracy and deployment are not verified by the existing conversion checks.

## Revision 3 Technical Approach and File Scope

Qt Multimedia from the actual Qt 5.12.12 Kit is implemented with Windows DirectShow. MJPEG/PCM AVI and WMV2/WMA playback/seek pass locally; H.264/AAC MP4 fails with ResourceError / 0x80040266. Use the approved explicit error/whole-conversion fallback. No Qt upgrade, third-party player, system-codec installer, FFplay external window, FFmpeg-library decoder or preview proxy is included.

Historical revision 3 files and responsibilities (playback implementation superseded by revision 4 above):

| Layer / file | Change and responsibility |
|---|---|
| `Interface/media_range.h` (new) | Passive optional range value, explicit enabled flag, input-relative integer milliseconds; shared without upper-layer dependencies |
| `Interface/media_playback_service.h/.cpp` (new) | Own QMediaPlayer; async load/play/pause/seek/volume/unload, position/duration/seekability/status/error signals, media session generation; never mutate conversion tasks |
| `Control/preview_controller.h/.cpp` (new) | Preview session/task ID, capability gating, selected-playback boundary state, stale-event guards and close settlement; no QWidget dependency |
| `Application/preview_dialog.h/.cpp/.ui` (new) | QVideoWidget, draft/time controls, modal Save/Cancel, aspect-preserving layout, read-only terminal mode |
| `Application/main.cpp` (modified) | Assemble playback service/preview controller, lifetime ordering and display binding |
| `Application/main_window.h/.cpp/.ui` (modified) | Single-selection entry/double-click, task-ID initialization and atomic commit via Control, range text/tooltips and locks |
| `Control/conversion_types.h`, `conversion_controller.h/.cpp` (modified) | Saved range, edit validation, immutable batch ranges, target-duration progress/verification, shutdown integration as needed |
| `Interface/ffmpeg_arguments.h/.cpp` (modified) | Explicit range input and target-duration verification; retain raw source metadata |
| `VideoConversion.pro`, three layer `.pri` files (modified) | Multimedia/multimediawidgets modules and source/header/form registrations; rerun qmake; main .pro retains only project configuration/layer includes |
| `tests/conversion_tests.cpp`, `tests/ui_tests.cpp`, `tests/tests.pro`, `tests/ui_tests.pro` (modified) | Production-path range/transaction/batch/fidelity/lifecycle/native UI tests; source/module registrations |
| `scripts/prepare_fixtures.ps1`, `scripts/package_release.ps1` (modified as needed) | Marked non-keyframe/nonzero-timestamp/short samples and actual playback plugin/runtime inventory |
| `README.md`, `docs/依赖与许可证.md`, `docs/测试记录.md`, screenshots (implementation-stage only) | Actual new usage, verified playback limits, dependencies and evidence |
| Existing Chinese plan/navigation, five task files, `会话总结.md` | Update now, reconcile in place after implementation; do not duplicate records |

Keep QWidget out of Control and upward includes out of Interface. Application owns the display widget; entrypoint or a narrow Control-mediated Qt display binding attaches it to the Interface-owned player. The dialog sends business controls through PreviewController, not direct player transport calls or FFmpeg commands. Detach the surface before destruction; a Qt-native view binding is not permission for lower layers to depend on the dialog.

## Revision 3 Range and Conversion Flow

Store a disabled default or explicit start/end. Zero is a valid start; unknown duration is not a fake end. Draft is a dialog-owned copy; do not retain vector-element references across events. Only ConversionController commits a waiting item's range after ID/state/batch/capability/time checks. Reset is draft-only until Save; selection/tab changes preserve saved data, remove/re-add/restart discards it. Terminal preview cannot commit/reschedule.

Start validates all waiting ranges before scheduling, names invalid items and refuses partial start. Snapshot ID/range/expected duration separately from shared encoding; derive progress/verification from it. Refuse range edits while running even from non-UI callers. Preserve original MediaInfo for mapping/sizing.

Use output-side `-ss` and `-t` after `-i` with normal re-encoding. Input-side seek lost reference frames/video on nonzero-timestamp long-GOP MPEGTS; decoding preceding input fixes all marked-content rows but increases cost for late ranges. User zero is normal input playback start; output starts at zero. Include corresponding audio and preserve silent/rotation/size rules and owned temp/verify/no-overwrite commit. No intermediate cropped files or stream-copy promise. Progress uses output-relative seconds / selected duration, monotonic and capped at 99 until commit. Segment duration deviation is <=0.5 seconds and strictly below half the selected duration; whole-file tolerance is unchanged. Eight real content rows inspect RGB/audio frequency as well as probe/decode; human audio/video synchronization remains unverified.

## Revision 3 Preview Lifecycle and Failures

Diagnostic follow-up: `MediaPlaybackService` emits source/session-tagged diagnostics; `PreviewController` connects them to `ConversionController::recordPreviewDiagnostic`, reusing timestamping, bounded storage and existing log/copy presentation. Probed metadata and open refusals are recorded at the business boundary. State summaries exclude position and are deduplicated; failure-detail keys allow a later actual error to supplement an earlier InvalidMedia event. A mutex-protected process Qt-message router preserves the previous handler, captures multimedia categories/DirectShow/QMediaPlayer/media-service warnings, and queues immutable generation-tagged text onto the service QObject. Unload disables capture before destroying the backend; late queued events are discarded, QObject context drops delivery after destruction. Per-load routing is limited to 100 backend messages before queueing, with explicit truncation. The last service restores the original Qt handler; unhandled output retains formatted stderr. No guessed codec diagnosis, decoder/engine change or persistent log file is introduced.

App-side QObjects remain on the GUI event thread with asynchronous decoding. MainWindow owns one modal dialog/session; parent/stack ownership and context-bound callbacks protect close paths. Session generation identifies loads so old callbacks cannot update a new dialog. Invalidate/disconnect/drain old session events before reuse; labeling every delayed signal with the current generation is not sufficient.

Only idle batches and valid inspected inputs can open preview. Other queued inspection may continue on the separate FFmpeg service; playback never steals its process slot. Modal dialog prevents main Start/remove/close until settled; Control rechecks the batch lock. Save/Cancel/X/Esc stop/unload media and detach display before destroying the dialog. Reopening must not leave sound or file handles active. Shutdown releases preview resources and retains existing async tool settlement; no synchronous GUI waits.

Whole-file Play and Play Selected are separate states. Selected playback validates draft/capabilities, positions asynchronously, then plays after the start position is established, not from stale position during seek. Position/end-of-media events pause once at the selected end; normal Play may traverse the full file. Seek/edit/close cancels selected-playback state as appropriate; no frame-perfect timer/slider claim.

Known duration, seekability and playable media gate range edits/saves. Unknown duration, unseekable media, backend/source duration differences >500ms or errors show feedback; preserve saved range and otherwise eligible whole-file conversion without automatic range reset/proxy generation. Loading has a 10-second timeout. Selected playback waits up to 2 seconds for position within 100ms of the requested start before playing; position granularity is backend-dependent. Endpoints use inspected input duration. A fresh QMediaPlayer per load is disconnected/unloaded/deleted before reuse; a zero-timer session token loads only after display binding.

## Revision 3 Verification and Documentation

New/modified ACs require new evidence. First smoke actual modules/backend/play/audio/seek; dependency failure does not authorize another engine. Test Save/Cancel/reset, task isolation, invalid times, terminal/batch refusal, snapshots, mixed full/segment output, short validation, progress and cancellation. Real marked videos cover beginning/middle/end, non-keyframe/nonzero timestamp, sound/silent and both containers. Complete decoding alone does not prove synchronization.

Native UI/render checks cover selection, minimum/default geometry, long text, landscape/portrait and repeated close during load/seek/error with file-handle release. Mock playback may check state logic but cannot prove backend or deployment. Rebuild affected targets, check changed C++ standards/format and run relevant core/UI regressions because all layers change. Broaden only for new changes/failures/unresolved risks. Test packaged dependencies without developer paths; retain separate high-DPI/clean-target limitations.

The same Chinese integrated plan/navigation, README, dependency notes, screenshots/test records and sole summary are reconciled. Current app and revision3 package/source archives contain revision 3; the old montage GIF remains revision 1. Native 55-core/10-UI and isolated-runtime 10-UI/11-range results are recorded separately. No publication or HR contact included.
