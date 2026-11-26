/*
 * ClassMetrics - Student Marks & Attendance Analyzer
 * Team 3 | CSM-B | Semester 1 | 2026-27
 *
 * common.h : data structures, global tables and function prototypes shared by
 *            every module.  This header has NO GTK code, so the "logic" files
 *            (storage, students, subjects, marks, attendance, math_analyzer,
 *            reports) can be compiled and tested without the GUI.
 *
 * PPS concepts used across the project: structures, arrays (1-D and 2-D),
 * strings, pointers, function pointers, loops, conditions, file handling,
 * searching, sorting and mathematical calculations.
 *
 * ACADEMIC MODEL (demonstration data, not official college records)
 *   6 subjects x 3 examinations (CIE 1 /20, CIE 2 /20, SEE /60).
 *   One MarkRec holds the marks of one student in one subject for one examination.
 *   Subject total = CIE 1 + CIE 2 + SEE (out of 100).  Grades, grade points and SGPA
 *   are calculated from the subject totals (see marks.c).
 */
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>

/* ---------- Limits (fixed-size arrays keep the code beginner friendly) ---------- */
#define MAX_STUDENTS   200
#define MAX_SUBJECTS   12
#define MAX_EXAMS      8
#define MAX_MARKS      20000
#define MAX_ATT        40000
#define MAX_TEACHERS   5
#define MAT_MAX        MAX_STUDENTS    /* matrices are at most 200 x 200 */
#define EXAM_ALL       (-1)            /* "all examinations" (overall / totals) */
#define DATA_VERSION   4               /* data layout version (see storage.c migration) */
#define ROLL_BASE      990000000055LL   /* first roll = ROLL_BASE + 1 = 990000000056 */

/* ---------- Structures ---------- */
typedef struct {
    char name[48];
    char roll[24];             /* roll number kept as a STRING to keep every digit */
    char branch[16];
    char section[8];
    int  semester;
    char blood[8];             /* optional blood group (A+ ... O-), empty = not specified; never used in calculations */
} Student;

typedef struct {
    char   code[16];           /* e.g. U26MA101 */
    char   name[64];
    char   short_name[8];      /* e.g. M1 */
    char   faculty[48];
    char   email[56];
    char   type[16];           /* Theory */
    double max_marks;          /* total marks of the subject (100) */
    double pass_marks;         /* demonstration passing marks (40) */
    int    credits;            /* 0 is allowed (Environmental Science) */
    char   units[5][72];       /* syllabus units (information only) */
} Subject;

/* An examination is one assessment component, e.g. CIE 1 (max 20). */
typedef struct { char name[32]; double max_marks; } Exam;

typedef struct {
    char   roll[24];
    char   subject[16];        /* subject code */
    int    exam;               /* index into exams[] */
    double marks;
} MarkRec;

typedef struct {
    char roll[24];
    char subject[16];
    char date[12];             /* YYYY-MM-DD */
    int  session;              /* period number 1..8 */
    int  present;              /* 1 = present, 0 = absent */
} AttRec;

typedef struct {
    char username[24], password[24], name[48], faculty_id[16];
    char dept[56], email[56], cls[24], year[12];
    int  semester;
    char subject[56], subject_code[16], institution[72];
} Teacher;

typedef struct {
    double att_threshold;      /* default 75 */
    double weight_x;           /* weight of marks %      (default 70) */
    double weight_y;           /* weight of attendance % (default 30) */
    double grade_min[6];       /* minimum % for S, A, B, C, D, E (default 90,80,70,60,50,40) */
    int    data_version;
    int    animations;         /* 1 = small animations on (default), 0 = reduced motion */
} Settings;

/* ---------- Global tables (defined in storage.c) ---------- */
extern Student  students[MAX_STUDENTS];  extern int n_students;
extern Subject  subjects[MAX_SUBJECTS];  extern int n_subjects;
extern Exam     exams[MAX_EXAMS];        extern int n_exams;
extern MarkRec  marks[MAX_MARKS];        extern int n_marks;
extern AttRec   atts[MAX_ATT];           extern int n_atts;
extern Teacher  teachers[MAX_TEACHERS];  extern int n_teachers;
extern Settings settings;
extern char     migration_note[1000];     /* non-empty after a data migration */

/* ---------- Helpers (storage.c) ---------- */
void   clean_text(char *s);                       /* removes commas/newlines, trims */
int    parse_double(const char *s, double *out);  /* strict number parsing */
int    parse_int(const char *s, int *out);
int    valid_date(const char *s);                 /* YYYY-MM-DD check */
void   today_str(char *out);
void   sb_add(char *buf, size_t cap, const char *fmt, ...); /* append to buffer */
int    csv_split(char *line, char *f[], int maxf);

/* ---------- Storage (storage.c) ---------- */
void storage_set_dir(const char *dir);
const char *storage_dir(void);
int  storage_load_all(void);   /* 0 ok, 1 cannot write, 2 migration failed; seeds/migrates data */
int  storage_save_all(void);   /* returns 0 on success */

/* ---------- Students (students.c) ---------- */
int  student_find(const char *roll);                    /* linear search, -1 if none */
int  student_add(const Student *s, char *err);
int  student_update(int idx, const Student *s, char *err);
void student_delete(int idx);                           /* also removes marks/attendance */
int  student_search(const char *query, int *out);       /* by name or roll */
void student_sort(int *idx, int n, int key, int ascending); /* 0 roll,1 name,2 overall % */
int  student_import_csv(const char *path, int *added, int *skipped, char *first_problem);
int  blood_group_valid(const char *b);                       /* "" or one of the 8 groups */
extern const char *BLOOD_GROUPS[8];

/* ---------- Subjects (subjects.c) ---------- */
int  subject_find(const char *code);
int  subject_add(const Subject *s, char *err);
int  subject_update(int idx, const Subject *s, char *err);   /* keeps the syllabus units */
void subject_delete(int idx);
int  total_credits(void);                               /* sum of all subject credits */
int  total_units(void);                                 /* number of syllabus units stored */

/* ---------- Marks (marks.c) ---------- */
int    exam_add(const char *name, double max, char *err);
int    marks_find(const char *roll, const char *code, int exam);
int    marks_set(const char *roll, const char *code, int exam, double value, char *err);
void   marks_remove(int idx);
int    marks_import_csv(const char *path, int *added, int *skipped, char *first_problem);
int    marks_export_csv(const char *path, int choice, int *rows);   /* choice 0 CIE 1, 1 CIE 2, 2 SEE, 3 all */
double pct_of(double value, double max);
double exam_subject_max(int exam, int subj);            /* max of one exam (or the subject total) */
const char *grade_of(double pct);                       /* S, A, B, C, D, E, F (F = Fail) */
int    grade_index(double pct);                         /* 0..6 = S..F */
const char *grade_name(int idx);
const char *grade_performance(const char *grade);       /* Outstanding ... Fail */
void   grade_legend(char *buf, size_t cap);             /* legend text from the current scale */
double grade_points_of(double pct);                     /* 10 ... 0 */
int    marks_passed(double value, const Subject *s);    /* subject TOTAL passed? */
int    comp_passed(double value, double max, const Subject *s);  /* one exam passed? (same %) */
int    subject_total(const char *roll, int subj, double *total, double *maxv);  /* #exams found */
double subject_pct(const char *roll, int subj);         /* -1 if no marks */
int    subject_pass(const char *roll, int subj);        /* 1 = passed (needs marks) */
double student_exam_pct(const char *roll, int exam, double *total, double *maxtot); /* exam or EXAM_ALL */
int    student_exam_pass(const char *roll, int exam);   /* 1 = passed every recorded subject */
double student_sgpa(const char *roll, int *earned, int *credits_counted); /* -1 if no data */
int    latest_exam(void);                                /* last exam holding any marks, -1 none */
int    exam_ranking(int exam, int *order, double *pct);  /* returns count, best first */
int    student_rank(const char *roll, int exam);         /* 0 if unranked */
double subject_avg_pct(int subj, int exam);              /* -1 if no data; exam may be EXAM_ALL */
double class_avg_pct(int exam);
double class_pass_pct(int exam);
double class_avg_sgpa(void);
int    student_trend(const char *roll, double pct[MAX_EXAMS], int ex[MAX_EXAMS]); /* count */

/* consolidated student-wise marks sheet */
typedef struct {
    double cell[MAX_SUBJECTS];  int has[MAX_SUBJECTS];   /* per subject: marks of the chosen exam (or the subject total) */
    double total, max, pct;                              /* for the chosen exam; pct = -1 if no marks */
    double sgpa;  int earned, credits;                   /* always OVERALL */
    int any, pass;                                       /* pass = overall result */
    int fail[MAX_SUBJECTS];                              /* subject failed? (decided by the COMPLETE subject total) */
    double subtotal[MAX_SUBJECTS];                       /* complete subject total (out of the subject maximum) */
    int failed, passed;                                  /* number of failed / passed subjects */
} SheetRow;
void marks_sheet_row(const char *roll, int exam, SheetRow *r);
int  marks_filter_match(const char *roll, int subj, int grade_idx, int result);
int  marks_save_student(const char *roll, const double *vals, const int *given, char *err);

/* ---------- Attendance (attendance.c) ---------- */
int    att_find(const char *roll, const char *code, const char *date, int session);
int    att_set(const char *roll, const char *code, const char *date, int session, int present);
int    att_period_subject(const char *date, int session, char *code_out);   /* 1 if the period already has a subject */
int    att_save_session(const char *code, const char *date, int session, const int *present, char *err);
int    att_find_conflicts(char *report, size_t cap);        /* date+period recorded for several subjects */
void   att_delete_session(const char *code, const char *date, int session);
void   att_stats(const char *roll, const char *code, int *conducted, int *attended);
double att_pct(const char *roll, const char *code);      /* code NULL = all subjects, -1 none */
int    att_is_low(const char *roll);                     /* overall below threshold */
int    att_subject_low(const char *roll, const char *code); /* one subject below threshold */
int    att_category(double pct);                         /* 0 excellent 1 good 2 satisfactory 3 low */
const char *att_category_name(int cat);
double class_att_pct(void);
int    session_count(const char *code);                  /* classes held for subject */

/* ---------- Mathematical analyzer (math_analyzer.c) ---------- */
typedef struct { int rows, cols; double v[MAT_MAX][MAT_MAX]; } Matrix;

void mat_transpose(const Matrix *a, Matrix *t);
int  mat_add(const Matrix *a, const Matrix *b, Matrix *out);   /* 0 if sizes differ */
void mat_vec(const Matrix *a, const double *w, double *out);
void mat_marks(Matrix *m, int exam, const int *stu, int nrows); /* exam may be EXAM_ALL (totals) */
double perf_score(double x, double y, double wx, double wy);  /* P = wx*x + wy*y */
const char *trend_label(double rate);
int  mat_sub(const Matrix *a, const Matrix *b, Matrix *out);   /* a - b, 0 if sizes differ */

#define ATT_NEED 0
#define ATT_DONE 1
#define ATT_IMPOSSIBLE 2
#define ATT_NODATA 3
double att_projection(int attended, int held, int extra);   /* f(x) = 100 (A + x) / (T + x), -1 if no classes */
double att_slope(int attended, int held, int extra);        /* f'(x) = 100 (T - A) / (T + x)^2 */
int    att_classes_needed(int attended, int held, double target, long *extra);

/* ---------- Reports (reports.c) ---------- */
#define REP_ROWS 600
#define REP_COLS 16
typedef struct {
    char title[128];
    int  ncols, nrows;
    char head[REP_COLS][32];
    char cell[REP_ROWS][REP_COLS][48];
    int  flag[REP_ROWS];       /* 1 = highlight row as a warning */
    char prepared[256];        /* "Prepared by ..." line shown above the table and in the CSV */
} Report;

void report_set_teacher(int idx);                       /* teacher named in report headers */
void report_student(Report *r, const char *roll);
void report_class(Report *r, int exam);
void report_subject(Report *r, int subj, int exam);
void report_exam(Report *r, int exam);
void report_attendance(Report *r);
void report_low_attendance(Report *r);
void report_grades(Report *r);
void report_sgpa(Report *r);
void report_math(Report *r, int exam);
int  report_export_csv(const Report *r, const char *path);   /* 0 on success */

#endif
