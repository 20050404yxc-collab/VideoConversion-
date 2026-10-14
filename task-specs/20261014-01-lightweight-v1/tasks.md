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

- Current mode/revision: execute / 2; authorized by “先更新文档再修改” for the displayed UI recommendation, with revision 1 scope retained.
- Actual progress: day-one product development, release build and testable package done; 41 core and 6 native UI tests passed. Minimal deployment was checked after integrated development rather than at the originally intended early milestone.
- Step 4.2 is partial: 18 real conversion combinations, orientation/odd/aspect/silent/HDR/path/cancellation/collision cases passed. Human playback, real camera quality, actual unknown-duration conversion and ACL denial still need evidence.
- Step 4.3 remains pending: sanitized PATH/bundled runtime smoke and 6 portable GUI tests passed on the current machine; no separate clean target available.
- Step 5.2: local source archive, Windows package/dependency inventory, license notes, screenshots and screenshot-montage GIF are prepared. A formal real-time video is a later delivery item, not claimed by the GIF.
- Step 5.3 remains pending: no GitHub publication or HR messages sent; local deliverables are reviewable.
- Workspace: source, build/test/package scripts and Chinese docs written; builds/tools/dist are ignored. Existing .gitignore rules preserved. No commits or pushes.
- Required input for current development: none. Later acceptance requires target-machine and human playback results; publication/contact requires explicit instruction.
- Next action: revision 2 local UI work is finished. Later delivery work should generate an updated package and obtain prior target-machine/human acceptance; no required UI-development input remains.
- Task remains verifying, not completed/archived; unrun required checks are not marked passed.

## 6. Revision 2 interface optimization

- [x] 6.1 Update the existing Chinese confirmation plan/navigation and English proposal/spec/design/tasks before product edits; AC-08/11.
- [x] 6.2 Implement full-body log tab, compact queue and shared status/actions; AC-08/09/11.
- [x] 6.3 Update meaningful native UI cases for tab continuity, minimum layout, long paths and log-page cancellation; rebuild, run, and inspect screenshots; AC-01/06/07/08/09/11.
- [x] 6.4 Reconcile README, screenshots, sole summary and evidence; retain prior unverified acceptance and release-package limitations; AC-10/11.
