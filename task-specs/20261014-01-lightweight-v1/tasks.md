# Execution Tasks

Read [proposal](proposal.md), [specs](specs.md), [design](design.md) and [evidence](verification.md).

## 1. Day-one environment and skeleton

- [x] 1.1 Confirm actual Qt/MinGW kit, FFmpeg/ffprobe paths, versions and encoders; AC-03/10.
- [x] 1.2 Create application/layer build files and basic window; compile and smoke-run; AC-01/10.
- [x] 1.3 Verify minimal release deployment early; AC-10.

## 2. Day-one single-file pipeline

- [x] 2.1 Implement bounded async import/probe/thumbnail and error handling; AC-01/02.
- [x] 2.2 Implement and verify output parameter generation for all choices; AC-03.
- [x] 2.3 Implement owned temp outputs, verification and safe commit; AC-05/09.
- [x] 2.4 Integrate actual conversion, fragmented progress, cancel and process shutdown; AC-06/07/09.

## 3. Day-one complete lightweight behavior

- [x] 3.1 Implement uniform batch snapshot, serial scheduling, failure continuation and counts; AC-04.
- [x] 3.2 Implement queue-edit locks, cancel-current versus stop-all, close prompt and stale-result guards; AC-01/04/06/07.
- [x] 3.3 Implement log display/copy and validated preference persistence; AC-08.
- [x] 3.4 Complete planned window presentation and generate a testable package; AC-01..10.

## 4. Day-two verification and repair

- [x] 4.1 Run meaningful Qt tests for arguments, fragmentation, file protection and queue/cancel lifecycle; AC-03..09.
- [ ] 4.2 Run real-media matrix, abnormal input/path/output/race cases and playback; AC-01..09.
- [ ] 4.3 Run clean-target package test, repair observed defects and rerun affected checks; AC-10.

## 5. Day-three delivery and reconciliation

- [x] 5.1 Reconcile Chinese plan, README, summary and test evidence against implemented behavior; AC-10.
- [x] 5.2 Prepare source/package inventory, dependency licenses, screenshots and short demonstration; AC-10.
- [ ] 5.3 Perform final package/repository checks and prepare HR delivery materials; AC-10. Do not send HR messages without explicit authorization.

## Handoff

- Current mode/revision: execute / 4; “开始实施” approves the selected libVLC backend while retaining Qt 5.12.12. Implementation and local native/isolated validation are complete; local packaging prepared by the updated script. Current evidence supersedes revision 3 for changed paths; human/independent-target acceptance remains pending.
- Actual progress: revision 3 application and both test targets build; 55 core and 10 native UI cases pass. Isolated deployment passes 10 native UI and 11 focused range/content cases. Local revision3 source/runtime packages are prepared; old GIF remains revision 1.
- Step 4.2 is partial: 18 real conversion combinations, orientation/odd/aspect/silent/HDR/path/cancellation/collision cases passed. Human playback, real camera quality, actual unknown-duration conversion and ACL denial still need evidence.
- Step 4.3 remains pending: revision3 sanitized PATH/deployed runtime passes 10 UI and 11 range/content cases on the current machine; no separate clean target available.
- Step 5.2: local source archive, Windows package/dependency inventory, license notes, screenshots and screenshot-montage GIF are prepared. A formal real-time video is a later delivery item, not claimed by the GIF.
- Step 5.3 remains pending: no GitHub publication or HR messages sent; local deliverables are reviewable.
- Workspace: source, build/test/package scripts and Chinese docs written; builds/tools/dist are ignored. Existing .gitignore rules preserved. No commits or pushes.
- Revision 4 is authorized and being implemented. C ABI dynamic loading avoids MSVC/MinGW import-library differences; actual runtime/source versions and hashes are verified. Prior DirectShow tests do not validate libVLC.
- No further implementation approval is required. Verify current native and isolated-runtime tests, reconcile docs and prepare local revision4 packages; publication/contact still requires explicit instruction.
- Independent-target, high-DPI, human sound/sync/quality and external delivery evidence remain outstanding.
- Revision 3 is implemented and locally verified. Remaining delivery acceptance is not marked complete or archived; new evidence supersedes historical counts for changed paths.

## 6. Revision 2 interface optimization

- [x] 6.1 Update the existing Chinese confirmation plan/navigation and English proposal/spec/design/tasks before product edits; AC-08/11.
- [x] 6.2 Implement full-body log tab, compact queue and shared status/actions; AC-08/09/11.
- [x] 6.3 Update meaningful native UI cases for tab continuity, minimum layout, long paths and log-page cancellation; rebuild, run, and inspect screenshots; AC-01/06/07/08/09/11.
- [x] 6.4 Reconcile README, screenshots, sole summary and evidence; retain prior unverified acceptance and release-package limitations; AC-10/11.

## 7. Revision 3 preview and segment conversion

- [x] 7.1 Update existing Chinese plan/navigation, five English records and sole summary; include sketch, flow, scope and evidence limits; AC-12..17 and modified AC-03/07/08/09/11. Documentation only, not implementation approval.
- [ ] 7.2 Actual Qt Multimedia/DirectShow modules, supported AVI/WMV playback/seek, output devices and minimal deployed playback verified; human audible sound remains unverified; AC-12, AC-14, AC-17. MP4 failure disclosed; no engine/Qt/system-codec replacement.
- [x] 7.3 Implement range value, controlled per-task Save/Cancel/reset, immutable batch ranges, list entry/display and locks; register files by layer; AC-03, AC-08, AC-11, AC-13, AC-16.
- [x] 7.4 Implement playback service, preview control/dialog, display binding/entrypoint ownership, controls, selected-stop, errors, read-only terminal and stale/close guards; AC-07, AC-12, AC-14, AC-16.
- [x] 7.5 Extend re-encoding args, target-duration progress and stricter range verification; preserve whole-file and output/cancel/stop safety; AC-03, AC-05, AC-06, AC-07, AC-09, AC-15.
- [x] 7.6 Add range/snapshot/content/lifecycle tests and fixtures; rebuild all affected targets, check changed-source format/standards, run 55 core/10 native UI cases and inspect screenshots; AC-03/05..09/11..16.
- [ ] 7.7 Partial: output RGB/audio-frequency content, real seek, dependency/license deployment and isolated-runtime 10 UI/11 range cases pass. Human audible playback/sync, independent target and high-DPI acceptance remain unverified; AC-10, AC-12, AC-14, AC-15, AC-16, AC-17.
- [x] 7.8 Reconcile same plan/navigation, README, dependencies, test records, screenshots, summary and evidence against actual behavior; retain incomplete acceptance and old GIF version labels; AC-10, AC-17.

## 8. Revision 3 preview diagnostics follow-up

- [x] 8.1 Connect source/probe context, playback state/errors/timeouts and threaded Qt multimedia warnings to bounded existing session logs; preserve handlers and session lifetime; AC-08/12/14/16.
- [x] 8.2 Build affected targets; verify actual backend failure detail/copy, success state deduplication, worker-thread and stale-session routing; 12 native and 12 isolated-runtime UI cases pass; reconcile docs/summary/evidence and prepare revision3-diagnostics artifacts; AC-08/12/14/16/17.

## 9. Revision 4 libVLC preview backend (authorized implementation)

### Creator runtime and latest release (2026-10-15; AC-17/18)

- [x] Diagnose Creator shadow directory and absent VLC run variable; verify official local runtime and deploy DLLs/plugins/data/licenses beside the actual Release EXE; preserve open IDE settings.
- [x] Add exact Creator run-environment/manual-copy instructions; update default latest package name and collect distinct timeline evidence.
- [x] Sanitized-PATH runtime passed 20 UI cases including timeline/MP4; runtime/source packages regenerated, 557 inventory entries and 387 tested runtime files match, archive CRC/content/source checks passed.
- [x] Superseded dist releases removed to Recycle Bin after successful verification; only latest runtime directory/ZIP/source ZIP remain. Permanent delete was rejected by automatic policy; reversible recycling succeeded. Docs/evidence/summary reconciled; independent target and direct Creator GUI acceptance remain unverified.

### Timeline interaction correction (2026-10-15; AC-12/14/16)

- [x] Reproduce actual slider inputs: thumb dragging passed; groove clicking and keyboard input failed before the fix (`build/ui-tests/timeline-before.txt`).
- [x] Repair absolute groove click/drag and non-pointer seek dispatch in preview_dialog.cpp/.h/.ui; preserve controller/backend and saved ranges; rebuild all three existing Release targets.
- [x] Five actual VLC timeline interaction rows pass (`build/ui-tests/timeline-after.txt`); clang-format and diff checks pass.
- [x] Full native UI regression passed 18/18; added start/end boundary rows passed 2/2 plus init/cleanup. Existing human docs, overview, sole summary and verification reconciled. Core conversion remains unchanged; retain prior core evidence. No new deployment package or independent-machine acceptance is claimed.

- [x] 9.1 Record user's Qt 5.12/libVLC choice in the same confirmation page and task records; disclose DirectShow MP4 failure and preserve revision 3 as historical evidence.
- [x] 9.2 Revision 4 approved and implementation authorized by “开始实施”.
- [x] 9.3 Verify official Windows x64 runtime/source SHA256, C ABI against matching headers, MinGW build/runtime, complete plugins and GPL/LGPL materials; use dynamic loading without compiler-specific import libraries; AC-18.
- [x] 9.4 Replace only the preview adapter with libVLC; preserve Qt dialog/range/controller contracts, FFmpeg conversion and session cleanup; add strict runtime preparation/packaging; AC-12/14/16/17/18.
- [ ] 9.5 Partial: native 55 core/13 UI, isolated 13 UI/3 actual-user-MP4/11 range-content cases pass; visible user MP4 and portrait screenshots checked; lifecycle/diagnostics, strict missing-runtime packaging and GPL/LGPL deployment verified. Human audible sound/sync, independent target and broad compatibility remain unverified; AC-03/07/08/09/12/14/15/16/17/18.
