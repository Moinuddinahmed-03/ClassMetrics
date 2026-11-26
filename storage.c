/*
 * storage.c : global data tables, small helper functions, CSV file handling,
 *             demonstration data and the version-1 -> version-2 data migration.
 *
 * Every table is saved to its own CSV file inside the data/ folder:
 *   teachers.csv students.csv subjects.csv exams.csv marks.csv
 *   attendance.csv settings.csv
 * Lines beginning with '#' are comments (header lines).
 *
 * All generated academic records are DEMONSTRATION data, not official college records.
 */
#include "common.h"
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

/* ---------- global tables ---------- */
Student  students[MAX_STUDENTS];  int n_students = 0;
Subject  subjects[MAX_SUBJECTS];  int n_subjects = 0;
Exam     exams[MAX_EXAMS];        int n_exams = 0;
MarkRec  marks[MAX_MARKS];        int n_marks = 0;
AttRec   atts[MAX_ATT];           int n_atts = 0;
Teacher  teachers[MAX_TEACHERS];  int n_teachers = 0;
Settings settings = { 75.0, 70.0, 30.0, { 90, 80, 70, 60, 50, 40 }, 0, 1 };
char     migration_note[1000] = "";

static char data_dir[260] = "data";

/* ================= helpers ================= */

/* Removes commas / line breaks (they would break the CSV) and trims spaces. */
void clean_text(char *s)
{
    int i, len;
    for (i = 0; s[i] != '\0'; i++)
        if (s[i] == ',' || s[i] == '\n' || s[i] == '\r' || s[i] == '"')
            s[i] = ' ';
    len = (int)strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
    i = 0;
    while (isspace((unsigned char)s[i])) i++;
    if (i > 0) memmove(s, s + i, strlen(s + i) + 1);
}

/* Strict number parsing: the whole text must be a number. */
int parse_double(const char *s, double *out)
{
    char *end;
    double v;
    if (s == NULL || *s == '\0') return 0;
    v = strtod(s, &end);
    while (isspace((unsigned char)*end)) end++;
    if (*end != '\0' || end == s) return 0;
    if (!(v == v) || v > 1e9 || v < -1e9) return 0;   /* rejects NaN / absurd values */
    *out = v;
    return 1;
}

int parse_int(const char *s, int *out)
{
    double d;
    if (!parse_double(s, &d) || d != floor(d)) return 0;
    *out = (int)d;
    return 1;
}

/* Checks the format YYYY-MM-DD and that the date really exists. */
int valid_date(const char *s)
{
    int y, m, d, i;
    static const int dim[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int maxd;
    if (strlen(s) != 10) return 0;
    for (i = 0; i < 10; i++) {
        if (i == 4 || i == 7) { if (s[i] != '-') return 0; }
        else if (!isdigit((unsigned char)s[i])) return 0;
    }
    y = atoi(s); m = atoi(s + 5); d = atoi(s + 8);
    if (m < 1 || m > 12 || y < 2000 || y > 2100) return 0;
    maxd = dim[m - 1];
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) maxd = 29;
    return d >= 1 && d <= maxd;
}

void today_str(char *out)
{
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    strftime(out, 12, "%Y-%m-%d", lt);
}

/* Appends formatted text to buf without overflowing. */
void sb_add(char *buf, size_t cap, const char *fmt, ...)
{
    size_t len = strlen(buf);
    va_list ap;
    if (len + 1 >= cap) return;
    va_start(ap, fmt);
    vsnprintf(buf + len, cap - len, fmt, ap);
    va_end(ap);
}

/* ================= low level CSV ================= */

void storage_set_dir(const char *dir)
{
    strncpy(data_dir, dir, sizeof(data_dir) - 1);
    data_dir[sizeof(data_dir) - 1] = '\0';
}

const char *storage_dir(void) { return data_dir; }

static void path_of(char *out, size_t cap, const char *file)
{
    snprintf(out, cap, "%s/%s", data_dir, file);
}

/* Splits a line at commas; returns number of fields. */
int csv_split(char *line, char *f[], int maxf)
{
    int n = 0;
    char *p = line;
    f[n++] = p;
    while (*p != '\0' && n < maxf) {
        if (*p == ',') { *p = '\0'; f[n++] = p + 1; }
        p++;
    }
    return n;
}

/* Copies text into a fixed-size char array safely. */
static void copy_to(char *dst, size_t cap, const char *src)
{
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

/*
 * Reads a CSV file line by line and hands every record to 'handler'
 * (a pointer to a function).  Returns 0 when the file is missing.
 */
static int read_csv(const char *file, int nfields, void (*handler)(char **, int))
{
    char path[300], line[1024], *f[20];
    FILE *fp;
    int n;
    path_of(path, sizeof(path), file);
    fp = fopen(path, "r");
    if (fp == NULL) return 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;
        n = csv_split(line, f, 20);
        if (n >= nfields) handler(f, n);   /* ignore damaged lines */
    }
    fclose(fp);
    return 1;
}

static void h_teacher(char **f, int n)
{
    Teacher *t;
    if (n_teachers >= MAX_TEACHERS) return;
    t = &teachers[n_teachers++];
    memset(t, 0, sizeof *t);
    copy_to(t->username, sizeof t->username, f[0]);
    copy_to(t->password, sizeof t->password, f[1]);
    copy_to(t->name, sizeof t->name, f[2]);
    copy_to(t->faculty_id, sizeof t->faculty_id, f[3]);
    copy_to(t->dept, sizeof t->dept, f[4]);
    copy_to(t->email, sizeof t->email, f[5]);
    copy_to(t->cls, sizeof t->cls, f[6]);
    t->semester = atoi(f[7]);
    copy_to(t->year, sizeof t->year, f[8]);
    if (n >= 12) {                                   /* columns added in data version 2 */
        copy_to(t->subject, sizeof t->subject, f[9]);
        copy_to(t->subject_code, sizeof t->subject_code, f[10]);
        copy_to(t->institution, sizeof t->institution, f[11]);
    }
}

static void h_student(char **f, int n)
{
    Student *s;
    if (n_students >= MAX_STUDENTS) return;
    s = &students[n_students++];
    copy_to(s->roll, sizeof s->roll, f[0]);
    copy_to(s->name, sizeof s->name, f[1]);
    copy_to(s->branch, sizeof s->branch, f[2]);
    copy_to(s->section, sizeof s->section, f[3]);
    s->semester = atoi(f[4]);
    s->blood[0] = '\0';
    if (n >= 6) {                                      /* optional column added with the blood group */
        copy_to(s->blood, sizeof s->blood, f[5]);
        if (!blood_group_valid(s->blood)) s->blood[0] = '\0';
    }
}

static void h_subject(char **f, int n)
{
    Subject *s;
    int u;
    if (n_subjects >= MAX_SUBJECTS) return;
    s = &subjects[n_subjects++];
    memset(s, 0, sizeof *s);
    copy_to(s->code, sizeof s->code, f[0]);
    copy_to(s->name, sizeof s->name, f[1]);
    s->max_marks = atof(f[2]);
    s->pass_marks = atof(f[3]);
    s->credits = atoi(f[4]);
    if (s->credits < 0) s->credits = 0;              /* zero credits are allowed */
    copy_to(s->type, sizeof s->type, "Theory");
    if (n >= 9) {                                    /* columns added in data version 2 */
        copy_to(s->short_name, sizeof s->short_name, f[5]);
        copy_to(s->faculty, sizeof s->faculty, f[6]);
        copy_to(s->email, sizeof s->email, f[7]);
        copy_to(s->type, sizeof s->type, f[8]);
    }
    for (u = 0; u < 5 && 9 + u < n; u++) {
        char *c;
        copy_to(s->units[u], sizeof s->units[u], f[9 + u]);
        while ((c = strchr(s->units[u], '~')) != NULL) *c = ',';      /* ~ stands for a comma in the file */
    }
}

static void h_exam(char **f, int n)
{
    Exam *e;
    if (n_exams >= MAX_EXAMS) return;
    e = &exams[n_exams++];
    copy_to(e->name, sizeof e->name, f[0]);
    e->max_marks = (n >= 2) ? atof(f[1]) : 0.0;
}

static void h_mark(char **f, int n)
{
    MarkRec *m;
    (void)n;
    if (n_marks >= MAX_MARKS) return;
    m = &marks[n_marks++];
    copy_to(m->roll, sizeof m->roll, f[0]);
    copy_to(m->subject, sizeof m->subject, f[1]);
    m->exam = atoi(f[2]);
    m->marks = atof(f[3]);
}

static void h_att(char **f, int n)
{
    AttRec *a;
    (void)n;
    if (n_atts >= MAX_ATT) return;
    a = &atts[n_atts++];
    copy_to(a->roll, sizeof a->roll, f[0]);
    copy_to(a->subject, sizeof a->subject, f[1]);
    copy_to(a->date, sizeof a->date, f[2]);
    a->session = atoi(f[3]);
    a->present = atoi(f[4]) ? 1 : 0;
}

static void h_setting(char **f, int n)
{
    (void)n;
    if (strcmp(f[0], "att_threshold") == 0) settings.att_threshold = atof(f[1]);
    else if (strcmp(f[0], "weight_x") == 0) settings.weight_x = atof(f[1]);
    else if (strcmp(f[0], "weight_y") == 0) settings.weight_y = atof(f[1]);
    else if (strcmp(f[0], "data_version") == 0) settings.data_version = atoi(f[1]);
    else if (strcmp(f[0], "animations") == 0) settings.animations = atoi(f[1]) ? 1 : 0;
    else if (strncmp(f[0], "grade_min_", 10) == 0) {
        int k = atoi(f[0] + 10);
        if (k >= 0 && k < 6) settings.grade_min[k] = atof(f[1]);
    }
}

/* ================= demonstration data (version 2) ================= */

static unsigned long seed_state = 2026;

/* Simple linear congruential generator -> repeatable demo data. */
static int rnd(int lo, int hi)
{
    seed_state = seed_state * 1103515245UL + 12345UL;
    return lo + (int)((seed_state >> 16) % (unsigned long)(hi - lo + 1));
}

/* First 20 names are the original demonstration students (identities preserved). */
static const char *NAMES[] = {
    "Aarav Mehta", "Diya Nair", "Rohan Verma", "Sneha Reddy", "Karthik Iyer",
    "Ananya Gupta", "Vikram Singh", "Pooja Patel", "Arjun Rao", "Meera Joshi",
    "Rahul Das", "Ishita Bose", "Aditya Kulkarni", "Kavya Menon", "Siddharth Jain",
    "Nisha Khan", "Manoj Kumar", "Tanvi Shah", "Faizan Ahmed", "Lakshmi Prasad",
    /* added in version 2 (alternating female / male) */
    "Priya Sharma", "Mohammed Imran", "Aisha Siddiqui", "Syed Arfan", "Neha Kapoor",
    "Rajesh Naik", "Fatima Zaheer", "Varun Choudhary", "Divya Menon", "Abdul Rahman",
    "Riya Agarwal", "Harsh Vardhan", "Shreya Iyer", "Naveen Reddy", "Zainab Sheikh",
    "Yusuf Pathan", "Harini Subramanian", "Nikhil Sawant", "Ayesha Begum", "Omkar Patil",
    "Pallavi Deshmukh", "Suresh Yadav", "Madhuri Rao", "Zaid Mirza", "Swathi Reddy",
    "Tarun Malhotra", "Sumaiya Fatima", "Imtiaz Ahmed", "Anjali Singh", "Sai Teja Goud",
    "Bhavana Naidu", "Prashant Shetty", "Tabassum Ali", "Akash Thakur", "Rukhsar Parveen",
    "Danish Farooqui", "Nandini Hegde", "Gaurav Bhatt", "Mehak Arora", "Vinay Chaturvedi",
    "Sara Hussain", "Aman Khurana", "Lavanya Gowda"
};
#define N_NAMES ((int)(sizeof NAMES / sizeof NAMES[0]))

/* Per-student demonstration profile (index = position in the 55-student list). */
static const int ABILITY[55] = {
    88, 82, 74, 91, 67, 79, 55, 71, 48, 85, 38, 76, 63, 69, 44, 81, 58, 73, 52, 66,
    75, 92, 61, 47, 83, 72, 36, 66, 89, 53, 78, 58, 95, 70, 45, 86, 64, 33, 77, 81,
    57, 50, 73, 90, 68, 40, 87, 60, 49, 71, 63, 31, 79, 52, 74 };
static const int TREND[55] = {     /* SEE level minus CIE 1 level, in % points */
    3, 4, -2, 2, 6, 0, -4, 5, 7, 1, -5, 2, -3, 4, 6, -1, 3, 2, -6, 5,
    3, -2, 5, 1, -4, 2, 6, -1, 0, 4, -3, 7, -2, 3, 5, -5, 2, 8, -1, 4,
    -6, 3, 1, -2, 5, 6, -3, 2, 4, -4, 1, 7, -2, 3, -1 };
static const int ATTBASE[55] = {   /* typical attendance % */
    95, 92, 85, 97, 78, 90, 62, 80, 70, 93, 55, 88, 74, 82, 66, 91, 72, 84, 60, 79,
    96, 84, 77, 68, 91, 88, 79, 58, 98, 82, 76, 71, 93, 86, 78, 64, 90, 85, 75, 66,
    94, 81, 77, 52, 95, 87, 79, 73, 92, 80, 76, 83, 89, 84, 82 };
/* {student index, subject index} pairs that are weak enough to fail that subject */
static const int WEAK[][2] = { {12, 1}, {17, 3}, {22, 0}, {28, 5}, {35, 2}, {44, 4}, {49, 1}, {5, 5}, {30, 3} };
#define N_WEAK ((int)(sizeof WEAK / sizeof WEAK[0]))

static void seed_teacher_fields(Teacher *t)
{
    copy_to(t->name, sizeof t->name, "Ms. PPS Faculty");
    copy_to(t->email, sizeof t->email, "pps.faculty@example.edu");
    copy_to(t->subject, sizeof t->subject, "Programming for Problem Solving");
    copy_to(t->subject_code, sizeof t->subject_code, "U26CS101");
    copy_to(t->institution, sizeof t->institution, "Demo Institute of Engineering and Technology");
}

static void seed_teacher(void)
{
    n_teachers = 1;
    memset(&teachers[0], 0, sizeof teachers[0]);
    copy_to(teachers[0].username, 24, "teacher");
    copy_to(teachers[0].password, 24, "class123");
    copy_to(teachers[0].faculty_id, 16, "FAC1024");
    copy_to(teachers[0].dept, 56, "CSE (AI & ML)");
    copy_to(teachers[0].cls, 24, "CSM-B");
    teachers[0].semester = 1;
    copy_to(teachers[0].year, 12, "2026-27");
    seed_teacher_fields(&teachers[0]);
}

/* Adds generated students (roll 990000000056, 990000000057, ...) until 'target' exist.
 * Existing students are never changed; a roll number or name already in use is skipped. */
static void seed_students_to(int target)
{
    int k, i, dup;
    char roll[24], oldroll[24];
    for (k = 0; k < N_NAMES && n_students < target && n_students < MAX_STUDENTS; k++) {
        snprintf(roll, sizeof roll, "%lld", ROLL_BASE + k + 1);   /* 990000000056, 990000000057, ... */
        snprintf(oldroll, sizeof oldroll, "0262%03d", k + 1);    /* the same student in the previous roll format */
        dup = 0;
        for (i = 0; i < n_students; i++)
            if (strcmp(students[i].roll, roll) == 0 || strcmp(students[i].roll, oldroll) == 0 || strcmp(students[i].name, NAMES[k]) == 0) dup = 1;
        if (dup) continue;
        memset(&students[n_students], 0, sizeof students[0]);
        copy_to(students[n_students].roll, 24, roll);
        copy_to(students[n_students].name, 48, NAMES[k]);
        copy_to(students[n_students].branch, 16, "CSM");
        copy_to(students[n_students].section, 8, "B");
        students[n_students].semester = 1;
        n_students++;
    }
}

static void set_subject(int j, const char *code, const char *name, const char *shortn, const char *fac,
                        const char *mail, int credits, const char *u1, const char *u2, const char *u3,
                        const char *u4, const char *u5)
{
    Subject *s = &subjects[j];
    memset(s, 0, sizeof *s);
    copy_to(s->code, sizeof s->code, code);
    copy_to(s->name, sizeof s->name, name);
    copy_to(s->short_name, sizeof s->short_name, shortn);
    copy_to(s->faculty, sizeof s->faculty, fac);
    copy_to(s->email, sizeof s->email, mail);
    copy_to(s->type, sizeof s->type, "Theory");
    s->max_marks = 100;
    s->pass_marks = 40;
    s->credits = credits;
    copy_to(s->units[0], sizeof s->units[0], u1);
    copy_to(s->units[1], sizeof s->units[1], u2);
    copy_to(s->units[2], sizeof s->units[2], u3);
    copy_to(s->units[3], sizeof s->units[3], u4);
    copy_to(s->units[4], sizeof s->units[4], u5);
}

/* The six subjects of semester 1 with faculty, credits and syllabus units. */
static void seed_subjects_and_exams(void)
{
    set_subject(0, "U26MA101", "Mathematics-I", "M1", "Dr. Math Faculty", "math.faculty@example.edu", 4,
                "Matrices", "Calculus of One Variable", "Multivariable Calculus (Differentiation)",
                "Multivariable Calculus (Integration)", "Special Functions and Power Series Solution of ODE");
    set_subject(1, "U26CH101", "Engineering Chemistry", "EC", "Dr. Chemistry Faculty", "chemistry.faculty@example.edu", 3,
                "Electrochemistry", "Water Chemistry and Corrosion", "Polymers", "Chemical Fuels",
                "Green Chemistry & Spectroscopy");
    set_subject(2, "U26CS101", "Programming for Problem Solving", "PPS", "Ms. PPS Faculty", "pps.faculty@example.edu", 3,
                "Introduction to Programming and C Fundamentals", "Control Structures and Functions",
                "Recursion, Preprocessor and Arrays", "Pointers", "Structures, Unions and File Handling");
    set_subject(3, "U26EN101", "English for Professional Communication", "EPC", "Ms. English Faculty",
                "english.faculty@example.edu", 3, "Syllabus", "My Mother at Sixty-Six",
                "Dabbawallahs: Mumbai's Business", "A Swarm of Cyborg Insects", "The Rule of the Road");
    set_subject(4, "U26EN103", "Universal Human Values", "UHV", "Ms. UHV Faculty", "uhv.faculty@example.edu", 1,
                "Introduction to Value Education", "Universal Human Value",
                "Cyber Ethics and Privacy in the Digital Age", "Professional Ethics", "Indian Knowledge Systems");
    set_subject(5, "U26CH102", "Environmental Science", "EVS", "Dr. EVS Faculty", "evs.faculty@example.edu", 0,
                "Introduction to Environmental Studies", "Ecosystem – Introduction", "Biodiversity – I",
                "Environmental Pollution – Introduction & Air Pollution", "Watershed Management & Environmental Ethics");
    n_subjects = 6;

    copy_to(exams[0].name, 32, "CIE 1");      exams[0].max_marks = 20;
    copy_to(exams[1].name, 32, "CIE 2");      exams[1].max_marks = 20;
    copy_to(exams[2].name, 32, "SEE"); exams[2].max_marks = 60;
    n_exams = 3;
}

/* Marks for every student in every subject and examination (990 records for 55 students).
 * A student's general ability decides the level; each subject, each exam and the
 * improvement/decline trend add realistic variation. */
static void seed_marks(void)
{
    int i, j, e, w, weak, pct, skill;
    seed_state = 2026;
    n_marks = 0;
    for (i = 0; i < n_students; i++) {
        int ability = i < 55 ? ABILITY[i] : 55 + (i * 7) % 30;
        int trend = i < 55 ? TREND[i] : (i % 9) - 4;
        for (j = 0; j < n_subjects; j++) {
            weak = 0;
            for (w = 0; w < N_WEAK; w++) if (WEAK[w][0] == i && WEAK[w][1] == j) weak = 1;
            skill = ability + rnd(-8, 8);
            if (weak) skill = 26 + rnd(0, 6);          /* a subject this student fails */
            for (e = 0; e < n_exams; e++) {
                double m;
                if (e == 0) pct = skill + rnd(-6, 6);
                else if (e == 1) pct = skill + trend / 2 + rnd(-6, 6);
                else pct = skill + trend + rnd(-7, 7);
                if (weak && pct > 34) pct = 34;
                if (pct > 99) pct = 99;
                if (pct < 5) pct = 5;
                m = floor(exams[e].max_marks * pct / 100.0 * 2.0 + 0.5) / 2.0;   /* nearest 0.5 */
                if (m > exams[e].max_marks) m = exams[e].max_marks;
                if (n_marks >= MAX_MARKS) return;
                copy_to(marks[n_marks].roll, 24, students[i].roll);
                copy_to(marks[n_marks].subject, 16, subjects[j].code);
                marks[n_marks].exam = e;
                marks[n_marks].marks = m;
                n_marks++;
            }
        }
    }
}

/* Class sessions from 1 Sep 2026 on weekdays: a different set of weekdays and a
 * different number of classes for every subject. */
static void seed_attendance(void)
{
    static const int target[6] = { 40, 30, 30, 30, 16, 16 };
    static const int mask[6] = { 0x36, 0x2A, 0x34, 0x1A, 0x28, 0x14 };   /* bit n = weekday n (1=Mon) */
    int held[6] = { 0, 0, 0, 0, 0, 0 }, day, j, i, done, w, p;
    struct tm base;
    seed_state = 77;
    n_atts = 0;
    memset(&base, 0, sizeof base);
    base.tm_year = 2026 - 1900; base.tm_mon = 8; base.tm_mday = 1; base.tm_hour = 12;
    for (day = 0; day < 200; day++) {
        char date[12];
        struct tm t2 = base;
        time_t tt;
        t2.tm_mday += day;
        tt = mktime(&t2);                              /* normalises the date */
        t2 = *localtime(&tt);
        if (t2.tm_wday == 0 || t2.tm_wday == 6) continue;
        strftime(date, sizeof date, "%Y-%m-%d", &t2);
        for (j = 0; j < n_subjects && j < 6; j++) {
            if (!(mask[j] & (1 << t2.tm_wday)) || held[j] >= target[j]) continue;
            held[j]++;
            for (i = 0; i < n_students; i++) {
                AttRec *a;
                if (n_atts >= MAX_ATT) return;
                w = i < 55 ? ATTBASE[i] : 70 + (i * 11) % 28;
                p = w + rnd(-4, 4);                    /* a little variation per subject */
                if (p > 100) p = 100;
                a = &atts[n_atts++];
                copy_to(a->roll, 24, students[i].roll);
                copy_to(a->subject, 16, subjects[j].code);
                copy_to(a->date, 12, date);
                a->session = j + 1;
                a->present = (rnd(1, 100) <= p) ? 1 : 0;
            }
        }
        done = 1;
        for (j = 0; j < 6; j++) if (held[j] < target[j]) done = 0;
        if (done) break;
    }
}

/* ================= demonstration data update (data version 3) =================
 * 1. Blood groups (synthetic) for students that have none.
 * 2. A few failing demonstration students receive higher marks so that the class
 *    pass rate is realistic.  Applied once (data_version 3); never on every start. */

#define DEMO_TARGET_FAILING 12      /* demonstration class: 12 of 55 students fail (43 pass = 78.2 %) */

/* Realistic distribution for 55 students: positive groups are more common. */
static void assign_blood_groups(int *assigned)
{
    static const char *grp[8] = { "O+", "B+", "A+", "AB+", "O-", "A-", "B-", "AB-" };
    static const int cnt[8] = { 15, 13, 11, 4, 4, 3, 3, 2 };            /* = 55 */
    const char *list[55];
    int i, k, n = 0, j;
    for (k = 0; k < 8; k++) for (j = 0; j < cnt[k]; j++) list[n++] = grp[k];
    seed_state = 505;
    for (i = n - 1; i > 0; i--) {                                      /* shuffle (Fisher-Yates) */
        const char *tmp;
        j = rnd(0, i);
        tmp = list[i]; list[i] = list[j]; list[j] = tmp;
    }
    *assigned = 0;
    for (i = 0; i < n_students; i++)
        if (students[i].blood[0] == '\0') {                            /* existing values are kept */
            copy_to(students[i].blood, sizeof students[i].blood, list[i % 55]);
            (*assigned)++;
        }
}

/* Raises the marks of one subject so that its total becomes 'target' (SEE first, then CIE 2, CIE 1). */
static void raise_subject_total(const char *roll, int subj, double target)
{
    static const int order[3] = { 2, 1, 0 };
    static const double frac[3] = { 0.6, 0.2, 0.2 };
    int idx[3], q, pass;
    double cur[3], total = 0, rem;
    for (q = 0; q < 3; q++) {
        idx[q] = marks_find(roll, subjects[subj].code, q);
        if (idx[q] < 0) return;                                        /* incomplete record: leave it */
        cur[q] = marks[idx[q]].marks;
        total += cur[q];
    }
    rem = target - total;
    for (pass = 0; pass < 2 && rem > 0; pass++)
        for (q = 0; q < 3 && rem > 0; q++) {
            int e = order[q];
            double add = pass == 0 ? floor(rem * frac[q] * 2.0 + 0.5) / 2.0 : rem;
            double room = exams[e].max_marks - cur[e];
            if (add > rem) add = rem;
            if (add > room) add = room;
            cur[e] += add;
            rem -= add;
        }
    for (q = 0; q < 3; q++) marks[idx[q]].marks = cur[q];
}

/* Lets the students who are closest to passing pass (smallest shortfall first). */
static void raise_failing_students(int *fixed)
{
    int cand[MAX_STUDENTS], n = 0, i, j, k, best, t;
    double def[MAX_STUDENTS], d, tot;
    *fixed = 0;
    for (i = 0; i < n_students; i++) {
        int f = 0;
        d = 0;
        for (j = 0; j < n_subjects; j++)
            if (subject_pct(students[i].roll, j) >= 0 && !subject_pass(students[i].roll, j)) {
                subject_total(students[i].roll, j, &tot, NULL);
                d += subjects[j].pass_marks - tot;
                f = 1;
            }
        if (f) { cand[n] = i; def[n] = d; n++; }
    }
    for (i = 0; i < n - 1; i++) {                                      /* selection sort, smallest shortfall first */
        best = i;
        for (j = i + 1; j < n; j++) if (def[j] < def[best]) best = j;
        if (best != i) { d = def[i]; def[i] = def[best]; def[best] = d; t = cand[i]; cand[i] = cand[best]; cand[best] = t; }
    }
    seed_state = 303;
    for (k = 0; k < n - DEMO_TARGET_FAILING; k++)
        for (j = 0; j < n_subjects; j++)
            if (subject_pct(students[cand[k]].roll, j) >= 0 && !subject_pass(students[cand[k]].roll, j)) {
                /* a varied result just above the passing marks (40.5 ... 46.5), not exactly 40 */
                raise_subject_total(students[cand[k]].roll, j, subjects[j].pass_marks + 0.5 + 0.5 * rnd(0, 12));
            }
    *fixed = n > DEMO_TARGET_FAILING ? n - DEMO_TARGET_FAILING : 0;
}

/* marks_are_demo = 0 when the marks were edited by the teacher: then they are not touched. */
static void apply_v3_demo(int marks_are_demo, int *blood_n, int *fixed_n)
{
    assign_blood_groups(blood_n);
    *fixed_n = 0;
    if (marks_are_demo) raise_failing_students(fixed_n);
}

static void seed_demo_data(void)
{
    int b, f;
    n_students = n_subjects = n_exams = n_marks = n_atts = 0;
    seed_teacher();
    seed_students_to(55);
    seed_subjects_and_exams();
    seed_marks();
    seed_attendance();
    apply_v3_demo(1, &b, &f);
    settings.att_threshold = 75.0;
    settings.weight_x = 70.0;
    settings.weight_y = 30.0;
    settings.data_version = DATA_VERSION;
}

/* ================= migration: data version 1 -> 2 ================= */

static int copy_file(const char *src, const char *dst)
{
    FILE *a = fopen(src, "rb"), *b;
    char buf[4096];
    size_t n;
    if (a == NULL) return 0;                           /* nothing to copy */
    b = fopen(dst, "wb");
    if (b == NULL) { fclose(a); return 1; }
    while ((n = fread(buf, 1, sizeof buf, a)) > 0)
        if (fwrite(buf, 1, n, b) != n) { fclose(a); fclose(b); return 1; }
    fclose(a);
    return fclose(b) != 0;
}

static int is_old_demo_code(const char *c)
{
    static const char *old[5] = { "MA101", "CS102", "PH103", "EE104", "EN105" };
    int k;
    for (k = 0; k < 5; k++) if (strcmp(c, old[k]) == 0) return 1;
    return 0;
}

/*
 * Replaces the old demonstration subjects / examinations / marks / attendance
 * with the new academic structure.  Students (identities), settings and the
 * teacher account are kept.  The old files are copied to a backup folder first.
 * Returns 0 on success, 1 when the backup could not be made (nothing changed).
 */
static int migrate_v1_to_v2(void)
{
    static const char *files[7] = { "teachers.csv", "students.csv", "subjects.csv", "exams.csv",
                                    "marks.csv", "attendance.csv", "settings.csv" };
    char stamp[24], bdir[320], src[320], dst[360], report[360];
    int k, conflict = 0, old_students = n_students, old_marks = n_marks, old_atts = n_atts, old_subjects = n_subjects;
    time_t t = time(NULL);
    FILE *fp;

    strftime(stamp, sizeof stamp, "%Y%m%d_%H%M%S", localtime(&t));
    snprintf(bdir, sizeof bdir, "%s_backup_v1_%s", data_dir, stamp);
    if (MKDIR(bdir) != 0 && errno != EEXIST) return 1;
    for (k = 0; k < 7; k++) {
        path_of(src, sizeof src, files[k]);
        snprintf(dst, sizeof dst, "%s/%s", bdir, files[k]);
        if (copy_file(src, dst) != 0) return 1;
    }

    /* Records that do not look like the old demonstration data are reported as a conflict. */
    for (k = 0; k < n_subjects; k++) if (!is_old_demo_code(subjects[k].code)) conflict = 1;
    if (old_subjects != 5 || n_exams != 3 || old_marks != 300 || old_atts != 1000) conflict = 1;

    /* teacher account: keep login, update the demo profile */
    for (k = 0; k < n_teachers; k++)
        if (strcmp(teachers[k].username, "teacher") == 0) seed_teacher_fields(&teachers[k]);
    if (n_teachers == 0) seed_teacher();

    n_subjects = n_exams = n_marks = n_atts = 0;
    seed_subjects_and_exams();
    seed_students_to(55);                              /* keeps every existing student */
    seed_marks();
    seed_attendance();
    {   int b, f;                                      /* data version 3 steps for the freshly generated demo data */
        apply_v3_demo(1, &b, &f);
    }
    settings.data_version = 3;                    /* the version-4 step (roll numbers, CIE/SEE names) follows */

    snprintf(migration_note, sizeof migration_note,
             "Data was upgraded to the new academic structure (6 subjects, 3 examinations, %d students).\n"
             "Your previous data files were copied to:\n%s\n%s",
             n_students, bdir,
             conflict ? "The old subjects, marks or attendance did not match the original demonstration data\n"
                        "(possible manual entries). They were NOT merged; see migration_report.txt and the backup."
                      : "The old demonstration subjects, marks and attendance were replaced.");

    path_of(report, sizeof report, "migration_report.txt");
    fp = fopen(report, "w");
    if (fp != NULL) {
        fprintf(fp, "ClassMetrics data migration to version %d (%s)\n\n", DATA_VERSION, stamp);
        fprintf(fp, "Backup folder          : %s\n", bdir);
        fprintf(fp, "Students kept          : %d (now %d after adding demonstration students)\n", old_students, n_students);
        fprintf(fp, "Old subjects replaced  : %d\nOld marks records      : %d (kept in backup only)\n", old_subjects, old_marks);
        fprintf(fp, "Old attendance records : %d (kept in backup only)\n", old_atts);
        fprintf(fp, "Conflict detected      : %s\n", conflict ? "YES - old data did not match the original demonstration data" : "no");
        fprintf(fp, "\nAll generated marks and attendance are demonstration data, not official records.\n");
        fclose(fp);
    }
    return 0;
}

/*
 * Data version 2 -> 3: assigns blood groups to students without one and, when the marks are
 * still the original demonstration marks, lets the students closest to passing pass.
 * The old files are copied to a backup folder first.  Returns 1 when the backup failed.
 */
static int migrate_v2_to_v3(void)
{
    static const char *files[7] = { "teachers.csv", "students.csv", "subjects.csv", "exams.csv",
                                    "marks.csv", "attendance.csv", "settings.csv" };
    static MarkRec loaded[MAX_MARKS];
    char stamp[24], bdir[320], src[320], dst[360], report[360];
    int k, loaded_n, pristine = 1, blood_n = 0, fixed_n = 0;
    time_t t = time(NULL);
    FILE *fp;

    strftime(stamp, sizeof stamp, "%Y%m%d_%H%M%S", localtime(&t));
    snprintf(bdir, sizeof bdir, "%s_backup_v2_%s", data_dir, stamp);
    if (MKDIR(bdir) != 0 && errno != EEXIST) return 1;
    for (k = 0; k < 7; k++) {
        path_of(src, sizeof src, files[k]);
        snprintf(dst, sizeof dst, "%s/%s", bdir, files[k]);
        if (copy_file(src, dst) != 0) return 1;
    }

    /* are the stored marks exactly the generated demonstration marks?  (then they may be adjusted) */
    memcpy(loaded, marks, sizeof(MarkRec) * (size_t)n_marks);
    loaded_n = n_marks;
    seed_marks();
    if (n_marks != loaded_n) pristine = 0;
    for (k = 0; pristine && k < loaded_n; k++)
        if (strcmp(marks[k].roll, loaded[k].roll) != 0 || strcmp(marks[k].subject, loaded[k].subject) != 0 ||
            marks[k].exam != loaded[k].exam || fabs(marks[k].marks - loaded[k].marks) > 1e-9) pristine = 0;
    if (!pristine) { memcpy(marks, loaded, sizeof(MarkRec) * (size_t)loaded_n); n_marks = loaded_n; }   /* keep the teacher's marks */

    apply_v3_demo(pristine, &blood_n, &fixed_n);
    settings.data_version = 3;

    snprintf(migration_note, sizeof migration_note,
             "Demonstration data was updated:\n- blood groups assigned to %d students\n- %s\n"
             "Your previous data files were copied to:\n%s",
             blood_n, pristine ? "marks of the students closest to passing were improved (class pass rate now realistic)"
                               : "your edited marks were NOT changed (only blood groups were added)", bdir);
    path_of(report, sizeof report, "migration_report.txt");
    fp = fopen(report, "a");
    if (fp != NULL) {
        fprintf(fp, "\nData update to version 3 (%s)\nBackup folder: %s\nBlood groups assigned: %d\nStudents whose marks were improved: %d (%s)\n",
                stamp, bdir, blood_n, fixed_n, pristine ? "marks were the original demonstration marks" : "marks had been edited: skipped");
        fclose(fp);
    }
    return 0;
}

/*
 * Data version 3 -> 4:
 *  - examinations renamed: Unit Test 1 -> CIE 1, Unit Test 2 -> CIE 2, Final Examination -> SEE
 *  - roll numbers 0262001 ... 0262055 become 990000000056 ... 990000000110 (students, marks, attendance)
 * Only rolls of the form 0262NNN are converted (NNN = 1..999 -> ROLL_BASE + NNN); any other roll stays.
 * The old files are copied to a backup folder first.  Returns 1 when the backup failed.
 */
static int roll_candidate(const char *old, char *out, size_t cap)
{
    int i;
    if (strlen(old) != 7 || strncmp(old, "0262", 4) != 0) return 0;
    for (i = 4; i < 7; i++) if (!isdigit((unsigned char)old[i])) return 0;
    if (atoi(old + 4) < 1) return 0;
    snprintf(out, cap, "%lld", ROLL_BASE + atoi(old + 4));
    return 1;
}

static int migrate_v3_to_v4(int backup)
{
    static const char *files[7] = { "teachers.csv", "students.csv", "subjects.csv", "exams.csv",
                                    "marks.csv", "attendance.csv", "settings.csv" };
    static const char *OLD_EXAM[3] = { "Unit Test 1", "Unit Test 2", "Final Examination" };
    static const char *NEW_EXAM[3] = { "CIE 1", "CIE 2", "SEE" };
    char stamp[24], bdir[320], src[320], dst[360], report[360], nr[24];
    int k, e, conv = 0, ren = 0;
    time_t t = time(NULL);
    FILE *fp;

    strftime(stamp, sizeof stamp, "%Y%m%d_%H%M%S", localtime(&t));
    snprintf(bdir, sizeof bdir, "%s_backup_v3_%s", data_dir, stamp);
    if (backup) {                                   /* an earlier step of the same run has already copied the old files */
        if (MKDIR(bdir) != 0 && errno != EEXIST) return 1;
        for (k = 0; k < 7; k++) {
            path_of(src, sizeof src, files[k]);
            snprintf(dst, sizeof dst, "%s/%s", bdir, files[k]);
            if (copy_file(src, dst) != 0) return 1;
        }
    }
    for (e = 0; e < n_exams; e++)
        for (k = 0; k < 3; k++)
            if (strcmp(exams[e].name, OLD_EXAM[k]) == 0) { copy_to(exams[e].name, sizeof exams[e].name, NEW_EXAM[k]); ren++; }
    for (k = 0; k < n_students; k++)                                  /* students first (unless the new roll is already taken) */
        if (roll_candidate(students[k].roll, nr, sizeof nr) && student_find(nr) < 0) {
            copy_to(students[k].roll, sizeof students[k].roll, nr);
            conv++;
        }
    for (k = 0; k < n_marks; k++)                                     /* then every record of a converted student */
        if (roll_candidate(marks[k].roll, nr, sizeof nr) && student_find(marks[k].roll) < 0 && student_find(nr) >= 0)
            copy_to(marks[k].roll, sizeof marks[k].roll, nr);
    for (k = 0; k < n_atts; k++)
        if (roll_candidate(atts[k].roll, nr, sizeof nr) && student_find(atts[k].roll) < 0 && student_find(nr) >= 0)
            copy_to(atts[k].roll, sizeof atts[k].roll, nr);
    settings.data_version = DATA_VERSION;

    {
        size_t used = strlen(migration_note);
        snprintf(migration_note + used, sizeof migration_note - used,
                 "%sData updated: examinations are now CIE 1, CIE 2 and SEE; %d roll numbers were changed to the 990000000xxx format.%s%s",
                 used ? "\n\n" : "", conv, backup ? "\nYour previous data files were copied to:\n" : "", backup ? bdir : "");
    }
    path_of(report, sizeof report, "migration_report.txt");
    fp = fopen(report, "a");
    if (fp != NULL) {
        fprintf(fp, "\nData update to version 4 (%s)\nBackup folder: %s\nExaminations renamed: %d\nRoll numbers changed: %d (0262NNN -> %lld + NNN)\n",
                stamp, bdir, ren, conv, ROLL_BASE);
        fclose(fp);
    }
    return 0;
}

/* ================= load / save ================= */

int storage_load_all(void)
{
    int had_students, had_teachers;
    n_students = n_subjects = n_exams = n_marks = n_atts = n_teachers = 0;
    migration_note[0] = '\0';
    {   /* start from the default settings so values of a previous load never leak in */
        Settings d = { 75.0, 70.0, 30.0, { 90, 80, 70, 60, 50, 40 }, 0, 1 };
        settings = d;
    }
    MKDIR(data_dir);

    read_csv("settings.csv", 2, h_setting);
    had_teachers = read_csv("teachers.csv", 9, h_teacher);
    had_students = read_csv("students.csv", 5, h_student);
    read_csv("subjects.csv", 5, h_subject);
    read_csv("exams.csv", 1, h_exam);
    read_csv("marks.csv", 4, h_mark);
    read_csv("attendance.csv", 5, h_att);

    if (!had_students && !had_teachers) {              /* first run: install demonstration data */
        seed_demo_data();
        return storage_save_all();
    }
    if (settings.data_version < DATA_VERSION) {        /* update the files step by step (each step makes its own backup), then save once */
        int v0 = settings.data_version;                /* an earlier step of this run has already made a backup */
        if (settings.data_version < 2 && migrate_v1_to_v2() != 0) return 2;    /* very old files: new academic structure */
        if (settings.data_version < 3 && migrate_v2_to_v3() != 0) return 2;    /* blood groups + demo pass rate */
        if (settings.data_version < 4 && migrate_v3_to_v4(v0 >= 3) != 0) return 2;    /* CIE/SEE names + roll number format */
        return storage_save_all();
    }
    if (n_teachers == 0) {                             /* never leave the app without a login */
        seed_teacher();
        return storage_save_all();
    }
    return 0;
}

int storage_save_all(void)
{
    char path[300];
    FILE *fp;
    int i, u;

    MKDIR(data_dir);

    path_of(path, sizeof path, "teachers.csv");
    if ((fp = fopen(path, "w")) == NULL) return 1;
    fprintf(fp, "#username,password,name,faculty_id,department,email,class,semester,year,subject,subject_code,institution\n");
    for (i = 0; i < n_teachers; i++)
        fprintf(fp, "%s,%s,%s,%s,%s,%s,%s,%d,%s,%s,%s,%s\n", teachers[i].username, teachers[i].password,
                teachers[i].name, teachers[i].faculty_id, teachers[i].dept, teachers[i].email,
                teachers[i].cls, teachers[i].semester, teachers[i].year, teachers[i].subject,
                teachers[i].subject_code, teachers[i].institution);
    fclose(fp);

    path_of(path, sizeof path, "students.csv");
    if ((fp = fopen(path, "w")) == NULL) return 1;
    fprintf(fp, "#roll,name,branch,section,semester,blood_group (optional)\n");
    for (i = 0; i < n_students; i++)
        fprintf(fp, "%s,%s,%s,%s,%d,%s\n", students[i].roll, students[i].name,
                students[i].branch, students[i].section, students[i].semester, students[i].blood);
    fclose(fp);

    path_of(path, sizeof path, "subjects.csv");
    if ((fp = fopen(path, "w")) == NULL) return 1;
    fprintf(fp, "#code,name,max_marks,pass_marks,credits,short_name,faculty,faculty_email,type,unit1,unit2,unit3,unit4,unit5\n");
    for (i = 0; i < n_subjects; i++) {
        fprintf(fp, "%s,%s,%.1f,%.1f,%d,%s,%s,%s,%s", subjects[i].code, subjects[i].name,
                subjects[i].max_marks, subjects[i].pass_marks, subjects[i].credits,
                subjects[i].short_name, subjects[i].faculty, subjects[i].email, subjects[i].type);
        for (u = 0; u < 5; u++) {
            char tmp[72], *c;
            copy_to(tmp, sizeof tmp, subjects[i].units[u]);
            while ((c = strchr(tmp, ',')) != NULL) *c = '~';          /* commas would split the CSV line */
            fprintf(fp, ",%s", tmp);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);

    path_of(path, sizeof path, "exams.csv");
    if ((fp = fopen(path, "w")) == NULL) return 1;
    fprintf(fp, "#exam_name,max_marks (line order = exam index)\n");
    for (i = 0; i < n_exams; i++) fprintf(fp, "%s,%.1f\n", exams[i].name, exams[i].max_marks);
    fclose(fp);

    path_of(path, sizeof path, "marks.csv");
    if ((fp = fopen(path, "w")) == NULL) return 1;
    fprintf(fp, "#roll,subject_code,exam_index,marks\n");
    for (i = 0; i < n_marks; i++)
        fprintf(fp, "%s,%s,%d,%.2f\n", marks[i].roll, marks[i].subject, marks[i].exam, marks[i].marks);
    fclose(fp);

    path_of(path, sizeof path, "attendance.csv");
    if ((fp = fopen(path, "w")) == NULL) return 1;
    fprintf(fp, "#roll,subject_code,date,session,present\n");
    for (i = 0; i < n_atts; i++)
        fprintf(fp, "%s,%s,%s,%d,%d\n", atts[i].roll, atts[i].subject, atts[i].date,
                atts[i].session, atts[i].present);
    fclose(fp);

    path_of(path, sizeof path, "settings.csv");
    if ((fp = fopen(path, "w")) == NULL) return 1;
    fprintf(fp, "#key,value (grade_min_0..5 = minimum percentage for grades S, A, B, C, D, E)\n");
    fprintf(fp, "data_version,%d\natt_threshold,%.2f\nweight_x,%.2f\nweight_y,%.2f\n",
            settings.data_version, settings.att_threshold, settings.weight_x, settings.weight_y);
    for (i = 0; i < 6; i++) fprintf(fp, "grade_min_%d,%.1f\n", i, settings.grade_min[i]);
    fprintf(fp, "animations,%d\n", settings.animations);
    fclose(fp);

    return 0;
}
