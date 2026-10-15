# Task: VideoConversion lightweight v1

- ID: 20261014-01-lightweight-v1
- Status: verifying
- Workflow mode: execute
- Plan revision: 4
- Approval status: approved
- Approved revision: 4
- Previous planning basis: The user selected the complete lightweight version, added high quality and 1080p, and then stated “都按你推荐的，我觉得挺好的，第一版先这样定下来”. This approves all ten recommended product behaviors from the preceding reply. It does not authorize implementation or establish approval of previously unseen technical defaults.
- Approval basis: Revision 1 was authorized by “根据整体项目开发方案进行开发”. Revision 2 was authorized by “先更新文档再修改” for the log-tab/compact-layout change. For revision 3, “按推荐方案来，先更新文档” selects a separate preview window and one range per video and authorizes documentation only. The user subsequently stated “我已经阅读了文档，开始改第三版”, approving the documented revision 3 and authorizing implementation.
- Updated: 2026-10-14
- Playback interaction correction (2026-10-15): user reports inability to drag the timeline to choose playback time. Repair the existing revision 4 seek contract under this bug-fix request; no backend, conversion or saved-range changes. Existing staged work is preserved. Native tests show thumb dragging works but groove clicks and keyboard changes never seek because only sliderReleased is connected. Scope: preview_dialog.cpp/.h/.ui, UI regressions and related existing documentation. No subagents or external actions.
- Authorized deployment follow-up (2026-10-15): user requests Qt Creator dependency setup guidance, updated release artifacts and retaining only the latest packages. Diagnose missing VLC run path, deploy verified runtime beside the actual Creator shadow-build executable, document environment-variable setup, regenerate the latest runtime/source package with timeline evidence, validate isolated runtime and delete only superseded dist release artifacts after new-package validation. No app/backend API change, external publication, Qt installation modification or global environment change.
- Docs language: Chinese
- Task-specs language: English
- Human documentation: [Integrated plan](../../docs/确认页/项目开发/整体项目开发方案.md)
- Plan routing: revise-existing; continue the same product record and integrated confirmation page for preview/segment conversion.
- Routing basis: Related-plan search found only this maintained product plan and its revision 2 UI follow-up. The requested addition extends the same import/conversion workflow; no parallel task or confirmation page is needed. Historical initial discovery found no earlier plan; that is not the current workspace state.

## Intent

Deliver an approachable Windows desktop video converter for a three-day hiring exercise. Complete development and a testable package on day one, test and repair on day two, and prepare delivery to HR on day three.

## Scope

In scope: retained lightweight conversion behavior plus a separate modal input-preview dialog, sound/play/pause/seek/volume, one optional saved range per waiting item, selected-range preview, immutable batch ranges, range-aware re-encoding/progress/result validation, resource lifecycle, playback dependency deployment, focused tests and the single maintained conversation summary. Revision 4 replaces only the Qt Multimedia/DirectShow playback backend with libVLC while retaining Qt 5.12.12, the UI, range workflow and FFmpeg conversion. The user's “开始实施” approves and authorizes this selected revision.

Out of scope: folder import, per-file encoding settings, queue/range edits or preview during a batch, parallel conversion, multi-segment editing/concatenation, frame-step editing, lossless stream-copy trimming, hardware encoding, tray operation, persisted queue/ranges/history, resumable conversion, embedded AI chat, upgrading Qt, requiring system codec installation, publishing or external messaging. An alternate backend beyond the user's selected libVLC is not authorized by this revision.

## Approach

Retain Qt 5.12.12 Widgets / MinGW 7.3 x64, C++17, qmake and asynchronous FFmpeg/ffprobe conversion. Revision 4 uses VLC 3.0.24's Windows x64 C ABI through dynamic DLL loading, avoiding compiler-specific import libraries. Native HWND output and GUI-thread state polling retain the existing preview contract. Decoder-thread diagnostics are bounded and queued with session generations; log_unset waits for in-flight callbacks before teardown. VLC runtime and source archives have verified official SHA256 values; deployment and regression evidence are tracked in verification.md. Revision 3 DirectShow evidence is historical.

## Acceptance Criteria

AC-01 import; AC-02 inspection; AC-03 output settings; AC-04 serial batch; AC-05 safe outputs; AC-06 cancellation; AC-07 shutdown; AC-08 logs/preferences; AC-09 progress/results; AC-10 portable delivery. Contracts and scenarios are in [specs](specs.md).

Revision 2: AC-11 compact two-page layout. Revision 3 adds AC-12 playback entry/controls, AC-13 transactional per-item range, AC-14 range preview/capability failures, AC-15 segment conversion fidelity, AC-16 preview lifecycle and AC-17 playback deployment. AC-03/07/08/09/11 are extended as specified; original cancellation/file-safety contracts continue to apply.

Revision 4 modifies AC-12/14/16/17 to use libVLC without changing their user-visible contracts, and adds AC-18 for toolchain, runtime/plugin and license validation. Approved by “开始实施”; no further backend-selection approval is needed.

## Standards

- User-provided AGENTS instructions: search before reading, trace affected asynchronous/resource chains, avoid repeated reads/checks, report evidence limits. Authority: current user message.
- QT-CPP-001: `C:/Users/yang/.codex/skills/qt-cpp-engineering/references/cpp-coding-standard.md`; read 2026-10-14; SHA256 `88AB56BAA546A3A8159D05D215B8484BDC5D5D355B22AAB68ACC19A621D3E0FA`. Applies to all project C++/Qt code and review. Named namespaces, lowercase source filenames, four-line headers, natural Chinese contract/stage comments and explicit ownership apply. Implemented C++ reviewed against these rules.
- QT-PRI-001: `C:/Users/yang/.codex/skills/qt-cpp-engineering/references/工程分层pri规范.md`; read 2026-10-14; SHA256 `1E05F0093E8BA2445435BEF66511A2ACC8C70B4FEEDEFC0DD0F22DCED0590723`. Applies to the proposed single-target application. Separate test target is not forced into the desktop target's source lists.
- No additional project standards files were found. Process protocol/device examples in the source standards are not video-conversion requirements.

## Decisions and Remaining Validation

Approved: complete lightweight product, all ten recommended behavior groups, high-quality and 1080p additions, day-one development/day-two tests/day-three delivery, exactly one conversation-summary file.

Proposed defaults: libx264 CRF 18/23/28 with fast preset, AAC 128 kb/s, MP4/standard/original defaults, first real video and first audio, limited timeouts, simple light UI, re-add to retry. These defaults are covered by the user's authorization to develop the integrated plan. All 18 combinations passed real short-fixture conversion, probe and decode; visual quality/performance remain unverified.

Execution authorized for revisions 1 and 2 in their recorded scopes. Actual Qt 5.12.12 / MinGW 7.3 x64 and FFmpeg dependencies are verified; local implementation and applicable checks are complete. No subagents, commits, pushes or HR communication authorized or performed.

Revision 3 approved and implemented decisions: independent modal preview, one range per file, re-encoding through the existing pipeline, common encoding settings retained; session-only ranges, terminal items read-only, no preview during batches, explicit capability failures preserving whole-file conversion and saved ranges, no proxy generation. Actual DirectShow compatibility limits are disclosed; conversion support is broader than playback. The original three-day target is historical; additional work has no verified completion estimate.

Revision 4 user decision: “选择libVLC，本地qt环境是5.12”, followed by “开始实施”. Preserve Qt 5.12 and use libVLC as the embedded preview backend. C ABI signatures and lifecycle are checked against the matching VLC 3.0.24 source headers; complete plugins and GPL/LGPL materials enter the package inventory. Human sound/synchronization and an independent clean machine remain separate acceptance limits.

## Plan Changes

- Revision 3 → 4, 2026-10-14: after DirectShow `ResourceError` / `0x80040266`, the user selected libVLC and then authorized “开始实施”. The same plan is now being implemented and verified. Prior revision 3 evidence remains historical and does not validate libVLC.

- Revision 2 → 3, 2026-10-14: user requested preview and choosing a segment during playback, selected the recommended scheme and asked for documents first. After reading the same updated Chinese plan, the user authorized implementation: “我已经阅读了文档，开始改第三版”. Approved revision is 3. New/changed ACs and files are listed in specs/design/tasks. Affected conversion/progress/validation paths were rebuilt and retested rather than relying on revision 1/2 evidence.
- Standards recheck: bundled index and both sources retain the recorded SHA256 fingerprints on 2026-10-14; no additions were found. User reading/verification instructions remain active. No global skill files changed.

## Execution Outcome

Revision 3 diagnostic follow-up is explicitly authorized by “帮我增加对应日志，辅助查看因为什么无法打开”. Retain revision 3; this extends the existing AC-08/12/14 diagnostics without changing playback engine, conversion semantics or UI layout. Preview context/state/errors and Qt multimedia warnings now enter bounded session logs, preserving prior Qt handlers and rejecting stale queued messages. All three Release targets build; full native and isolated-runtime UI suites each pass 12 cases, including real failure/copy, worker-thread/lifecycle routing and bounded backend messages. Latest artifacts are revision3-diagnostics runtime/source packages; original revision3 archives remain pre-diagnostic builds.

Revision 3 is implemented in all three layers, build registrations, fixtures, native/core tests, deployment script and affected documentation. Release application and both test targets build; 55 core and 10 native UI cases pass. With developer paths removed, deployed dependencies pass 10 native UI and 11 focused range/content cases. Marked-content tests exposed input-side seeking losing video references in nonzero-timestamp MPEGTS; output-side `-ss`/`-t` passed all 8 content rows, with the disclosed cost of decoding preceding input. New preview screenshots were visually inspected. Local Windows/source packages are prepared under `dist/VideoConversion-1.0.0-win64-revision3*`; old montage GIF remains revision 1. No commits, publication or external messaging occurred. Overall task remains verifying due to independent-target, high-DPI, human sound/sync/quality and external delivery gaps, not incomplete local product implementation.

Revision 2 UI follow-up: documentation updated before implementation, then reconciled with results. Scope: `Application/main_window.ui/.cpp`, existing native UI tests, README, affected screenshots, confirmation navigation, task records and the sole conversation summary. No Control/Interface API, process ownership, queue, encoding, log retention or preference changes. Prior core/encoding evidence remains valid; affected screenshots are regenerated. Release builds, changed-file clang-format checks and all 7 native UI cases passed. Latest executable: `build/app/release/VideoConversion.exe`. Existing release archives and montage GIF remain revision 1 and are explicitly labeled; no repackaging was performed. Overall task remains verifying because prior independent-machine/human acceptance is still outstanding.

Revision 1 product development is implemented. Release build, 41 core test cases and 6 native UI cases passed; portable dependency isolation repeated 6 UI cases successfully. A local Windows package and screenshots are prepared. Task remains verifying because required clean-target deployment and human playback/quality evidence are unavailable, and external GitHub/HR delivery is not performed. No approval question or archival is needed for completed local development.

## Revision 3 execution authorization and backend preflight

User instruction: “我已经阅读了文档，开始改第三版”. This approves revision 3 and authorizes its implementation. Actual Kit smoke built Qt Multimedia/MultimediaWidgets successfully. DirectShow played and sought MJPEG/PCM AVI and WMV2/WMA samples; H.264/AAC MP4 failed with ResourceError / DirectShow 0x80040266. Continue the already documented unsupported-playback fallback; no engine replacement, codec installation or Qt change. Audio output devices were enumerated; audible sound has not been human verified. Logs: build/playback-smoke/playback.log.
