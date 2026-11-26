
## Fix: Attendance period "+" jumped to 8 (2026-11-26)
* Cause: the period selector was a GtkSpinButton (1-8). GTK spin buttons auto-repeat while the arrow is held (first repeat after the
  initial timeout, then every ~20 ms, accelerating towards the page increment). Each step re-loads the session (~10-15 ms), so a slightly
  slow click ran on to the limit (8). The change handler itself was verified to step exactly 1 per call.
* Fix (V2 only, `ui_attendance.c`): the period is now a "-" / value / "+" control built from ordinary buttons. A button emits one
  "clicked" per click, never repeats. Range 1-8 (buttons disable at the limits). Session + subject load/lock logic is unchanged.
* Not changed: the low-attendance threshold spin button in the Summary tab still uses a stock spin button.
ubject pass = 40/100 (configurable: `settings.csv`
  keys `grade_min_0..5`, and "Passing marks" per subject). SGPA = sum(credits x grade points)/sum(credits);
  Environmental Science (0 credits) is excluded from SGPA only.
* Teacher account: Ms. PPS Faculty, pps.faculty@example.edu, PPS (U26CS101), Demo Institute of Engineering and Technology.

## Done
- [x] Data model: subject faculty/credits/units, exam maximums, teacher subject/institution, grade scale, SGPA
- [x] Data migration v1 -> v2 (idempotent): backup folder `data_backup_v1_<time>`, `migration_report.txt`, students kept,
      35 students added, old subjects/exams/marks/attendance replaced, conflict reporting
- [x] Marks sheet is now ONE ROW PER STUDENT (55 rows): subject totals, total, %, SGPA, overall result; examination filter (Overall/UT-1/UT-2/Final); View Student (read-only, double-click) and Edit Marks windows (all 18 marks, live totals/grades/SGPA, atomic validation); CSV marks import; ranking with SGPA
- [x] Subjects page with faculty column, View Subject window (details, exam pattern, syllabus)
- [x] Dashboard (8 cards incl. average SGPA, charts, top performers, attention list), students table (overall %, SGPA)
- [x] Student profile (8 cards, subject table with UT-1/UT-2/Final/Total/Grade/GP/Credits/Status, attendance bars)
- [x] Mathematical Analyzer updated for all students/subjects (matrix 55 x 6, totals option, zero-credit weight 0)
- [x] 9 reports + CSV export (teacher/institution header); native Windows file dialogs kept
- [x] `make test`: 943 automated checks pass (grading S-F incl. decimals, sheet rows, exam filters, filters, SGPA/credits/fail rule, atomic editing, data preservation, plus all earlier checks)
      matrices 55 x 6, reports, CSV import/export, persistence, migration, empty dataset)
- [x] Application compiles with no errors

## Not verified by hand (no UI automation allowed) — please check
- Every page visually with the new data: Dashboard, Students, Subjects (+View Subject), Marks (filters, entry, import),
  Attendance, Math Analyzer (4 tabs, 55-row tables), Reports (9 types, export), Profile
- Performance of tab switching on a slow PC (tables are grids of labels; the Math tabs rebuild when their selection changes)

## Notes / limitations
- Marks percentages in the examination trend are per examination (UT-1, UT-2, Final), not cumulative.
- "Add examination" was removed from the Marks page to keep the fixed 20/20/60 pattern (`exam_add` still exists).
- Attendance category thresholds: 90 / 80 / configurable threshold (75).
- Grade scale and passing marks are editable only through `settings.csv` / the subject dialog (no settings screen).
- Migration conflict detection only compares record counts and subject codes; edited values of the old demo
  marks cannot be detected — the originals are always kept in the backup folder.
- Env var `CLASSMETRICS_PAGE=0..7` opens a chosen page after login (used for screenshots).

## Marks / grading update (2026-11-26, later)
- Grading replaced everywhere by S/A/B/C/D/E/F (central functions `grade_index/grade_of/grade_points_of/grade_performance/grade_legend` in marks.c);
  obsolete labels (O, A+, B+) no longer exist in source or screens. Existing marks and attendance were NOT changed
  (data files compared byte-for-byte with a backup taken before the change).
- Blank box in Edit Marks removes that mark (there is no separate delete button any more).
- Not verified on screen by me: the new Marks sheet, View Student window, Edit Marks window, alternating row colours on all tables.

## View Student / Marks sheet presentation update (2026-11-26, latest)
- View Student window: fixed header, ONE always-visible vertical scrollbar (policy NEVER/ALWAYS, overlay scrolling off) holding the
  nine 3 x 3 summary cards and the examination table, fixed Edit Marks (purple) / Close buttons. Table columns share the width and
  labels are ellipsized so there is no horizontal overflow by design (not verified visually).
- Cards: Total Marks, Overall %, SGPA, Class Rank, Earned Credits, Subjects Passed/Failed, Classes Attended (all subjects incl. EVS), Blood Group.
- New OPTIONAL student field `blood` (6th column of students.csv, editable in the student form, "Not specified" when empty). Existing
  students were not given a blood group. The file only gains the column the next time data is saved (backup taken before the change).
- Main Marks sheet: only failed subject cells (light red background, red text) and the overall result are coloured; Overall Result shows
  Pass / Fail (n) with a tooltip listing failed subjects and totals. Failure is always judged on the complete subject total, also when
  an examination filter is selected.
- Not verified on screen by me: 1366x768 / 1600x900 layouts, mouse-wheel and scrollbar dragging, tooltips, icons (Unicode symbols).

## Demo data update (data version 3, 2026-11-26)
- Blood groups (synthetic, not medical data) assigned to all 55 students: O+ 15, B+ 13, A+ 11, AB+ 4, O- 4, A- 3, B- 3, AB- 2
  (shuffled with a fixed seed). Existing values are kept; editable in the student form; shown in View Student and the student profile.
- The six failing demo students closest to passing (smallest shortfall) now pass with varied totals 40.5-46.5 (Final exam raised first);
  result: 43 pass / 12 fail = 78.18 % (dashboard value is calculated, nothing hard-coded). Topper (Shreya Iyer) and all other marks, attendance,
  names, rolls, subjects, grading and credits unchanged.
- Runs once: data_version 3 in settings.csv. Backup folder `data_backup_v2_<time>` + `migration_report.txt` entry. If the marks were edited by
  the teacher (differ from the generated demo marks) they are NOT touched; only blood groups are added.
- Fresh installs get the same final data directly. 974 automated checks pass.
- Not verified on screen by me: Dashboard 78.2 %, View Student blood group card, startup update message.

## Attendance period locking (2026-11-26, latest)
- A session is identified by DATE + PERIOD; its subject is stored with the records. Rule enforced in `attendance.c`
  (`att_period_subject`, `att_set` refuses a different subject, `att_save_session` validates everything and saves nothing on error).
- Take Attendance: changing date or period loads the saved session automatically (subject selected + locked, lock icon, message
  "Existing attendance loaded. Subject locked for this period."); an unused period unlocks the subject ("New session. Select a subject and
  record attendance."). Unsaved changes -> dialog Save and continue / Discard changes / Cancel. Re-saving edits the session (no duplicates,
  no extra class). A wrong subject is fixed with History > Delete session (strong warning) which unlocks the period.
- Existing data checked: no conflicting date/period sessions (each period number belongs to one subject). Conflicts, if ever present,
  are reported (start-up dialog + banner on the page) and never changed. Data files are unchanged (no format change).
- 998 automated checks pass. Not verified on screen by me: the UI flow, lock icon, confirmation dialog.

## ClassMetrics 2.0 — Premium UI, creative design and motion upgrade (2026-11-26)
This folder (`ClassMetrics_V2`) is an independent copy made from V1 (`student_management_project`) before the redesign.
V1 was not touched (no V1 file has a modification time after V2 was created). Own exe (`ClassMetrics_V2.exe`),
own `run.bat`, own `data/` (copy of V1 data at the time of duplication). Business logic, CSV formats and calculations are unchanged.

* Design system (`ui_theme.c`): palette #6842F5 / #14213D / #EDE9FE / #F3F4F9 / #17213D / #64748B / #16A34A / #DC2626 / #F59E0B / #E2E8F0;
  Inter -> Segoe UI -> Sans font stack (Inter is not bundled, so Segoe UI is what is shown on a normal Windows PC); rounded cards, soft shadows.
* Icons (`ui_icons.c`): one Cairo vector icon set (no emoji / font glyphs), with small event-triggered animations.
* Motion (150-500 ms, only on events): page cross-fade, count-up numbers, chart entrance, hover lift, save toast, lock/check icon animation.
  Switch: Profile > Preferences > "Interface animations" (stored as `animations` in `data/settings.csv`). Off = final state at once.
* Pages: login, sidebar + faculty avatar, dashboard (8 stat cards, ring, rounded charts, top performers), students (avatars, status),
  subjects (cards), marks, View Student (3x3 cards, vertical scroll only), attendance (live counters, lock icon, History summary cards/filters/
  View-Edit-Delete session/CSV export, Summary tab with progress bars), Math Analyzer, Reports, Faculty Profile.
* Verified: builds (warnings only), `make test` = 998 checks, 0 failed; screenshots captured at 1366x768, 1600x900, 1920x1080 by the app's own
  screenshot mode (env vars `CLASSMETRICS_SHOT_DIR`, `CLASSMETRICS_SHOT_VIEW`, `CLASSMETRICS_SIZE`, `CLASSMETRICS_PAGE`, `CLASSMETRICS_TAB`;
  `CLASSMETRICS_NOANIM=1` tests the reduced-motion path) and inspected by eye; no horizontal overflow seen at 1366x768.
* NOT verified by hand: mouse/keyboard interaction (hover, click animations, dialogs' look, real-time smoothness) — no input simulation was used.
* Limitations: Inter font not bundled; some dialogs (edit/add forms, file chooser) use mostly default GTK styling; animations are timer based
  (about 60 fps on the frame clock) and not measured on a low-power PC.

## CIE / SEE names and new roll numbers (data version 4, 2026-11-26)
* Examinations renamed everywhere (screens, reports, CSV export/import, data): Unit Test 1 -> **CIE 1**, Unit Test 2 -> **CIE 2**, Final Examination -> **SEE** (maxima 20 / 20 / 60 unchanged).
* Roll numbers: 0262001 ... 0262055 -> **990000000056 ... 990000000110** in students, marks and attendance (one-time migration to data version 4,
  old files copied to `data_backup_v3_<time>`; migration_report.txt updated). New demo students use the same format. Other roll formats are left alone.
* Marks spreadsheet export columns now read "<Subject name> (CIE 1)" etc.; sheets exported earlier with (UT1)/(UT2)/(Final) are still accepted.
* Only V2 was changed; V1 keeps Unit Test names and old roll numbers.

## Mathematical Analyzer redesign (2026-11-26, V2 only - awaiting review)
* Old implementation backed up in `backup_math_analyzer_before_redesign_20261126/` (ui_math.c, math_analyzer.c, common.h, tests, Makefile).
* New home page: six cards (3 x 2): Marks Comparison, Performance Trends, Attendance Predictor, Combined Performance, Academic Calculations,
  Class Performance. Each section has "Back to Analyzer", student / subject / examination selectors and a collapsed
  "Mathematics Behind This Analysis" panel (concept, definition, usefulness, formula, step-by-step with real data, result, where used).
  No syllabus unit numbers appear in the interface.
* New tested logic in `math_analyzer.c`: `mat_sub`, `att_projection` f(x) = 100 (A + x) / (T + x), `att_slope` f'(x), `att_classes_needed`
  (handles no records, already achieved, impossible 100 % target). `make test`: 1022 checks, 0 failed (14 new).
* The combined score uses the fixed P(x, y) = 0.7 x + 0.3 y (the old weight spin boxes were removed); it is shown as a project-defined score and never
  changes grades, SGPA or pass / fail. Stored data is never modified by the analyzer.
* Screenshot aids (screenshot mode only): CLASSMETRICS_TAB = 0 (home) ... 6, and CLASSMETRICS_MATH_STATE="astu=10;target=80;scroll=600;open=3;dialog=0".
