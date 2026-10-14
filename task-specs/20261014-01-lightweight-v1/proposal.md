# Task: VideoConversion lightweight v1

- ID: 20261014-01-lightweight-v1
- Status: ready
- Workflow mode: revise
- Plan revision: 3
- Approval status: pending
- Approved revision: 2
- Previous planning basis: The user selected the complete lightweight version, added high quality and 1080p, and then stated “都按你推荐的，我觉得挺好的，第一版先这样定下来”. This approves all ten recommended product behaviors from the preceding reply. It does not authorize implementation or establish approval of previously unseen technical defaults.
- Approval basis: Revision 1 was authorized by “根据整体项目开发方案进行开发”. Revision 2 was authorized by “先更新文档再修改” for the log-tab/compact-layout change. For revision 3, “按推荐方案来，先更新文档” selects a separate preview window and one range per video and authorizes documentation only. Detailed revision 3 contracts are pending review; product implementation is not authorized.
- Updated: 2026-10-14
- Docs language: Chinese
- Task-specs language: English
- Human documentation: [Integrated plan](../../docs/确认页/项目开发/整体项目开发方案.md)
- Plan routing: revise-existing; continue the same product record and integrated confirmation page for preview/segment conversion.
- Routing basis: Related-plan search found only this maintained product plan and its revision 2 UI follow-up. The requested addition extends the same import/conversion workflow; no parallel task or confirmation page is needed. Historical initial discovery found no earlier plan; that is not the current workspace state.

## Intent

Deliver an approachable Windows desktop video converter for a three-day hiring exercise. Complete development and a testable package on day one, test and repair on day two, and prepare delivery to HR on day three.

## Scope

In scope: retained lightweight conversion behavior plus a separate modal input-preview dialog, sound/play/pause/seek/volume, one optional saved range per waiting item, selected-range preview, immutable batch ranges, range-aware re-encoding/progress/result validation, resource lifecycle, playback dependency deployment, focused tests and the single maintained conversation summary. This turn changes planning documents only.

Out of scope: folder import, per-file encoding settings, queue/range edits or preview during a batch, parallel conversion, multi-segment editing/concatenation, frame-step editing, lossless stream-copy trimming, hardware encoding, tray operation, persisted queue/ranges/history, resumable conversion, embedded AI chat, publishing or external messaging. Replacing Qt/playback engines or requiring system codec installation is not authorized by this plan.

## Approach

Retain verified Qt 5.12.12 Widgets / MinGW 7.3 x64, C++17, qmake and asynchronous FFmpeg/ffprobe conversion. Prefer Qt Multimedia for independent playback, subject to actual Kit/backend/deployment checks. Preserve Application → Control → Interface; snapshot each item's range separately from uniform encoding preferences. Kit/encoder availability is verified for existing conversion; new playback support and independent clean-target deployment remain unverified.

## Acceptance Criteria

AC-01 import; AC-02 inspection; AC-03 output settings; AC-04 serial batch; AC-05 safe outputs; AC-06 cancellation; AC-07 shutdown; AC-08 logs/preferences; AC-09 progress/results; AC-10 portable delivery. Contracts and scenarios are in [specs](specs.md).

Revision 2: AC-11 compact two-page layout. Revision 3 adds AC-12 playback entry/controls, AC-13 transactional per-item range, AC-14 range preview/capability failures, AC-15 segment conversion fidelity, AC-16 preview lifecycle and AC-17 playback deployment. AC-03/07/08/09/11 are extended as specified; original cancellation/file-safety contracts continue to apply.

## Standards

- User-provided AGENTS instructions: search before reading, trace affected asynchronous/resource chains, avoid repeated reads/checks, report evidence limits. Authority: current user message.
- QT-CPP-001: `C:/Users/yang/.codex/skills/qt-cpp-engineering/references/cpp-coding-standard.md`; read 2026-10-14; SHA256 `88AB56BAA546A3A8159D05D215B8484BDC5D5D355B22AAB68ACC19A621D3E0FA`. Applies to all project C++/Qt code and review. Named namespaces, lowercase source filenames, four-line headers, natural Chinese contract/stage comments and explicit ownership apply. Implemented C++ reviewed against these rules.
- QT-PRI-001: `C:/Users/yang/.codex/skills/qt-cpp-engineering/references/工程分层pri规范.md`; read 2026-10-14; SHA256 `1E05F0093E8BA2445435BEF66511A2ACC8C70B4FEEDEFC0DD0F22DCED0590723`. Applies to the proposed single-target application. Separate test target is not forced into the desktop target's source lists.
- No additional project standards files were found. Process protocol/device examples in the source standards are not video-conversion requirements.

## Decisions and Remaining Validation

Approved: complete lightweight product, all ten recommended behavior groups, high-quality and 1080p additions, day-one development/day-two tests/day-three delivery, exactly one conversation-summary file.

Proposed defaults: libx264 CRF 18/23/28 with fast preset, AAC 128 kb/s, MP4/standard/original defaults, first real video and first audio, limited timeouts, simple light UI, re-add to retry. These defaults are covered by the user's authorization to develop the integrated plan. All 18 combinations passed real short-fixture conversion, probe and decode; visual quality/performance remain unverified.

Execution authorized for revisions 1 and 2 in their recorded scopes. Actual Qt 5.12.12 / MinGW 7.3 x64 and FFmpeg dependencies are verified; local implementation and applicable checks are complete. No subagents, commits, pushes or HR communication authorized or performed.

Revision 3 selected decisions: independent modal preview, one range per file, re-encoding through the existing pipeline, common encoding settings retained. Detailed defaults proposed for review: session-only ranges; terminal items preview read-only; preview disabled during batches; unknown-duration/unseekable/failed playback falls back to whole-file conversion; no proxy generation. Playback backend choice is a preference pending validation, not a compatibility claim. The original three-day target is historical; additional work has no verified completion estimate.

## Plan Changes

- Revision 2 → 3, 2026-10-14: user requested preview and choosing a segment during playback, then selected the recommended scheme and asked for documents first. Updated the same Chinese plan/navigation and English records plus the sole session summary. New/changed ACs and intended files are listed in specs/design/tasks. Approved revision remains 2; revision 3 detailed plan approval and implementation authorization are pending. Previously passed evidence remains historical revision 1/2 evidence and does not validate changed conversion/progress/validation paths.
- Standards recheck: bundled index and both sources retain the recorded SHA256 fingerprints on 2026-10-14; no additions were found. User reading/verification instructions remain active. No global skill files changed.

## Execution Outcome

Revision 3: planning documents only. No source/build/dependency/script/test fixture changes, builds, runtime playback checks, package replacement, commits or publication. New evidence is limited to document structure/link/consistency checks recorded in verification.md. Overall delivery remains incomplete; `ready` describes the revised plan, not completed implementation or acceptance.

Revision 2 UI follow-up: documentation updated before implementation, then reconciled with results. Scope: `Application/main_window.ui/.cpp`, existing native UI tests, README, affected screenshots, confirmation navigation, task records and the sole conversation summary. No Control/Interface API, process ownership, queue, encoding, log retention or preference changes. Prior core/encoding evidence remains valid; affected screenshots are regenerated. Release builds, changed-file clang-format checks and all 7 native UI cases passed. Latest executable: `build/app/release/VideoConversion.exe`. Existing release archives and montage GIF remain revision 1 and are explicitly labeled; no repackaging was performed. Overall task remains verifying because prior independent-machine/human acceptance is still outstanding.

Revision 1 product development is implemented. Release build, 41 core test cases and 6 native UI cases passed; portable dependency isolation repeated 6 UI cases successfully. A local Windows package and screenshots are prepared. Task remains verifying because required clean-target deployment and human playback/quality evidence are unavailable, and external GitHub/HR delivery is not performed. No approval question or archival is needed for completed local development.
