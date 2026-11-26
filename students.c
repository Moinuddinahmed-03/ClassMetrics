/*
 * students.c : student records - add, edit, delete, search and sort.
 * Demonstrates: structures, arrays, strings, searching and sorting.
 */
#include "common.h"
#include <strings.h>

/* Linear search: compare the roll-number strings one by one. */
int student_find(const char *roll)
{
    int i;
    for (i = 0; i < n_students; i++)
        if (strcmp(students[i].roll, roll) == 0) return i;
    return -1;
}

/* Checks the fields of a student; writes a message to err when invalid. */
const char *BLOOD_GROUPS[8] = { "A+", "A-", "B+", "B-", "AB+", "AB-", "O+", "O-" };

/* Blood group is optional: empty or one of the eight groups. */
int blood_group_valid(const char *b)
{
    int k;
    if (b[0] == '\0') return 1;
    for (k = 0; k < 8; k++) if (strcmp(b, BLOOD_GROUPS[k]) == 0) return 1;
    return 0;
}

static int validate(Student *s, char *err)
{
    int i;
    clean_text(s->blood);
    if (!blood_group_valid(s->blood)) { strcpy(err, "Blood group must be A+, A-, B+, B-, AB+, AB-, O+, O- or empty."); return 0; }
    clean_text(s->name); clean_text(s->roll); clean_text(s->branch); clean_text(s->section);
    if (s->name[0] == '\0')    { strcpy(err, "Student name cannot be empty."); return 0; }
    if (s->roll[0] == '\0')    { strcpy(err, "Roll number cannot be empty."); return 0; }
    for (i = 0; s->roll[i] != '\0'; i++)
        if (isspace((unsigned char)s->roll[i])) { strcpy(err, "Roll number cannot contain spaces."); return 0; }
    if (s->branch[0] == '\0')  { strcpy(err, "Branch cannot be empty."); return 0; }
    if (s->section[0] == '\0') { strcpy(err, "Section cannot be empty."); return 0; }
    if (s->semester < 1 || s->semester > 8) { strcpy(err, "Semester must be between 1 and 8."); return 0; }
    return 1;
}

int student_add(const Student *in, char *err)
{
    Student s = *in;
    if (!validate(&s, err)) return 0;
    if (n_students >= MAX_STUDENTS) { strcpy(err, "Student limit reached."); return 0; }
    if (student_find(s.roll) >= 0) { strcpy(err, "A student with this roll number already exists."); return 0; }
    students[n_students++] = s;
    return 1;
}

/* Updating a roll number must also update marks and attendance rows. */
int student_update(int idx, const Student *in, char *err)
{
    Student s = *in;
    int other, i;
    if (idx < 0 || idx >= n_students) { strcpy(err, "No student selected."); return 0; }
    if (!validate(&s, err)) return 0;
    other = student_find(s.roll);
    if (other >= 0 && other != idx) { strcpy(err, "Another student already uses this roll number."); return 0; }
    if (strcmp(students[idx].roll, s.roll) != 0) {
        for (i = 0; i < n_marks; i++)
            if (strcmp(marks[i].roll, students[idx].roll) == 0) strcpy(marks[i].roll, s.roll);
        for (i = 0; i < n_atts; i++)
            if (strcmp(atts[i].roll, students[idx].roll) == 0) strcpy(atts[i].roll, s.roll);
    }
    students[idx] = s;
    return 1;
}

void student_delete(int idx)
{
    char roll[24];
    int i, w;
    if (idx < 0 || idx >= n_students) return;
    strcpy(roll, students[idx].roll);
    for (i = idx; i < n_students - 1; i++) students[i] = students[i + 1];
    n_students--;
    for (i = 0, w = 0; i < n_marks; i++)            /* compact the marks array */
        if (strcmp(marks[i].roll, roll) != 0) marks[w++] = marks[i];
    n_marks = w;
    for (i = 0, w = 0; i < n_atts; i++)
        if (strcmp(atts[i].roll, roll) != 0) atts[w++] = atts[i];
    n_atts = w;
}

/* Case-insensitive "contains" test used by the search box. */
static int contains_nocase(const char *text, const char *pat)
{
    size_t i, j, lt = strlen(text), lp = strlen(pat);
    if (lp == 0) return 1;
    for (i = 0; i + lp <= lt; i++) {
        for (j = 0; j < lp; j++)
            if (tolower((unsigned char)text[i + j]) != tolower((unsigned char)pat[j])) break;
        if (j == lp) return 1;
    }
    return 0;
}

/* Fills out[] with the indexes of matching students; returns how many. */
int student_search(const char *query, int *out)
{
    int i, n = 0;
    for (i = 0; i < n_students; i++)
        if (contains_nocase(students[i].name, query) || contains_nocase(students[i].roll, query))
            out[n++] = i;
    return n;
}

/* Value compared while sorting: key 0 roll, 1 name, 2 marks percentage. */
static int compare_students(int a, int b, int key)
{
    if (key == 0) return strcmp(students[a].roll, students[b].roll);
    if (key == 1) return strcmp(students[a].name, students[b].name);
    {
        double pa = student_exam_pct(students[a].roll, EXAM_ALL, NULL, NULL);   /* overall percentage */
        double pb = student_exam_pct(students[b].roll, EXAM_ALL, NULL, NULL);
        return (pa > pb) - (pa < pb);
    }
}

/* Insertion sort on an array of student indexes. */
void student_sort(int *idx, int n, int key, int ascending)
{
    int i, j, cur;
    for (i = 1; i < n; i++) {
        cur = idx[i];
        j = i - 1;
        while (j >= 0) {
            int c = compare_students(idx[j], cur, key);
            if (!ascending) c = -c;
            if (c <= 0) break;
            idx[j + 1] = idx[j];
            j--;
        }
        idx[j + 1] = cur;
    }
}

/*
 * Imports students from a CSV file with columns:
 *   roll, name, branch, section, semester     (branch/section/semester optional)
 * A header line is skipped.  Rows that are invalid or duplicate are skipped.
 * Returns 0 on success, 1 if the file cannot be opened.
 */
int student_import_csv(const char *path, int *added, int *skipped, char *first_problem)
{
    FILE *fp = fopen(path, "r");
    char line[400], *f[5], err[160];
    int lineno = 0, n, k;
    *added = *skipped = 0;
    first_problem[0] = '\0';
    if (fp == NULL) return 1;
    while (fgets(line, sizeof line, fp) != NULL) {
        char *p = line;
        Student s;
        lineno++;
        if (lineno == 1 && (unsigned char)p[0] == 0xEF) p += 3;          /* skip UTF-8 BOM */
        p[strcspn(p, "\r\n")] = '\0';
        if (p[0] == '\0') continue;
        for (n = 0, f[0] = p; n < 5; n++) {                               /* split at commas */
            char *c = strchr(f[n], ',');
            if (c == NULL) { n++; break; }
            *c = '\0';
            if (n < 4) f[n + 1] = c + 1;
        }
        for (k = n; k < 5; k++) f[k] = (char *)"";
        for (k = 0; k < 5; k++) {                                         /* strip quotes */
            char *q = f[k];
            if (*q == '"') q++;
            if (*q && q[strlen(q) - 1] == '"') q[strlen(q) - 1] = '\0';
            f[k] = q;
        }
        if (lineno == 1 && (strncasecmp(f[0], "roll", 4) == 0 || f[0][0] == '#')) continue;   /* header */
        memset(&s, 0, sizeof s);
        strncpy(s.roll, f[0], sizeof s.roll - 1);
        strncpy(s.name, f[1], sizeof s.name - 1);
        strncpy(s.branch, f[2][0] ? f[2] : "CSM", sizeof s.branch - 1);
        strncpy(s.section, f[3][0] ? f[3] : "B", sizeof s.section - 1);
        s.semester = 1;
        if (f[4][0] && !parse_int(f[4], &s.semester)) s.semester = 0;      /* 0 fails validation */
        if (student_add(&s, err)) (*added)++;
        else {
            (*skipped)++;
            if (first_problem[0] == '\0') snprintf(first_problem, 160, "line %d: %s", lineno, err);
        }
    }
    fclose(fp);
    return 0;
}
