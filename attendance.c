/*
 * attendance.c : attendance records.  A "class session" is identified by
 * (subject, date, period number).  A student can have only ONE record for
 * a given session, so att_set() edits the existing record instead of
 * adding a duplicate.
 */
#include "common.h"

int att_find(const char *roll, const char *code, const char *date, int session)
{
    int i;
    for (i = 0; i < n_atts; i++)
        if (atts[i].session == session && strcmp(atts[i].roll, roll) == 0 &&
            strcmp(atts[i].subject, code) == 0 && strcmp(atts[i].date, date) == 0)
            return i;
    return -1;
}

/*
 * One subject per period per day: if any attendance exists for (date, period), its subject is
 * copied to code_out and 1 is returned.  The lock is part of the data logic (not only of the screen).
 */
int att_period_subject(const char *date, int session, char *code_out)
{
    int i;
    for (i = 0; i < n_atts; i++)
        if (atts[i].session == session && strcmp(atts[i].date, date) == 0) {
            if (code_out) strcpy(code_out, atts[i].subject);
            return 1;
        }
    return 0;
}

/* Add or update one attendance record (no duplicates).  Returns 1 if stored, 0 if the storage is full
 * or the period of that date already belongs to ANOTHER subject. */
int att_set(const char *roll, const char *code, const char *date, int session, int present)
{
    int idx = att_find(roll, code, date, session);
    if (idx < 0) {
        char owner[16];
        if (att_period_subject(date, session, owner) && strcmp(owner, code) != 0) return 0;   /* period locked */
        if (n_atts >= MAX_ATT) return 0;
        idx = n_atts++;
        strcpy(atts[idx].roll, roll);
        strcpy(atts[idx].subject, code);
        strcpy(atts[idx].date, date);
        atts[idx].session = session;
    }
    atts[idx].present = present ? 1 : 0;
    return 1;
}

void att_delete_session(const char *code, const char *date, int session)
{
    int i, w = 0;
    for (i = 0; i < n_atts; i++)
        if (!(atts[i].session == session && strcmp(atts[i].subject, code) == 0 &&
              strcmp(atts[i].date, date) == 0))
            atts[w++] = atts[i];
    n_atts = w;
}

/* Counts classes conducted and attended for a student (code NULL = all subjects). */
void att_stats(const char *roll, const char *code, int *conducted, int *attended)
{
    int i, c = 0, a = 0;
    for (i = 0; i < n_atts; i++) {
        if (strcmp(atts[i].roll, roll) != 0) continue;
        if (code != NULL && strcmp(atts[i].subject, code) != 0) continue;
        c++;
        if (atts[i].present) a++;
    }
    if (conducted) *conducted = c;
    if (attended) *attended = a;
}

double att_pct(const char *roll, const char *code)
{
    int c, a;
    att_stats(roll, code, &c, &a);
    return c > 0 ? a * 100.0 / c : -1.0;
}

/* Low attendance = has records and the percentage is below the threshold. */
int att_is_low(const char *roll)
{
    double p = att_pct(roll, NULL);
    return p >= 0 && p < settings.att_threshold;
}

/* Average attendance of the students that have attendance records. */
double class_att_pct(void)
{
    int i, n = 0;
    double sum = 0, p;
    for (i = 0; i < n_students; i++) {
        p = att_pct(students[i].roll, NULL);
        if (p >= 0) { sum += p; n++; }
    }
    return n ? sum / n : -1.0;
}

/* Number of distinct (date,period) sessions held for a subject. */
int session_count(const char *code)
{
    int i, j, n = 0, seen;
    for (i = 0; i < n_atts; i++) {
        if (strcmp(atts[i].subject, code) != 0) continue;
        seen = 0;
        for (j = 0; j < i; j++)
            if (atts[j].session == atts[i].session && strcmp(atts[j].subject, code) == 0 &&
                strcmp(atts[j].date, atts[i].date) == 0) { seen = 1; break; }
        if (!seen) n++;
    }
    return n;
}

/* One subject below the threshold (and has records). */
int att_subject_low(const char *roll, const char *code)
{
    double p = att_pct(roll, code);
    return p >= 0 && p < settings.att_threshold;
}

/* Attendance categories: 0 excellent (90-100), 1 good (80-89), 2 satisfactory (75-79), 3 low (below 75). */
int att_category(double pct)
{
    if (pct >= 90) return 0;
    if (pct >= 80) return 1;
    if (pct >= settings.att_threshold) return 2;
    return 3;
}

const char *att_category_name(int cat)
{
    static const char *names[4] = { "Excellent", "Good", "Satisfactory", "Low" };
    return (cat >= 0 && cat < 4) ? names[cat] : "-";
}

/*
 * Saves the attendance of ALL students for one class session (date + period + subject).
 * present[i] = 1 / 0 for students[i].  Everything is validated first; nothing is saved on an error.
 * An existing session of the same subject is EDITED (no duplicates); a period that already
 * belongs to another subject on that date is refused.  Returns 1 on success.
 */
int att_save_session(const char *code, const char *date, int session, const int *present, char *err)
{
    char owner[16];
    int i;
    if (n_students == 0) { strcpy(err, "There are no students."); return 0; }
    if (subject_find(code) < 0) { strcpy(err, "Select a subject."); return 0; }
    if (!valid_date(date)) { strcpy(err, "Enter a valid date as YYYY-MM-DD."); return 0; }
    if (session < 1 || session > 8) { strcpy(err, "Period must be between 1 and 8."); return 0; }
    if (att_period_subject(date, session, owner) && strcmp(owner, code) != 0) {
        int oj = subject_find(owner);
        snprintf(err, 160, "Period %d on %s already belongs to %s.\nDelete that session in History first if it was recorded by mistake.",
                 session, date, oj >= 0 ? subjects[oj].name : owner);
        return 0;
    }
    for (i = 0; i < n_students; i++)
        if (present[i] != 0 && present[i] != 1) { strcpy(err, "Mark every student Present or Absent first."); return 0; }
    {   /* room for the new records? */
        int need = 0;
        for (i = 0; i < n_students; i++) if (att_find(students[i].roll, code, date, session) < 0) need++;
        if (n_atts + need > MAX_ATT) { strcpy(err, "Attendance storage is full."); return 0; }
    }
    for (i = 0; i < n_students; i++) att_set(students[i].roll, code, date, session, present[i]);
    return 1;
}

/*
 * Looks for date + period combinations that were recorded for MORE THAN ONE subject (old data).
 * Writes a readable list into report and returns the number of conflicting combinations.
 * Nothing is deleted, merged or changed.
 */
int att_find_conflicts(char *report, size_t cap)
{
    static struct { char date[12]; int session; char code[16]; } u[4000];
    int nu = 0, i, k, j, conflicts = 0, shown;
    if (report && cap) report[0] = '\0';
    for (i = 0; i < n_atts; i++) {
        for (k = 0; k < nu; k++)
            if (u[k].session == atts[i].session && strcmp(u[k].date, atts[i].date) == 0 && strcmp(u[k].code, atts[i].subject) == 0) break;
        if (k == nu && nu < 4000) {
            strcpy(u[nu].date, atts[i].date); u[nu].session = atts[i].session; strcpy(u[nu].code, atts[i].subject);
            nu++;
        }
    }
    for (i = 0; i < nu; i++) {
        shown = 0;
        for (k = 0; k < i; k++) if (u[k].session == u[i].session && strcmp(u[k].date, u[i].date) == 0) shown = 1;
        if (shown) continue;                                   /* this date + period was already examined */
        {
            int cnt = 0;
            for (j = i; j < nu; j++) if (u[j].session == u[i].session && strcmp(u[j].date, u[i].date) == 0) cnt++;
            if (cnt < 2) continue;
            conflicts++;
            if (report && cap) {
                sb_add(report, cap, "%s, period %d:", u[i].date, u[i].session);
                for (j = i, k = 0; j < nu; j++)
                    if (u[j].session == u[i].session && strcmp(u[j].date, u[i].date) == 0) sb_add(report, cap, "%s %s", k++ ? "," : "", u[j].code);
                sb_add(report, cap, "\n");
            }
        }
    }
    return conflicts;
}
