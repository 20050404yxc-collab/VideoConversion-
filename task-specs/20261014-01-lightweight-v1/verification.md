# Verification Record

## Environment and Scope

Executed 2026-10-14 in `D:/code/VideoConversion`, Windows x64 / PowerShell. Qt 5.12.12 from `D:/Qt/5.12.12/mingw73_64`, qmake 3.1, MinGW GCC 7.3.0 x64 from `D:/Qt/Tools/mingw730_64`. Gyan FFmpeg/ffprobe 9.0.2 essentials from the same archive, SHA256 `60f467265b1e312373dbcd92200c2618a74850f98d3d078e94296bb3fa2047ba`; libx264/AAC checked using actual encoder output. Provider source commit: `946fcce07b`.

## Acceptance Evidence

| AC / standard | Method / location | Result | Practical limitation |
|---|---|---|---|
| Build / layers | `scripts/build_release.ps1 -Tests`, actual qmake Kit | Passed, release application and both test targets built; no compiler warning/error in final logs | Windows x64 only |
| AC-01/02 | Core import tests and native UI file-drop events; actual damaged/HDR/silent/oriented fixtures | Passed duplicate/directory skips, async metadata/image, separate invalid-item errors, Chinese/space paths | Native file dialog manually selecting files not separately automated; synthetic drop follows actual window handlers |
| AC-03/09 | `outputMatrix`: 18 data rows using actual production controller and FFmpeg, followed by ffprobe and complete frame decoding | Passed all 18 combinations, actual H.264/AAC, MP4/MKV, expected dimensions and duration | Primarily 1.2-second synthetic sample; not a quality/performance benchmark |
| AC-03 edges | `mediaEdges`: portrait/silent, 90-degree rotation, 321x181 FFV1, sample aspect 2:1 | Passed real conversions and complete output decoding | Does not establish arbitrary rotation, HDR mapping or every camera format |
| AC-09 parser/validation | Byte-at-a-time progress, CRLF, historical microsecond field, reset, invalid numeric, timestamp fallback; wrong codec/duration/container rejected | Passed; records emitted only when complete | Real unknown-duration input conversion not run |
| AC-04 | Missing second input after probe in three-item batch; locks and duplicate Start | Passed two successes/one failure, next task continues, no terminal rerun | Imported invalid-media failures are separate from eligible batch counts |
| AC-05 | Two prepared file transactions with same stem, preexisting sentinel and commit-stage collision; cleanup; existing file as invalid output directory | Passed no overwrite/source deletion; terminal output kept | Actual system ACL-denied output directory not run; Windows MoveFileW used for no-overwrite same-directory commit |
| AC-06 | Cancel at actual conversion progress and at Verifying; Stop All repeatedly at verification; inspect `.vc-*` residues | Passed current cancellation continues, stop suppresses next task, unique summary, clean owned temps | No exhaustive randomized event fuzzing |
| AC-05/06 cleanup failure | Hold temp with Win32 handle denying delete sharing, cancel first task, allow next task | Passed residual location appended without hiding cancellation, next task succeeded, locked residual preserved then released/removed by test | OS failure reproduced using real Windows handle sharing, not an actual antivirus product |
| AC-07 | Shutdown during startup/probe/conversion plus native MainWindow continue/stop-and-exit choices | Passed actual service idle and no owned temporary output after completion; shutdown notification once | Forced OS task termination outside app shutdown contract not claimed safe |
| AC-08 | Invalid INI enums/empty directory, setting round-trip, GUI setting save and clipboard log | Passed, queue not persisted, exactly four setting keys | Runtime preference location is per-user QSettings |
| AC-10 core tests | `build/tests/test-results.txt` / `.xml` | 41 passed, 0 failed, 0 skipped; 26.468 seconds | Includes init/cleanup and data rows |
| AC-10 native UI tests | `build/ui-tests/test-results.txt` / `.xml` | 6 passed, 0 failed, 0 skipped; 4.708 seconds | Includes init/cleanup; actual native Windows Qt backend |
| AC-10/11 rendering | Inspect regenerated default/minimum/long-name/log-page screenshots; assert visible bounds and settings scroll visibility | Revision 2 passed; default 980x820, minimum 880x720, queue cap 620; default list browsing height >=350, minimum log height >=300 | No separate high-DPI monitor available |
| AC-10 packaged dependencies | Copy package runtime to build/deployment-check, add test harness/Qt5Test only, remove developer paths and tool override, run native UI suite | 6 passed with bundled Qt, plugins and tools, 7.176 seconds; independent packaged application also stayed running after launch | Same OS machine; not an independent clean target; test harness adds Qt5Test only |
| Standards | clang-format dry-run/Werror, direct ownership/state/callback/file review, header/namespace/trailing-comment/effective-function-size audit | Passed final source scope | Tests complement code review; no real hardware is involved |

## Observed Problems and Repair Evidence

1. First compile found a missing direct QStringList include in media parsing. Added include and rebuilt successfully.
2. Initial HDR synthetic fixture omitted actual HDR VUI metadata. Regenerated with x264 `colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc`, verified via actual ffprobe, and passed the intended rejection test. The original fixture did not prove HDR support/rejection.
3. Qt 5.12 offscreen backend emitted missing-font warnings and crashed rendering QMessageBox. Native Windows close test and complete UI suite passed. Offscreen GUI execution is not accepted as verified; no product state change was required for this backend limitation.
4. Visual inspection found settings overlap with logs open at small size. A scrollable settings panel plus minimum size/hint space removed overlap; production UI and real visibility assertions passed afterward.
5. A cleanup failure retaining the active output slot could make subsequent tasks fail prepare. Residual ownership is recorded, active slot released after error disclosure, and the real Windows handle-sharing regression passed.
6. Hidden independent app launch has no visible MainWindowHandle, and posting WM_CLOSE did not produce an application exit during the bounded hidden-start check. Those probe processes were stopped and no task tools remained. This check is recorded as a hidden-harness limitation/hypothesis (Qt last-visible-window behavior), not as successful standalone graceful shutdown. Native GUI close tests above are the evidence for application shutdown.

## Documentation and Delivery

### Revision 2 UI Follow-up

Authorization: user “先更新文档再修改”. Updated existing confirmation page and proposal/spec/design/tasks before changing product files, then reconciled README/navigation/summary/evidence. `scripts/build_release.ps1 -Tests` rebuilt the release application and UI tests; after the final delegate repair, incremental app/UI rebuilds passed without compiler warnings/errors. clang-format 23.1.3 native binary at `build/format-runtime/clang_format/data/bin/clang-format.exe` passed `--dry-run --Werror` on `Application/main_window.cpp` and `tests/ui_tests.cpp`; the launcher shim failed to import its Python module and was replaced by the bundled native binary.

The final native Windows `ui_tests.exe` run passed **7 cases, 0 failed, 0 skipped**, including import/damaged-item log accumulation and copy, actual conversion locks/results, default/wide/minimum layout and long filename tooltip/elision, real log-page Cancel Current with next-item success and selection/settings retention, and guarded close. Raw results: `build/ui-tests/test-results.txt/.xml`. Build logs: `build/build-ui-revision2.log`, `build/revision2-final-app-build.log`, `build/ui-tests/revision2-build.log`. No core suite rerun was needed: Control/Interface and their contracts are unchanged; retain prior 41-case evidence.

An initial layout test used a 400-pixel internal-list bound; screenshot review showed this incorrectly included group title/margins/hint. Corrected the bound to the original approximately 350-pixel browsing area. A genuine long-name failure then exposed Qt natural row width exceeding the viewport despite disabled scrolling. The list-owned standard-delegate subclass now constrains natural width and preserves row height. Targeted minimum-layout rerun passed, followed by the complete final UI suite.

Visually inspected default conversion, active-conversion logs, minimum-size logs/settings and long-name screenshots. Updated screenshot files 01..05 plus `06-converting-log.png` and `07-long-filename.png`. README labels the old GIF as revision 1; existing dist archives remain revision 1. Latest release executable is `build/app/release/VideoConversion.exe`; this task did not repackage or publish. Source review checked named namespace, delegate QObject ownership, context-bound callbacks, GUI-thread access, unchanged log retention, body comment coverage and <=80 effective lines in changed functions. Documentation links and XML form are checked locally.

Chinese README, integrated plan, navigation, test record, dependency/license notes and the sole root conversation summary reconciled. The same task/revision retained; user “根据整体项目开发方案进行开发” authorizes revision 1. Source files use the bundled coding/layering standards, no subagents were used, and original .gitignore rules remain.

`dist/VideoConversion-1.0.0-win64-reviewed.zip` is the local testable Windows package; `dist/VideoConversion-1.0.0-win64-reviewed-source.zip` contains source/manuals/tests/scripts without tools/build fixtures. The package includes Qt/MinGW/FFmpeg license texts, provider README/source reference, actual versions, SHA256 inventory, screenshots/GIF and test evidence. The GIF is a montage of real test screenshots, not a real-time recording. No Git commits/pushes/publication or HR contact occurred.

## Unverified Items and Residual Risks

- Revision 2 local UI checks are complete; high-DPI/independent-machine acceptance remains unverified. Existing dist archives and montage GIF are revision 1, explicitly disclosed; regeneration is later delivery work.

- Required independent clean Windows target run: not available; current-machine sanitized PATH is useful dependency evidence but cannot meet that acceptance alone.
- Human playback, audio/video synchronization and quality/size/time decisions on camera/phone footage: not performed; complete decode is not a substitute.
- Actual ACL-denied directories, real unknown-duration media conversion, wider special-media compatibility and high-DPI screen: not run.
- Public source/download URL, formal recorded video and HR delivery: not performed; no external communication authorized.
- Task remains verifying. Product development is implemented; incomplete acceptance is not silently marked complete or archived.
