/*
 * reports.c : builds the reports as simple tables (header + rows) and
 * exports any report to a CSV file.
 *
 *   Individual student       Class performance       Subject-wise marks
 *   Examination-wise marks   Attendance              Low-attendance students
 *   Grade distribution       SGPA ranking            Mathematical analysis
 *
 * All records are DEMONSTRATION data, not official college records.
 */
#include "common.h"

static int report_teacher = 0;

void report_set_teacher(int idx) { report_teacher = idx; }

static void start(Report *r, const char *title, int ncols, const char **heads)
{
    int i;
    memset(r, 0, sizeof *r);
    strncpy(r->title, title, sizeof r->title - 1);
    r->ncols = ncols > REP_COLS ? REP_COLS : ncols;
    for (i = 0; i < r->ncols; i++) strncpy(r->head[i], heads[i], sizeof r->head[i] - 1);
}

/* Starts a new row; returns its index or -1 when the report is full. */
static int new_row(Report *r, int flag)
{
    if (r->nrows >= REP_ROWS) return -1;
    r->flag[r->nrows] = flag;
    return r->nrows++;
}

static void put(Report *r, int row, int col, const char *fmt, ...)
{
    va_list ap;
    if (row < 0 || col >= REP_COLS) return;
    va_start(ap, fmt);
    vsnprintf(r->cell[row][col], sizeof r->cell[row][col], fmt, ap);
    va_end(ap);
}

/* Short examination name for narrow column headings ("CIE 1", "CIE 2", "SEE"). */
static void exam_short(int e, char *out, size_t cap)
{
    if (e < 0 || e >= n_exams) { snprintf(out, cap, "-"); return; }
    else snprintf(out, cap, "%.10s", exams[e].name);
}

/* Heading text such as "CIE 1 (20)" into a header array. */
static void exam_head(int e, char *out, size_t cap)
{
    char s[24];
    exam_short(e, s, sizeof s);
    if (e >= 0 && e < n_exams) snprintf(out, cap, "%s (%.0f)", s, exams[e].max_marks);
    else snprintf(out, cap, "%s", s);
}

/* Cell text for the marks of one exam, "-" when missing. */
static void mark_cell(Report *r, int row, int col, const char *roll, int subj, int exam)
{
    int m = (exam < n_exams) ? marks_find(roll, subjects[subj].code, exam) : -1;
    if (m >= 0) put(r, row, col, "%.1f", marks[m].marks); else put(r, row, col, "-");
}

static void prepared_by(Report *r)
{
    Teacher *t;
    if (report_teacher < 0 || report_teacher >= n_teachers) return;
    t = &teachers[report_teacher];
    snprintf(r->prepared, sizeof r->prepared, "Prepared by %s (%s) - %s (%s) - %s", t->name, t->email,
             t->subject, t->subject_code, t->institution);
}

/* ---------- 1. individual student ---------- */
void report_student(Report *r, const char *roll)
{
    char h[12][32], hdr[96];
    const char *heads[12];
    int k = student_find(roll), j, row, i, c, a, earned = 0, cred = 0;
    double tot, mx, pp, sgpa;
    strcpy(h[0], "Subject"); strcpy(h[1], "Code"); strcpy(h[2], "Faculty");
    for (i = 0; i < 3; i++) exam_head(i, h[3 + i], sizeof h[0]);
    strcpy(h[6], "Total (100)"); strcpy(h[7], "Grade"); strcpy(h[8], "Grade pts"); strcpy(h[9], "Credits");
    strcpy(h[10], "Status"); strcpy(h[11], "Attendance %");
    for (i = 0; i < 12; i++) heads[i] = h[i];
    if (k < 0) { start(r, "Individual Student Report - no student selected", 12, heads); prepared_by(r); return; }
    pp = student_exam_pct(roll, EXAM_ALL, &tot, &mx);
    sgpa = student_sgpa(roll, &earned, &cred);
    snprintf(hdr, sizeof hdr, "Individual Student Report - %s (%s)", students[k].name, roll);
    start(r, hdr, 12, heads);
    prepared_by(r);
    for (j = 0; j < n_subjects; j++) {
        double st, sm, sp = subject_pct(roll, j);
        int pass = subject_pass(roll, j);
        subject_total(roll, j, &st, &sm);
        att_stats(roll, subjects[j].code, &c, &a);
        row = new_row(r, (sp >= 0 && !pass) || att_subject_low(roll, subjects[j].code));
        put(r, row, 0, "%s", subjects[j].name);
        put(r, row, 1, "%s", subjects[j].code);
        put(r, row, 2, "%s", subjects[j].faculty);
        for (i = 0; i < 3; i++) mark_cell(r, row, 3 + i, roll, j, i);
        if (sp >= 0) {
            put(r, row, 6, "%.1f", st);
            put(r, row, 7, "%s", grade_of(sp));
            put(r, row, 8, "%.0f", pass ? grade_points_of(sp) : 0.0);
            put(r, row, 10, "%s", pass ? "Pass" : "Fail");
        } else { put(r, row, 6, "-"); put(r, row, 7, "-"); put(r, row, 8, "-"); put(r, row, 10, "-"); }
        put(r, row, 9, "%d", subjects[j].credits);
        if (c > 0) put(r, row, 11, "%.1f", a * 100.0 / c); else put(r, row, 11, "-");
    }
    att_stats(roll, NULL, &c, &a);
    row = new_row(r, !student_exam_pass(roll, EXAM_ALL) || att_is_low(roll));
    put(r, row, 0, "OVERALL (total / SGPA)");
    put(r, row, 1, "-"); put(r, row, 2, "-");
    put(r, row, 3, "-"); put(r, row, 4, "-"); put(r, row, 5, "-");
    if (pp >= 0) {
        put(r, row, 6, "%.1f / %.0f", tot, mx);
        put(r, row, 7, "%s", grade_of(pp));
    } else { put(r, row, 6, "-"); put(r, row, 7, "-"); }
    if (sgpa >= 0) put(r, row, 8, "SGPA %.2f", sgpa); else put(r, row, 8, "-");
    put(r, row, 9, "%d / %d", earned, total_credits());
    put(r, row, 10, "%s", student_exam_pass(roll, EXAM_ALL) ? "Pass" : "Fail");
    if (c > 0) put(r, row, 11, "%.1f", a * 100.0 / c); else put(r, row, 11, "-");
}

/* ---------- 2. class performance ---------- */
void report_class(Report *r, int exam)
{
    static const char *h[] = { "Rank", "Roll No", "Name", "Total", "Max", "Percent", "Grade", "SGPA", "Credits earned", "Result", "Attendance %" };
    int order[MAX_STUDENTS], n, i, row, rank = 0, earned, cred;
    double pct[MAX_STUDENTS], tot, mx, ap, g;
    char t[128];
    snprintf(t, sizeof t, "Class Performance Report - %s", (exam >= 0 && exam < n_exams) ? exams[exam].name : "all examinations");
    start(r, t, 11, h);
    prepared_by(r);
    n = exam_ranking(exam, order, pct);
    for (i = 0; i < n; i++) {
        int s = order[i];
        if (i == 0 || pct[i] < pct[i - 1]) rank = i + 1;
        student_exam_pct(students[s].roll, exam, &tot, &mx);
        ap = att_pct(students[s].roll, NULL);
        row = new_row(r, !student_exam_pass(students[s].roll, exam));
        put(r, row, 0, "%d", rank);
        put(r, row, 1, "%s", students[s].roll);
        put(r, row, 2, "%s", students[s].name);
        put(r, row, 3, "%.1f", tot);
        put(r, row, 4, "%.0f", mx);
        put(r, row, 5, "%.2f", pct[i]);
        put(r, row, 6, "%s", grade_of(pct[i]));
        g = (exam < 0) ? student_sgpa(students[s].roll, &earned, &cred) : -1;
        if (g >= 0) { put(r, row, 7, "%.2f", g); put(r, row, 8, "%d", earned); } else { put(r, row, 7, "-"); put(r, row, 8, "-"); }
        put(r, row, 9, "%s", student_exam_pass(students[s].roll, exam) ? "Pass" : "Fail");
        if (ap >= 0) put(r, row, 10, "%.1f", ap); else put(r, row, 10, "-");
    }
}

/* ---------- 3. subject-wise marks ---------- */
void report_subject(Report *r, int subj, int exam)
{
    char h[12][32], t[200];
    const char *heads[12];
    int i, row, j;
    strcpy(h[0], "Roll No"); strcpy(h[1], "Name"); strcpy(h[2], "Code"); strcpy(h[3], "Faculty");
    for (i = 0; i < 3; i++) exam_head(i, h[4 + i], sizeof h[0]);
    strcpy(h[7], "Total (100)"); strcpy(h[8], "Percent"); strcpy(h[9], "Grade"); strcpy(h[10], "Grade pts"); strcpy(h[11], "Status");
    for (i = 0; i < 12; i++) heads[i] = h[i];
    if (subj < 0 || subj >= n_subjects) { start(r, "Subject-wise Marks Report - select a subject", 12, heads); prepared_by(r); return; }
    snprintf(t, sizeof t, "Subject-wise Marks Report - %s (%s), faculty %s%s%s", subjects[subj].name, subjects[subj].code,
             subjects[subj].faculty, exam >= 0 && exam < n_exams ? ", marks entered in " : "",
             exam >= 0 && exam < n_exams ? exams[exam].name : "");
    start(r, t, 12, heads);
    prepared_by(r);
    for (j = 0; j < n_students; j++) {
        double st, sm, sp = subject_pct(students[j].roll, subj);
        int pass;
        if (sp < 0) continue;
        if (exam >= 0 && marks_find(students[j].roll, subjects[subj].code, exam) < 0) continue;
        pass = subject_pass(students[j].roll, subj);
        subject_total(students[j].roll, subj, &st, &sm);
        row = new_row(r, !pass);
        put(r, row, 0, "%s", students[j].roll);
        put(r, row, 1, "%s", students[j].name);
        put(r, row, 2, "%s", subjects[subj].code);
        put(r, row, 3, "%s", subjects[subj].faculty);
        for (i = 0; i < 3; i++) mark_cell(r, row, 4 + i, students[j].roll, subj, i);
        put(r, row, 7, "%.1f", st);
        put(r, row, 8, "%.2f", sp);
        put(r, row, 9, "%s", grade_of(sp));
        put(r, row, 10, "%.0f", pass ? grade_points_of(sp) : 0.0);
        put(r, row, 11, "%s", pass ? "Pass" : "Fail");
    }
}

/* ---------- 4. examination-wise marks (one column per subject) ---------- */
void report_exam(Report *r, int exam)
{
    char h[REP_COLS][32], t[128];
    const char *heads[REP_COLS];
    int nc = 0, i, j, row, ns = n_subjects > REP_COLS - 4 ? REP_COLS - 4 : n_subjects;
    double tot, mx, p;
    strcpy(h[nc++], "Roll No"); strcpy(h[nc++], "Name");
    for (j = 0; j < ns; j++) snprintf(h[nc++], sizeof h[0], "%s", subjects[j].short_name[0] ? subjects[j].short_name : subjects[j].code);
    strcpy(h[nc++], "Total"); strcpy(h[nc++], "Percent");
    for (i = 0; i < nc; i++) heads[i] = h[i];
    snprintf(t, sizeof t, "Examination-wise Marks Report - %s", (exam >= 0 && exam < n_exams) ? exams[exam].name : "Totals of all examinations");
    start(r, t, nc, heads);
    prepared_by(r);
    for (i = 0; i < n_students; i++) {
        p = student_exam_pct(students[i].roll, exam, &tot, &mx);
        if (p < 0) continue;
        row = new_row(r, !student_exam_pass(students[i].roll, exam));
        put(r, row, 0, "%s", students[i].roll);
        put(r, row, 1, "%s", students[i].name);
        for (j = 0; j < ns; j++) {
            if (exam >= 0) mark_cell(r, row, 2 + j, students[i].roll, j, exam);
            else {
                double st;
                if (subject_total(students[i].roll, j, &st, NULL) > 0) put(r, row, 2 + j, "%.1f", st); else put(r, row, 2 + j, "-");
            }
        }
        put(r, row, 2 + ns, "%.1f / %.0f", tot, mx);
        put(r, row, 3 + ns, "%.2f", p);
    }
}

/* ---------- 5 and 6. attendance (subject-wise columns) ---------- */
static void attendance_rows(Report *r, int only_low)
{
    int i, j, row, c, a, low_subj;
    double p;
    for (i = 0; i < n_students; i++) {
        char why[96] = "";
        att_stats(students[i].roll, NULL, &c, &a);
        if (c == 0) continue;
        p = a * 100.0 / c;
        low_subj = 0;
        for (j = 0; j < n_subjects; j++)
            if (att_subject_low(students[i].roll, subjects[j].code)) {
                low_subj++;
                sb_add(why, sizeof why, "%s%s", why[0] ? ", " : "", subjects[j].short_name[0] ? subjects[j].short_name : subjects[j].code);
            }
        if (only_low && p >= settings.att_threshold && low_subj == 0) continue;
        row = new_row(r, p < settings.att_threshold || low_subj > 0);
        put(r, row, 0, "%s", students[i].roll);
        put(r, row, 1, "%s", students[i].name);
        for (j = 0; j < n_subjects && j < REP_COLS - 4; j++) {
            double sp = att_pct(students[i].roll, subjects[j].code);
            if (sp >= 0) put(r, row, 2 + j, "%.1f", sp); else put(r, row, 2 + j, "-");
        }
        j = n_subjects > REP_COLS - 4 ? REP_COLS - 4 : n_subjects;
        put(r, row, 2 + j, "%.1f", p);
        if (p < settings.att_threshold) put(r, row, 3 + j, "Low overall");
        else if (low_subj > 0) put(r, row, 3 + j, "Low in: %s", why);
        else put(r, row, 3 + j, "%s", att_category_name(att_category(p)));
    }
}

static void attendance_start(Report *r, const char *title)
{
    char h[REP_COLS][32];
    const char *heads[REP_COLS];
    int nc = 0, j, i, ns = n_subjects > REP_COLS - 4 ? REP_COLS - 4 : n_subjects;
    strcpy(h[nc++], "Roll No"); strcpy(h[nc++], "Name");
    for (j = 0; j < ns; j++) snprintf(h[nc++], sizeof h[0], "%s %%", subjects[j].short_name[0] ? subjects[j].short_name : subjects[j].code);
    strcpy(h[nc++], "Overall %"); strcpy(h[nc++], "Status");
    for (i = 0; i < nc; i++) heads[i] = h[i];
    start(r, title, nc, heads);
    prepared_by(r);
}

void report_attendance(Report *r)
{
    char t[96];
    snprintf(t, sizeof t, "Attendance Report (warning below %.0f%%)", settings.att_threshold);
    attendance_start(r, t);
    attendance_rows(r, 0);
}

void report_low_attendance(Report *r)
{
    char t[120];
    snprintf(t, sizeof t, "Low-Attendance Students (overall or any subject below %.0f%%)", settings.att_threshold);
    attendance_start(r, t);
    attendance_rows(r, 1);
}

/* ---------- 7. grade distribution per subject ---------- */
void report_grades(Report *r)
{
    static const char *h[] = { "Subject", "Code", "Faculty", "S", "A", "B", "C", "D", "E", "F", "Pass %" };
    static const char *gl[7] = { "S", "A", "B", "C", "D", "E", "F" };
    int j, i, k, row, cnt[7], n, passed;
    double p;
    start(r, "Grade Distribution Report (subject totals)", 11, h);
    prepared_by(r);
    for (j = 0; j < n_subjects; j++) {
        memset(cnt, 0, sizeof cnt);
        n = passed = 0;
        for (i = 0; i < n_students; i++) {
            p = subject_pct(students[i].roll, j);
            if (p < 0) continue;
            n++;
            if (subject_pass(students[i].roll, j)) passed++;
            for (k = 0; k < 7; k++) if (strcmp(grade_of(p), gl[k]) == 0) cnt[k]++;
        }
        row = new_row(r, n > 0 && passed * 100.0 / n < 75);
        put(r, row, 0, "%s", subjects[j].name);
        put(r, row, 1, "%s", subjects[j].code);
        put(r, row, 2, "%s", subjects[j].faculty);
        for (k = 0; k < 7; k++) put(r, row, 3 + k, "%d", cnt[k]);
        if (n > 0) put(r, row, 10, "%.1f", passed * 100.0 / n); else put(r, row, 10, "-");
    }
}

/* ---------- 8. SGPA ranking ---------- */
void report_sgpa(Report *r)
{
    static const char *h[] = { "Rank", "Roll No", "Name", "SGPA", "Credits earned", "Credits (total)", "Overall %", "Result" };
    int idx[MAX_STUDENTS], n = 0, i, j, best, t, row, rank = 0, earned, cred;
    double sg[MAX_STUDENTS], d;
    char title[96];
    snprintf(title, sizeof title, "SGPA Report (credit-weighted, %d credits; zero-credit subjects excluded)", total_credits());
    start(r, title, 8, h);
    prepared_by(r);
    for (i = 0; i < n_students; i++) {
        d = student_sgpa(students[i].roll, NULL, NULL);
        if (d >= 0) { idx[n] = i; sg[n] = d; n++; }
    }
    for (i = 0; i < n - 1; i++) {                       /* selection sort, highest SGPA first */
        best = i;
        for (j = i + 1; j < n; j++) if (sg[j] > sg[best]) best = j;
        if (best != i) { d = sg[i]; sg[i] = sg[best]; sg[best] = d; t = idx[i]; idx[i] = idx[best]; idx[best] = t; }
    }
    for (i = 0; i < n; i++) {
        int s = idx[i];
        if (i == 0 || sg[i] < sg[i - 1]) rank = i + 1;
        student_sgpa(students[s].roll, &earned, &cred);
        row = new_row(r, !student_exam_pass(students[s].roll, EXAM_ALL));
        put(r, row, 0, "%d", rank);
        put(r, row, 1, "%s", students[s].roll);
        put(r, row, 2, "%s", students[s].name);
        put(r, row, 3, "%.2f", sg[i]);
        put(r, row, 4, "%d", earned);
        put(r, row, 5, "%d", cred);
        put(r, row, 6, "%.2f", student_exam_pct(students[s].roll, EXAM_ALL, NULL, NULL));
        put(r, row, 7, "%s", student_exam_pass(students[s].roll, EXAM_ALL) ? "Pass" : "Backlog");
    }
}

/* ---------- 9. mathematical analysis ---------- */
void report_math(Report *r, int exam)
{
    static const char *h[] = { "Roll No", "Name", "Marks % (x)", "Attendance % (y)", "P(x,y)",
                               "Rate (%/exam)", "Trend", "Rank" };
    int i, row, n, ex[MAX_EXAMS];
    double x, y, pc[MAX_EXAMS];
    char t[140];
    snprintf(t, sizeof t, "Mathematical Analysis Report - P = %.2fx + %.2fy, marks: %s", settings.weight_x / 100,
             settings.weight_y / 100, (exam >= 0 && exam < n_exams) ? exams[exam].name : "overall");
    start(r, t, 8, h);
    prepared_by(r);
    for (i = 0; i < n_students; i++) {
        x = student_exam_pct(students[i].roll, exam, NULL, NULL);
        y = att_pct(students[i].roll, NULL);
        if (x < 0) continue;
        row = new_row(r, att_is_low(students[i].roll));
        put(r, row, 0, "%s", students[i].roll);
        put(r, row, 1, "%s", students[i].name);
        put(r, row, 2, "%.2f", x);
        if (y >= 0) { put(r, row, 3, "%.2f", y); put(r, row, 4, "%.2f", perf_score(x, y, settings.weight_x, settings.weight_y)); }
        else { put(r, row, 3, "-"); put(r, row, 4, "-"); }
        n = student_trend(students[i].roll, pc, ex);
        if (n >= 2) {
            double rate = (pc[n - 1] - pc[n - 2]) / (double)(ex[n - 1] - ex[n - 2]);
            put(r, row, 5, "%+.2f", rate);
            put(r, row, 6, "%s", trend_label(rate));
        } else { put(r, row, 5, "-"); put(r, row, 6, "-"); }
        put(r, row, 7, "%d", student_rank(students[i].roll, exam));
    }
}

/* ---------- CSV export ---------- */
int report_export_csv(const Report *r, const char *path)
{
    FILE *fp = fopen(path, "w");
    int i, j;
    if (fp == NULL) return 1;
    fprintf(fp, "\"%s\"\n", r->title);
    if (r->prepared[0]) fprintf(fp, "\"%s\"\n", r->prepared);
    fprintf(fp, "\"Demonstration data - not official college records\"\n");
    for (j = 0; j < r->ncols; j++) fprintf(fp, "%s%s", j ? "," : "", r->head[j]);
    fprintf(fp, "\n");
    for (i = 0; i < r->nrows; i++) {
        for (j = 0; j < r->ncols; j++) {
            const char *c = r->cell[i][j];
            if (strchr(c, ',') != NULL) fprintf(fp, "%s\"%s\"", j ? "," : "", c);
            else fprintf(fp, "%s%s", j ? "," : "", c);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
    return 0;
}
