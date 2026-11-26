/*
 * subjects.c : configurable subjects (code, name, faculty, maximum marks,
 * passing marks, credits, syllabus units).
 */
#include "common.h"

int subject_find(const char *code)
{
    int i;
    for (i = 0; i < n_subjects; i++)
        if (strcmp(subjects[i].code, code) == 0) return i;
    return -1;
}

static int validate(Subject *s, char *err)
{
    clean_text(s->code); clean_text(s->name); clean_text(s->short_name);
    clean_text(s->faculty); clean_text(s->email); clean_text(s->type);
    if (s->code[0] == '\0') { strcpy(err, "Subject code cannot be empty."); return 0; }
    if (s->name[0] == '\0') { strcpy(err, "Subject name cannot be empty."); return 0; }
    if (s->type[0] == '\0') strcpy(s->type, "Theory");
    if (s->short_name[0] == '\0') snprintf(s->short_name, sizeof s->short_name, "%.7s", s->code);
    if (s->max_marks <= 0)  { strcpy(err, "Maximum marks must be greater than 0."); return 0; }
    if (s->pass_marks < 0 || s->pass_marks > s->max_marks) {
        strcpy(err, "Passing marks must be between 0 and the maximum marks."); return 0; }
    if (s->credits < 0 || s->credits > 10) { strcpy(err, "Credits must be between 0 and 10."); return 0; }
    return 1;
}

int subject_add(const Subject *in, char *err)
{
    Subject s = *in;
    if (!validate(&s, err)) return 0;
    if (n_subjects >= MAX_SUBJECTS) { strcpy(err, "Subject limit reached."); return 0; }
    if (subject_find(s.code) >= 0) { strcpy(err, "This subject code already exists."); return 0; }
    subjects[n_subjects++] = s;
    return 1;
}

/* Editing: a changed code is copied to marks/attendance; the syllabus units are kept. */
int subject_update(int idx, const Subject *in, char *err)
{
    Subject s = *in;
    int other, i;
    if (idx < 0 || idx >= n_subjects) { strcpy(err, "No subject selected."); return 0; }
    if (!validate(&s, err)) return 0;
    other = subject_find(s.code);
    if (other >= 0 && other != idx) { strcpy(err, "Another subject already uses this code."); return 0; }
    memcpy(s.units, subjects[idx].units, sizeof s.units);
    if (strcmp(subjects[idx].code, s.code) != 0) {
        for (i = 0; i < n_marks; i++)
            if (strcmp(marks[i].subject, subjects[idx].code) == 0) strcpy(marks[i].subject, s.code);
        for (i = 0; i < n_atts; i++)
            if (strcmp(atts[i].subject, subjects[idx].code) == 0) strcpy(atts[i].subject, s.code);
    }
    subjects[idx] = s;
    return 1;
}

/* Deleting a subject also deletes its marks and attendance. */
void subject_delete(int idx)
{
    char code[16];
    int i, w;
    if (idx < 0 || idx >= n_subjects) return;
    strcpy(code, subjects[idx].code);
    for (i = idx; i < n_subjects - 1; i++) subjects[i] = subjects[i + 1];
    n_subjects--;
    for (i = 0, w = 0; i < n_marks; i++)
        if (strcmp(marks[i].subject, code) != 0) marks[w++] = marks[i];
    n_marks = w;
    for (i = 0, w = 0; i < n_atts; i++)
        if (strcmp(atts[i].subject, code) != 0) atts[w++] = atts[i];
    n_atts = w;
}

int total_credits(void)
{
    int i, t = 0;
    for (i = 0; i < n_subjects; i++) t += subjects[i].credits;
    return t;
}

int total_units(void)
{
    int i, u, n = 0;
    for (i = 0; i < n_subjects; i++)
        for (u = 0; u < 5; u++) if (subjects[i].units[u][0] != '\0') n++;
    return n;
}
