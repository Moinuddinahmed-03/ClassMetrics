/*
 * marks.c : examinations, marks entry with validation, subject totals, grades,
 *           grade points, SGPA, class statistics and ranking.
 *
 * One MarkRec = one student, one subject, one examination (CIE 1 /20,
 * CIE 2 /20 or SEE /60).  Subject total = sum of the three.
 * Passing marks and the grading scale are DEMONSTRATION values (settings.csv / subjects.csv).
 */
#include "common.h"
#include <strings.h>

int exam_add(const char *name_in, double max, char *err)
{
    char name[32];
    int i;
    strncpy(name, name_in, sizeof name - 1); name[sizeof name - 1] = '\0';
    clean_text(name);
    if (name[0] == '\0') { strcpy(err, "Examination name cannot be empty."); return -1; }
    if (max <= 0) { strcpy(err, "Maximum marks must be greater than 0."); return -1; }
    if (n_exams >= MAX_EXAMS) { strcpy(err, "Examination limit reached."); return -1; }
    for (i = 0; i < n_exams; i++)
        if (strcmp(exams[i].name, name) == 0) { strcpy(err, "This examination already exists."); return -1; }
    strcpy(exams[n_exams].name, name);
    exams[n_exams].max_marks = max;
    return n_exams++;
}

int marks_find(const char *roll, const char *code, int exam)
{
    int i;
    for (i = 0; i < n_marks; i++)
        if (marks[i].exam == exam && strcmp(marks[i].roll, roll) == 0 && strcmp(marks[i].subject, code) == 0)
            return i;
    return -1;
}

/* Adds or edits one mark.  Marks cannot be negative or above the maximum of that examination. */
int marks_set(const char *roll, const char *code, int exam, double value, char *err)
{
    int si = subject_find(code), idx;
    if (student_find(roll) < 0) { strcpy(err, "Select a student."); return 0; }
    if (si < 0) { strcpy(err, "Select a subject."); return 0; }
    if (exam < 0 || exam >= n_exams) { strcpy(err, "Select an examination."); return 0; }
    if (value < 0) { strcpy(err, "Marks cannot be negative."); return 0; }
    if (value > exams[exam].max_marks) {
        sprintf(err, "%s marks cannot exceed the maximum (%.0f).", exams[exam].name, exams[exam].max_marks);
        return 0;
    }
    idx = marks_find(roll, code, exam);
    if (idx < 0) {
        if (n_marks >= MAX_MARKS) { strcpy(err, "Marks storage is full."); return 0; }
        idx = n_marks++;
        strcpy(marks[idx].roll, roll);
        strcpy(marks[idx].subject, code);
        marks[idx].exam = exam;
    }
    marks[idx].marks = value;
    return 1;
}

void marks_remove(int idx)
{
    int i;
    if (idx < 0 || idx >= n_marks) return;
    for (i = idx; i < n_marks - 1; i++) marks[i] = marks[i + 1];
    n_marks--;
}

/*
 * Roll number as typed in a spreadsheet -> the real roll number.  Excel drops leading zeros
 * (0262001 becomes 262001) and may change letter case of subject codes, so both are tolerated.
 * Returns the matching student index or -1.
 */
static int resolve_roll(const char *roll)
{
    int i;
    const char *a;
    if ((i = student_find(roll)) >= 0) return i;
    for (a = roll; *a == '0'; a++) ;
    if (*a == '\0') return -1;
    for (i = 0; i < n_students; i++) {
        const char *b = students[i].roll;
        while (*b == '0') b++;
        if (strcmp(a, b) == 0) return i;
    }
    return -1;
}

static int resolve_subject(const char *code)
{
    int j;
    for (j = 0; j < n_subjects; j++)
        if (strcasecmp(subjects[j].code, code) == 0) return j;
    return -1;
}

static const char *EXAM_TAG[3] = { "CIE 1", "CIE 2", "SEE" };
static const char *EXAM_OLD_TAG[3] = { "UT1", "UT2", "Final" };      /* sheets exported by the previous version */

/*
 * Exports the marks as a spreadsheet: one row per student (Roll No, Student Name) and one column per
 * subject, e.g. "Mathematics-I (CIE 1)".  choice 0 = CIE 1, 1 = CIE 2, 2 = SEE, 3 = all three side by side.
 * Marks not entered yet stay blank.  marks_import_csv reads this layout back.
 * Returns 0, or 1 if the file cannot be written.
 */
int marks_export_csv(const char *path, int choice, int *rows)
{
    FILE *fp = fopen(path, "w");
    int i, j, e, k, e0 = choice == 3 ? 0 : choice, e1 = choice == 3 ? 2 : choice;
    *rows = 0;
    if (fp == NULL) return 1;
    fputs("\xEF\xBB\xBF", fp);                                   /* UTF-8 marker so Excel reads it correctly */
    fputs("Roll No,Student Name", fp);
    for (e = e0; e <= e1; e++)
        for (j = 0; j < n_subjects; j++) fprintf(fp, ",%s (%s)", subjects[j].name, EXAM_TAG[e]);
    fputc('\n', fp);
    for (i = 0; i < n_students; i++) {
        fprintf(fp, "%s,%s", students[i].roll, students[i].name);
        for (e = e0; e <= e1; e++)
            for (j = 0; j < n_subjects; j++) {
                k = (e < n_exams) ? marks_find(students[i].roll, subjects[j].code, e) : -1;
                if (k >= 0) fprintf(fp, ",%g", marks[k].marks); else fputc(',', fp);
            }
        fputc('\n', fp);
        (*rows)++;
    }
    fclose(fp);
    return 0;
}

/* Spreadsheet column title "Subject name (CIE 1)" -> subject index and exam index; 0 if not recognised. */
static int header_col(char *h, int *subj, int *exam)
{
    char *q = strrchr(h, '('), name[80], tag[16];
    int e, j, n;
    if (q == NULL) return 0;
    n = (int)(q - h);
    while (n > 0 && h[n - 1] == ' ') n--;
    if (n <= 0 || n >= (int)sizeof name) return 0;
    memcpy(name, h, (size_t)n); name[n] = '\0';
    snprintf(tag, sizeof tag, "%s", q + 1);
    tag[strcspn(tag, ")")] = 0;
    { int tl = (int)strlen(tag); while (tl > 0 && tag[tl - 1] == 32) tag[--tl] = 0; }
    for (e = 0; e < 3 && strcasecmp(tag, EXAM_TAG[e]) != 0 && strcasecmp(tag, EXAM_OLD_TAG[e]) != 0; e++) ;
    if (e == 3) return 0;
    for (j = 0; j < n_subjects; j++)
        if (strcasecmp(name, subjects[j].name) == 0 || strcasecmp(name, subjects[j].code) == 0 ||
            (subjects[j].short_name[0] && strcasecmp(name, subjects[j].short_name) == 0)) {
            *subj = j; *exam = e;
            return 1;
        }
    return 0;
}


/*
 * Imports marks from CSV:  roll, subject code, CIE 1, CIE 2, SEE  (a blank cell is skipped).
 * Every value is validated with marks_set().  Returns 0, or 1 if the file cannot be opened.
 */
int marks_import_csv(const char *path, int *added, int *skipped, char *first_problem)
{
    FILE *fp = fopen(path, "r");
    char line[2048], *f[48], err[160];
    int wide = 0, csub[48], cex[48];
    int lineno = 0, e, n, ok, bad;
    *added = *skipped = 0;
    first_problem[0] = '\0';
    if (fp == NULL) return 1;
    while (fgets(line, sizeof line, fp) != NULL) {
        char *p = line;
        lineno++;
        if (lineno == 1 && (unsigned char)p[0] == 0xEF) p += 3;
        p[strcspn(p, "\r\n")] = '\0';
        if (p[0] == '\0') continue;
        n = csv_split(p, f, 48);
        if (lineno == 1 && n >= 3 && strncasecmp(f[1], "student", 7) == 0) {       /* spreadsheet layout: one column per subject */
            int c, found = 0;
            wide = 1;
            for (c = 2; c < n; c++) {
                clean_text(f[c]);
                if (header_col(f[c], &csub[c], &cex[c])) found++; else csub[c] = -1;
            }
            if (!found) snprintf(first_problem, 160, "line 1: no subject column recognised (expected e.g. \"Mathematics-I (CIE 1)\")");
            continue;
        }
        if (wide) {
            int c, si_;
            clean_text(f[0]);
            si_ = n >= 3 ? resolve_roll(f[0]) : -1;
            ok = 0; bad = 0; err[0] = '\0';
            if (si_ < 0) { bad = 1; snprintf(err, sizeof err, "roll number '%s' not found", f[0]); }
            for (c = 2; !bad && c < n; c++) {
                double v;
                if (csub[c] < 0) continue;
                clean_text(f[c]);
                if (f[c][0] == '\0') continue;
                if (!parse_double(f[c], &v)) { bad = 1; snprintf(err, sizeof err, "'%s' is not a number", f[c]); break; }
                if (marks_set(students[si_].roll, subjects[csub[c]].code, cex[c], v, err)) ok++; else bad = 1;
            }
            if (bad || ok == 0) {
                (*skipped)++;
                if (!first_problem[0]) snprintf(first_problem, 160, "line %d: %s", lineno, bad ? err : "no marks given");
            } else (*added)++;
            continue;
        }
        if (lineno == 1 && (strncasecmp(f[0], "roll", 4) == 0 || f[0][0] == '#')) continue;
        if (n < 3) { (*skipped)++; if (!first_problem[0]) snprintf(first_problem, 160, "line %d: too few columns", lineno); continue; }
        clean_text(f[0]); clean_text(f[1]);
        {
            int si_ = resolve_roll(f[0]), sj_ = resolve_subject(f[1]);
            if (si_ >= 0) f[0] = students[si_].roll;               /* leading zeros / letter case fixed */
            if (sj_ >= 0) f[1] = subjects[sj_].code;
        }
        ok = 0; bad = 0;
        for (e = 0; e < 3 && e + 2 < n; e++) {
            double v;
            clean_text(f[e + 2]);
            if (f[e + 2][0] == '\0') continue;
            if (!parse_double(f[e + 2], &v)) { bad = 1; snprintf(err, sizeof err, "'%s' is not a number", f[e + 2]); break; }
            if (marks_set(f[0], f[1], e, v, err)) ok++; else { bad = 1; break; }
        }
        if (bad || ok == 0) {
            (*skipped)++;
            if (!first_problem[0]) snprintf(first_problem, 160, "line %d: %s", lineno, bad ? err : "no marks given");
        } else (*added)++;
    }
    fclose(fp);
    return 0;
}

double pct_of(double value, double max) { return max > 0 ? value * 100.0 / max : 0.0; }

/* Maximum marks of one examination, or of the whole subject when exam is EXAM_ALL. */
double exam_subject_max(int exam, int subj)
{
    if (exam >= 0 && exam < n_exams) return exams[exam].max_marks;
    return (subj >= 0 && subj < n_subjects) ? subjects[subj].max_marks : 0.0;
}

/*
 * ONE grading system for the whole application (demonstration scale, configurable):
 *   S 90+ (10)  A 80+ (9)  B 70+ (8)  C 60+ (7)  D 50+ (6)  E 40+ (5)  F below 40 (0, Fail)
 * The minimum percentages come from settings.grade_min[0..5].  The grade of a subject
 * is always decided from the COMPLETE subject total (percentage of its maximum).
 */
static const char *GRADE_NAMES[7] = { "S", "A", "B", "C", "D", "E", "F" };
static const double GRADE_POINTS[7] = { 10, 9, 8, 7, 6, 5, 0 };
static const char *GRADE_TEXT[7] = { "Outstanding", "Excellent", "Very Good", "Good", "Average", "Pass", "Fail" };

const char *grade_name(int idx) { return (idx >= 0 && idx < 7) ? GRADE_NAMES[idx] : "-"; }

/* 0 = S ... 6 = F.  Decimal percentages are exact: 89.5 -> A, 90 -> S. */
int grade_index(double p)
{
    int k;
    for (k = 0; k < 6; k++)
        if (p + 1e-9 >= settings.grade_min[k]) return k;
    return 6;
}

const char *grade_of(double p) { return GRADE_NAMES[grade_index(p)]; }

double grade_points_of(double p) { return GRADE_POINTS[grade_index(p)]; }

const char *grade_performance(const char *g)
{
    int k;
    for (k = 0; k < 7; k++) if (strcmp(g, GRADE_NAMES[k]) == 0) return GRADE_TEXT[k];
    return "-";
}

/* Grade legend text from the current scale: "S 90-100 (10)   A 80-<90 (9) ..." */
void grade_legend(char *buf, size_t cap)
{
    int k;
    buf[0] = '\0';
    sb_add(buf, cap, "S %.0f-100 (10)", settings.grade_min[0]);
    for (k = 1; k < 6; k++)
        sb_add(buf, cap, "   %s %.0f-<%.0f (%.0f)", GRADE_NAMES[k], settings.grade_min[k], settings.grade_min[k - 1], GRADE_POINTS[k]);
    sb_add(buf, cap, "   F below %.0f (0, Fail)", settings.grade_min[5]);
}

/* Subject TOTAL passed? (value out of the subject maximum) */
int marks_passed(double value, const Subject *s) { return value + 1e-9 >= s->pass_marks; }

/* Same pass percentage applied to one examination (for colouring only). */
int comp_passed(double value, double max, const Subject *s)
{
    if (max <= 0 || s->max_marks <= 0) return 0;
    return value * 100.0 / max + 1e-9 >= s->pass_marks * 100.0 / s->max_marks;
}

/* Adds up the examinations found for one student and subject. Returns how many were found. */
int subject_total(const char *roll, int subj, double *total, double *maxv)
{
    int i, n = 0;
    double t = 0, m = 0;
    for (i = 0; i < n_marks; i++) {
        if (strcmp(marks[i].roll, roll) != 0 || strcmp(marks[i].subject, subjects[subj].code) != 0) continue;
        if (marks[i].exam < 0 || marks[i].exam >= n_exams) continue;
        t += marks[i].marks;
        m += exams[marks[i].exam].max_marks;
        n++;
    }
    if (total) *total = t;
    if (maxv) *maxv = m;
    return n;
}

/* Subject percentage = total / maximum of the examinations entered x 100. */
double subject_pct(const char *roll, int subj)
{
    double t, m;
    if (subj < 0 || subj >= n_subjects || subject_total(roll, subj, &t, &m) == 0 || m <= 0) return -1.0;
    return t * 100.0 / m;
}

int subject_pass(const char *roll, int subj)
{
    double p = subject_pct(roll, subj);
    return p >= 0 && p + 1e-9 >= subjects[subj].pass_marks * 100.0 / subjects[subj].max_marks;
}

/*
 * Percentage of one student = (sum of marks) / (sum of maximum marks) x 100.
 * exam >= 0 : only that examination (e.g. CIE 1, summed over the subjects)
 * exam = EXAM_ALL : every examination (overall percentage out of 600 when complete).
 * Returns -1 if nothing is recorded.
 */
double student_exam_pct(const char *roll, int exam, double *total, double *maxtot)
{
    int i;
    double t = 0, m = 0;
    for (i = 0; i < n_marks; i++) {
        if (strcmp(marks[i].roll, roll) != 0) continue;
        if (marks[i].exam < 0 || marks[i].exam >= n_exams) continue;
        if (exam >= 0 && marks[i].exam != exam) continue;
        if (subject_find(marks[i].subject) < 0) continue;
        t += marks[i].marks;
        m += exams[marks[i].exam].max_marks;
    }
    if (total) *total = t;
    if (maxtot) *maxtot = m;
    return m > 0 ? t * 100.0 / m : -1.0;
}

int student_exam_pass(const char *roll, int exam)
{
    int i, si, j;
    if (exam < 0) {                                  /* overall: every subject total must pass */
        for (j = 0; j < n_subjects; j++)
            if (subject_pct(roll, j) >= 0 && !subject_pass(roll, j)) return 0;
        return 1;
    }
    for (i = 0; i < n_marks; i++) {
        if (marks[i].exam != exam || strcmp(marks[i].roll, roll) != 0) continue;
        si = subject_find(marks[i].subject);
        if (si >= 0 && !comp_passed(marks[i].marks, exams[exam].max_marks, &subjects[si])) return 0;
    }
    return 1;
}

/*
 * SGPA = sum(credits x grade points) / sum(credits), over credit-bearing subjects.
 * Zero-credit subjects (Environmental Science) do not affect it.
 * A failed subject gives 0 grade points.  *earned = credits of passed subjects.
 */
double student_sgpa(const char *roll, int *earned, int *credits_counted)
{
    int j, cred = 0, ear = 0;
    double sum = 0, p;
    for (j = 0; j < n_subjects; j++) {
        if (subjects[j].credits <= 0) continue;
        p = subject_pct(roll, j);
        if (p < 0) continue;
        cred += subjects[j].credits;
        if (subject_pass(roll, j)) {
            sum += subjects[j].credits * grade_points_of(p);
            ear += subjects[j].credits;
        }
    }
    if (earned) *earned = ear;
    if (credits_counted) *credits_counted = cred;
    return cred > 0 ? sum / cred : -1.0;
}

int latest_exam(void)
{
    int e, i;
    for (e = n_exams - 1; e >= 0; e--)
        for (i = 0; i < n_marks; i++)
            if (marks[i].exam == e) return e;
    return -1;
}

/* Ranks students (highest percentage first) with selection sort. */
int exam_ranking(int exam, int *order, double *pct)
{
    int i, j, n = 0, best, t;
    double pv[MAX_STUDENTS], d;
    for (i = 0; i < n_students; i++) {
        d = student_exam_pct(students[i].roll, exam, NULL, NULL);
        if (d >= 0) { order[n] = i; pv[n] = d; n++; }
    }
    for (i = 0; i < n - 1; i++) {
        best = i;
        for (j = i + 1; j < n; j++)
            if (pv[j] > pv[best]) best = j;
        if (best != i) {
            d = pv[i]; pv[i] = pv[best]; pv[best] = d;
            t = order[i]; order[i] = order[best]; order[best] = t;
        }
    }
    for (i = 0; i < n; i++) pct[i] = pv[i];
    return n;
}

/* Standard competition ranking: students with equal % share a rank. */
int student_rank(const char *roll, int exam)
{
    int order[MAX_STUDENTS], n, i, rank = 0;
    double pct[MAX_STUDENTS];
    n = exam_ranking(exam, order, pct);
    for (i = 0; i < n; i++) {
        if (i == 0 || pct[i] < pct[i - 1]) rank = i + 1;
        if (strcmp(students[order[i]].roll, roll) == 0) return rank;
    }
    return 0;
}

/* Class average of one subject: one examination, or the subject totals (EXAM_ALL). */
double subject_avg_pct(int subj, int exam)
{
    int i, n = 0, k;
    double sum = 0, p;
    if (subj < 0 || subj >= n_subjects) return -1;
    for (i = 0; i < n_students; i++) {
        if (exam < 0) {
            p = subject_pct(students[i].roll, subj);
            if (p >= 0) { sum += p; n++; }
        } else if ((k = marks_find(students[i].roll, subjects[subj].code, exam)) >= 0) {
            sum += pct_of(marks[k].marks, exams[exam].max_marks);
            n++;
        }
    }
    return n ? sum / n : -1;
}

double class_avg_pct(int exam)
{
    int i, n = 0;
    double sum = 0, p;
    for (i = 0; i < n_students; i++) {
        p = student_exam_pct(students[i].roll, exam, NULL, NULL);
        if (p >= 0) { sum += p; n++; }
    }
    return n ? sum / n : -1;
}

double class_pass_pct(int exam)
{
    int i, n = 0, passed = 0;
    for (i = 0; i < n_students; i++) {
        if (student_exam_pct(students[i].roll, exam, NULL, NULL) < 0) continue;
        n++;
        if (student_exam_pass(students[i].roll, exam)) passed++;
    }
    return n ? passed * 100.0 / n : -1;
}

double class_avg_sgpa(void)
{
    int i, n = 0;
    double sum = 0, g;
    for (i = 0; i < n_students; i++) {
        g = student_sgpa(students[i].roll, NULL, NULL);
        if (g >= 0) { sum += g; n++; }
    }
    return n ? sum / n : -1;
}

/* Percentage in every exam that has data; returns how many (for trend/derivative). */
int student_trend(const char *roll, double pct[MAX_EXAMS], int ex[MAX_EXAMS])
{
    int e, n = 0;
    double p;
    for (e = 0; e < n_exams; e++) {
        p = student_exam_pct(roll, e, NULL, NULL);
        if (p >= 0) { pct[n] = p; ex[n] = e; n++; }
    }
    return n;
}

/* ================= consolidated marks sheet (one row per student) ================= */

/*
 * Fills one row of the student-wise marks sheet.
 * exam = EXAM_ALL : every subject cell is the subject total (out of 100), maximum 600.
 * exam >= 0       : every cell is the mark of that examination, maximum = subjects x exam maximum.
 * SGPA and the pass result are ALWAYS the overall result (from complete subject totals).
 */
void marks_sheet_row(const char *roll, int exam, SheetRow *r)
{
    int j, k;
    double t;
    memset(r, 0, sizeof *r);
    for (j = 0; j < n_subjects; j++) {
        if (exam < 0) {
            r->has[j] = subject_total(roll, j, &t, NULL) > 0;
            r->cell[j] = r->has[j] ? t : 0;
            r->max += subjects[j].max_marks;
        } else {
            k = marks_find(roll, subjects[j].code, exam);
            r->has[j] = k >= 0;
            r->cell[j] = k >= 0 ? marks[k].marks : 0;
            r->max += (exam < n_exams) ? exams[exam].max_marks : 0;
        }
        if (r->has[j]) { r->total += r->cell[j]; r->any = 1; }
        /* failure is always judged on the complete subject total, whatever examination is shown */
        subject_total(roll, j, &r->subtotal[j], NULL);
        if (subject_pct(roll, j) >= 0) {
            if (subject_pass(roll, j)) r->passed++; else { r->fail[j] = 1; r->failed++; }
        }
    }
    r->pct = (r->any && r->max > 0) ? r->total * 100.0 / r->max : -1.0;
    r->sgpa = student_sgpa(roll, &r->earned, &r->credits);
    r->pass = r->any && student_exam_pass(roll, EXAM_ALL);
}

/*
 * Filter test for the marks sheet.  grade_idx: -1 all, 0..6 = S..F.  result: 0 all, 1 pass, 2 fail.
 * subj >= 0 : grade and result are those of that subject.
 * subj < 0  : grade matches when at least one subject has it; result is the OVERALL result.
 */
int marks_filter_match(const char *roll, int subj, int grade_idx, int result)
{
    int j, pass;
    double p;
    if (subj >= 0) {
        p = subject_pct(roll, subj);
        if (p < 0) return grade_idx < 0 && result == 0;
        if (grade_idx >= 0 && grade_index(p) != grade_idx) return 0;
        pass = subject_pass(roll, subj);
        return result == 0 || (result == 1) == pass;
    }
    if (grade_idx >= 0) {
        int found = 0;
        for (j = 0; j < n_subjects && !found; j++) {
            p = subject_pct(roll, j);
            if (p >= 0 && grade_index(p) == grade_idx) found = 1;
        }
        if (!found) return 0;
    }
    if (result > 0) {
        if (student_exam_pct(roll, EXAM_ALL, NULL, NULL) < 0) return 0;
        pass = student_exam_pass(roll, EXAM_ALL);
        if ((result == 1) != pass) return 0;
    }
    return 1;
}

/*
 * Saves the marks of one student for all subjects at once.
 * vals/given are arrays indexed [subject * 3 + exam]; a box that is not given (blank)
 * removes that mark.  EVERY value is validated first - nothing is saved if one is invalid.
 */
int marks_save_student(const char *roll, const double *vals, const int *given, char *err)
{
    int j, e, k, ne = n_exams < 3 ? n_exams : 3;
    if (student_find(roll) < 0) { strcpy(err, "Student not found."); return 0; }
    for (j = 0; j < n_subjects; j++)
        for (e = 0; e < ne; e++) {
            double v = vals[j * 3 + e];
            if (!given[j * 3 + e]) continue;
            if (v < 0) { snprintf(err, 160, "%s, %s: marks cannot be negative.", subjects[j].short_name, exams[e].name); return 0; }
            if (v > exams[e].max_marks) {
                snprintf(err, 160, "%s, %s: marks cannot exceed %.0f.", subjects[j].short_name, exams[e].name, exams[e].max_marks);
                return 0;
            }
        }
    if (n_marks + n_subjects * ne > MAX_MARKS) { strcpy(err, "Marks storage is full."); return 0; }
    for (j = 0; j < n_subjects; j++)
        for (e = 0; e < ne; e++) {
            if (given[j * 3 + e]) marks_set(roll, subjects[j].code, e, vals[j * 3 + e], err);
            else if ((k = marks_find(roll, subjects[j].code, e)) >= 0) marks_remove(k);
        }
    return 1;
}
