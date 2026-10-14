# Added Requirements

Revision 3 is a documentation-only plan pending detailed review and implementation authorization. Revision 2 is implemented and locally verified. Unchanged requirements retain their historical evidence; new/modified behavior below is not implemented or verified.

## AC-01 File import

The application SHALL support file multi-select and file drop, reject directories, and skip duplicate normalized paths with an explanation. List edits SHALL be disabled while a batch is active.

Scenario: Import two videos, a duplicate and a directory. The list contains the two distinct files; duplicate and directory handling is visible. A drop during conversion does not mutate the active batch.

## AC-02 Media inspection

The application SHALL asynchronously display thumbnail, filename, duration, resolution, size, codecs and status. Missing audio SHALL be displayed as absent. Invalid media SHALL produce an actionable error. Thumbnail failure SHALL use a placeholder without blocking otherwise valid media.

Scenario: Import valid video, silent video and damaged media. The first two become ready after inspection; the damaged item displays a failure. The UI remains responsive.

## AC-03 Output settings

The application SHALL offer MP4/MKV, high/standard/smaller-size quality, and original/1080p-cap/720p-cap resolution as independent choices. A batch SHALL use one immutable snapshot of settings. Scaling SHALL preserve display proportion and avoid upscaling, subject to encoding alignment. The first version SHALL identify unsupported media rather than imply universal format/HDR support.

Scenario: Convert short media using all 18 combinations and inspect actual codecs, container and dimensions. Low-resolution video is not enlarged. A batch cannot acquire different settings from edits during conversion.

## AC-04 Serial batch and failure continuation

The application SHALL process eligible waiting items in import order, one at a time. An individual failure SHALL allow the next item to run. The batch SHALL report success, failure and cancellation counts. Terminal items SHALL not silently run again on another Start action.

Scenario: A three-item batch has an invalid/missing second input. First and third finish, second fails, and the summary agrees with task states.

## AC-05 Safe output handling

The application SHALL use a uniform output directory, `_converted` base names and numbered collisions. It SHALL never overwrite source files or existing outputs. It SHALL write owned temporary output first and commit only a validated result without overwrite. Failure/cancellation SHALL clean only owned temporary files or disclose cleanup failure.

Scenario: Same-stem inputs and an existing converted result produce separate final paths, with unchanged sources/existing files. An unwritable directory fails clearly. A collision at commit does not overwrite another file.

## AC-06 Cancellation

Cancel Current SHALL stop the current operation, settle its lifecycle and cleanup, then allow the next task. Stop All SHALL suppress further scheduling and mark pending batch items cancelled. Terminal outcomes SHALL be emitted once; cancellation SHALL not remove already committed successes.

Scenario: Cancel during conversion, then near completion/result inspection. Actual processes stop and late callbacks cannot mark the cancelled task successful or corrupt a later task. Stop All starts no next task.

## AC-07 Shutdown

The application SHALL ask before closing with active work. Continue Conversion SHALL keep the window open. Stop and Exit SHALL terminate owned processes and settle cleanup before exiting.

Scenario: Close during media inspection and during conversion. The chosen action is honored and no unowned processing process remains.

## AC-08 Session logs and preferences

The application SHALL expose readable errors and copyable session diagnostic logs in a separate full-body “运行日志” tab, reachable from the tab bar and a “查看日志” action. Logs SHALL continue to accumulate on the conversion tab. Switching tabs SHALL preserve the current queue, selection, settings, progress and active transaction. It SHALL persist output directory, format, quality and resolution, but not the queue/history. Previously, logs expanded in a bottom panel limited to 140 high.

Scenario: Copy an error log and restart. Valid saved settings are restored, the queue is empty, and invalid saved values use documented defaults with appropriate feedback.

Scenario: Switch to logs during an actual conversion. Recent diagnostics and shared progress are visible; Cancel Current and Stop All remain usable. Switching back retains the same queue and settings. Copy returns the bounded controller session log.

## AC-11 Compact two-page layout

The default window SHALL be 980x820, with a minimum of 880x720. The video list SHALL cap its width at 620 while retaining comparable vertical browsing space. Long filenames SHALL be elided with complete paths retained in tooltips, without requiring horizontal scrolling. The settings SHALL remain accessible by scrolling at minimum size. Both tabs SHALL show shared notices, current progress, summary and conversion controls. Errors SHALL not force navigation.

Scenario: Open long-name and damaged files, resize to minimum size, and switch both tabs. Logs use the body height, shared controls remain within the window, all settings are reachable, and full filenames/error details are available in tooltips.

## AC-09 Progress and trustworthy results

The application SHALL display current task progress and batch position. Unknown durations SHALL use indeterminate progress. Process progress completion SHALL not alone imply success. Success SHALL require normal exit, nonempty output, media verification and safe commit.

Scenario: Feed split progress records and unknown-duration media. Progress does not regress or leak between tasks. An exit failure, malformed output or commit failure never becomes success.

## AC-10 Testable delivery

The application SHALL have a reproducible qmake build and a release package with validated Qt/compiler/tool dependencies and actual version/source/license notes. README, screenshots, test evidence and the single conversation summary SHALL describe actual completion and limitations.

Scenario: On a target environment without Qt development installation, extract the release package, import and convert a sample, then locate/play the output. Record failure or unavailable clean-environment testing truthfully. Delivery to HR is prepared for day three; external messaging is a separate explicit action.

# Revision 3 ADDED Requirements (planned)

## AC-12 Separate input preview

The application SHALL open one separate modal preview dialog for exactly one inspected, valid, accessible input selected while the batch is idle. A labeled action and item double-click SHALL open the same dialog. The dialog SHALL offer audible playback when an audio track exists, pause, volume, seek and position/total duration, and remain usable for silent input. Multi-selection SHALL not choose an arbitrary input. Terminal items SHALL be preview-only and SHALL not be rescheduled.

Scenario: Select one waiting video, open preview, play/pause/seek and change volume; the queue/settings remain intact. Select two items: the entry is disabled with an explanation. Double-click a terminal item: its input opens read-only; conversion cannot be restarted by saving a range.

## AC-13 Per-item range transaction

Each waiting item SHALL default to whole-file conversion and MAY store one range in input-relative milliseconds. Set Start/End SHALL capture the current position; manual hours/minutes/seconds/milliseconds input SHALL also be supported. The dialog SHALL show range markers and selected duration. Saving SHALL require known duration, loaded playable/seekable media and `0 <= start < end <= source duration`. Invalid input SHALL explain the violation and prevent saving without changing the saved range. Save SHALL atomically commit only that item's draft; Cancel/Esc/window-close SHALL discard it. Reset to Whole SHALL affect the draft until saved. A new dialog SHALL show the last saved range.

Scenario: Save 10–35 seconds for A and retain whole-file B. Reopen A, alter it and cancel: A remains 10–35 seconds. Reset and save: A is whole-file. Negative, reversed, equal and out-of-duration values never commit. Remove/re-add A or restart: its range defaults to whole-file.

## AC-14 Selected-range preview and capability failure

Play Selected SHALL seek to the valid draft start and pause at its end without looping or converting. Whole-file playback SHALL remain independent of the saved conversion range. Unknown-duration or unseekable media SHALL disable range editing/saving with an explanation while retaining available playback and whole-file conversion. Playback errors SHALL be visible and disable playback-dependent controls/new range saves without invalidating otherwise convertible input or silently changing a saved range.

Scenario: Play a 10–35 second draft and observe start positioning and end pause. For unknown duration, failed seek capability or failed playback, no new segment can be saved; the user can cancel and convert an otherwise valid waiting item whole-file. A previously saved valid range is not erased by a later playback error.

## AC-15 Segment conversion fidelity

The application SHALL re-encode each saved range through the normal output pipeline using that item's immutable batch range and shared encoding settings. The output SHALL contain the selected input content and corresponding audio when present, with playback time starting at zero and no dependency on previous-keyframe-only cuts. Unselected items SHALL retain whole-file behavior. Output naming, safe temporary files, verification, cancellation, failure continuation and stop/exit rules SHALL apply equally to segments. Millisecond entry SHALL not imply frame-step editing or exact sample-boundary equality.

Scenario: Convert A's 10–35 second segment and whole-file B in one batch. Check visible time markers, audible markers, approximate 25-second duration, zero-relative playback, codecs and dimensions. Repeat near beginning/end, at non-keyframe start, with nonzero source timestamps, short/silent/rotated inputs and both containers. Cancel the first segment: no formal result is committed for it, and the next item follows existing policy.

## AC-16 Preview ownership and lifecycle

The preview dialog SHALL block main-window mutations/start/close while open. Closing any preview SHALL stop playback and release its media without changing conversion task state or leaving a preview window running. Repeated open/close SHALL ignore callbacks from previous sessions. Batch-active preview/range mutations SHALL be refused in both UI and control APIs. Application shutdown SHALL settle playback resources as well as the existing conversion lifecycle.

Scenario: Repeatedly open/close A then B, including closing during load/seek/error. No stale position/error updates reach B, files are released, and no active preview prevents later conversion or shutdown. During a real batch, both the UI and a direct range-mutation request reject editing; saved batch ranges stay fixed.

## AC-17 Playback deployment

The release package SHALL include the Qt playback libraries/plugins used by the chosen backend, record actual dependencies/licenses and disclose system playback limits. Playback, audio, seeking and conversion SHALL be verified using packaged dependencies with development paths removed; independent clean-target validation SHALL be recorded separately. Playback support SHALL not be advertised as equal to FFmpeg conversion support.

Scenario: With developer Qt paths removed, run the new package, preview with sound, seek, select/save a range and convert it. Missing/unsupported playback yields an actionable message and leaves eligible whole-file conversion available. No unperformed independent-machine check is reported as passed.

# Revision 3 MODIFIED Requirements (planned)

## AC-03 Output snapshot extension

In addition to the existing shared encoding snapshot, Start SHALL validate and snapshot all waiting items' saved ranges before scheduling any task. Invalid ranges SHALL name the affected item and reject batch start without partial execution. Range edits SHALL be rejected for active batches and terminal items. Previously, there were no per-item ranges.

Scenario: Start mixed whole/ranged items, then attempt a control-level range change: it is rejected. An invalid or out-of-duration saved range rejects Start and preserves the queue without starting FFmpeg conversion.

## AC-07 Shutdown extension

Existing guarded conversion shutdown SHALL remain unchanged and SHALL also release preview resources. Modal preview SHALL be saved/cancelled/closed before main-window exit is available. Previously, shutdown only handled tool/conversion work.

Scenario: Close preview while loading, then close the main window; no playback survives. Closing during segment conversion follows the existing Continue/Stop-and-Exit flow and cleans owned temporary output.

## AC-08 Session storage extension

Ranges SHALL belong only to their task in the current in-memory queue and SHALL NOT be stored in common output preferences or restored after restart. Saved ranges SHALL survive selection/tab changes, while remove/clear discards them. Existing preference keys/log retention SHALL remain unchanged. Previously, no ranges existed.

Scenario: Save A's range, switch tabs and select B, then return to A: A's range persists and B is unaffected. Restart: preferences persist but the queue/ranges are absent.

## AC-09 Target-duration progress and validation

Segment progress SHALL use selected duration and output-relative time, remain monotonic and cap below success until verification/commit. Validation SHALL compare against selected duration, preserving original input metadata. For known segments the accepted duration deviation SHALL be below half the selected duration and no greater than 0.5 seconds; actual media boundary calibration MAY tighten this bound. Wrong-content cuts SHALL be detected by acceptance fixtures even when duration matches. Existing whole-file/unknown-duration behavior remains. Previously, progress and validation used whole-input duration with a whole-file tolerance.

Scenario: A 25-second segment from a two-minute input completes without a false short-output failure or 20% final progress. A short-range output missing at least half its duration or containing seconds of extra material is rejected. Marked content verifies that correct duration alone does not prove the correct segment.

## AC-11 Main-window entry and range presentation

The compact two-tab window SHALL retain its existing default/minimum size, shared controls and accessible settings while adding the preview entry and per-item whole/range/time-length display with complete tooltips. Preview layout SHALL remain usable at its declared minimum and preserve the video's display aspect/orientation. Previously, the list had no range entry/display.

Scenario: At main-window minimum size, use long filenames/ranges and both tabs: preview entry, list and conversion controls remain reachable. Open landscape/portrait preview and verify controls, text and video fit without overlap.
