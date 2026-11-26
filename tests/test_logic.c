/*
 * test_logic.c : console tests for the non-GUI modules (known-answer checks).
 * Build & run with:  make test
 */
#include "../common.h"
#include <dirent.h>

static int fails = 0, checks = 0;
#define CHECK(cond, msg) do { checks++; if (!(cond)) { fails++; printf("FAIL: %s\n", msg); } } while (0)
#define NEAR(a, b) (fabs((a) - (b)) < 1e-6)

static Matrix MA, MB, MT, MS;       /* static: a Matrix is 320 KB */
static Report REP;

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (f) { fputs(text, f); fclose(f); }
}

/* sets the three exam marks of one subject so that the subject total is as given */
static int set_total(const char *roll, const char *code, double ut1, double ut2, double fin)
{
    char err[160];
    return marks_set(roll, code, 0, ut1, err) && marks_set(roll, code, 1, ut2, err) && marks_set(roll, code, 2, fin, err);
}

static int count_dirs_with_prefix(const char *prefix)
{
    DIR *d = opendir(".");
    struct dirent *e;
    int n = 0;
    if (!d) return 0;
    while ((e = readdir(d)) != NULL)
        if (strncmp(e->d_name, prefix, strlen(prefix)) == 0) n++;
    closedir(d);
    return n;
}

static int count_backup_dirs(void) { return count_dirs_with_prefix("testdata_old_backup_v1_"); }

int main(void)
{
    char err[160], prob[160];
    Student s;
    Subject sub;
    int i, j, k, idx, c, a, order[MAX_STUDENTS], stu[MAX_STUDENTS], ad, sk, cat[4] = { 0, 0, 0, 0 };
    double pct[MAX_STUDENTS], w[2] = { 0.5, 0.5 }, out[MAT_MAX], tot, mx, sum0 = 0, att0 = 0;

    /* =============== fresh install: demonstration data =============== */
    storage_set_dir("testdata");
    CHECK(storage_load_all() == 0, "first load seeds demo data");
    CHECK(n_students == 55, "exactly 55 students");
    CHECK(n_subjects == 6, "exactly 6 subjects");
    CHECK(total_credits() == 14, "total credits 14");
    CHECK(total_units() == 30, "30 syllabus units");
    CHECK(n_exams == 3 && NEAR(exams[0].max_marks, 20) && NEAR(exams[1].max_marks, 20) && NEAR(exams[2].max_marks, 60), "exam maximums 20/20/60");
    CHECK(strcmp(exams[2].name, "SEE") == 0 && strcmp(exams[0].name, "CIE 1") == 0, "exam names (no Mid Term)");
    CHECK(n_marks == 990, "990 marks records");
    for (i = 0, sum0 = 0; i < n_marks; i++) sum0 += marks[i].marks * (1 + marks[i].exam) + (double)i * 1e-3;   /* checksum of the demo marks */
    for (i = 0, att0 = 0; i < n_atts; i++) att0 += atts[i].present * (i % 97 + 1);                     /* checksum of attendance */

    /* demonstration class: 43 of 55 pass every subject (78.2 %), blood groups for everybody, topper unchanged */
    {
        int passed = 0, k2, seen[8] = { 0 }, pos = 0, neg = 0, allset = 1, ord2[MAX_STUDENTS];
        double p2[MAX_STUDENTS];
        for (i = 0; i < n_students; i++) {
            if (student_exam_pass(students[i].roll, EXAM_ALL)) passed++;
            if (!blood_group_valid(students[i].blood) || students[i].blood[0] == '\0') allset = 0;
            for (k2 = 0; k2 < 8; k2++) if (strcmp(students[i].blood, BLOOD_GROUPS[k2]) == 0) { seen[k2]++; if (strchr(students[i].blood, '+')) pos++; else neg++; }
        }
        CHECK(passed == 43 && n_students - passed == 12, "exactly 43 students pass and 12 fail");
        CHECK(NEAR(class_pass_pct(EXAM_ALL), 43 * 100.0 / 55) && fabs(class_pass_pct(EXAM_ALL) - 78.2) < 0.05, "class pass percentage is 78.2 % (calculated)");
        CHECK(allset, "all 55 students have a blood group");
        for (k2 = 0, c = 1; k2 < 8; k2++) if (seen[k2] == 0) c = 0;
        CHECK(c, "all eight blood groups are represented");
        CHECK(pos > neg * 2 && pos + neg == 55, "positive blood groups are more common than negative ones");
        exam_ranking(EXAM_ALL, ord2, p2);
        CHECK(strcmp(students[ord2[0]].name, "Shreya Iyer") == 0 && NEAR(p2[0], 94.25), "class topper is unchanged");
        {   /* remaining failures: some students fail one subject, some several; improved marks are varied, not all exactly 40 */
            int one = 0, multi = 0, exact40 = 0, near40 = 0;
            for (i = 0; i < n_students; i++) {
                int f = 0;
                for (j = 0; j < n_subjects; j++) {
                    double t2 = 0;
                    subject_total(students[i].roll, j, &t2, NULL);
                    if (!subject_pass(students[i].roll, j)) f++;
                    else if (t2 < 47) { near40++; if (NEAR(t2, 40)) exact40++; }
                }
                if (f == 1) one++; else if (f > 1) multi++;
            }
            CHECK(one > 0 && multi > 0, "failures remain for one subject and for several subjects");
            CHECK(exact40 == 0 || exact40 < near40, "improved marks are not all exactly 40");
        }
    }
    CHECK(n_students * n_subjects == 330, "330 student-subject combinations");
    CHECK(n_teachers == 1 && strcmp(teachers[0].name, "Ms. PPS Faculty") == 0 && strcmp(teachers[0].email, "pps.faculty@example.edu") == 0, "teacher account updated");
    CHECK(strcmp(teachers[0].username, "teacher") == 0 && strcmp(teachers[0].password, "class123") == 0, "login credentials preserved");
    CHECK(strstr(teachers[0].institution, "Demo Institute") != NULL && strcmp(teachers[0].subject_code, "U26CS101") == 0, "teacher subject and institution");

    {
        static const char *code[6] = { "U26MA101", "U26CH101", "U26CS101", "U26EN101", "U26EN103", "U26CH102" };
        static const char *name[6] = { "Mathematics-I", "Engineering Chemistry", "Programming for Problem Solving",
                                       "English for Professional Communication", "Universal Human Values", "Environmental Science" };
        static const char *fac[6] = { "Dr. Math Faculty", "Dr. Chemistry Faculty", "Ms. PPS Faculty", "Ms. English Faculty", "Ms. UHV Faculty", "Dr. EVS Faculty" };
        static const char *mail[6] = { "math.faculty@example.edu", "chemistry.faculty@example.edu", "pps.faculty@example.edu",
                                       "english.faculty@example.edu", "uhv.faculty@example.edu", "evs.faculty@example.edu" };
        static const int cr[6] = { 4, 3, 3, 3, 1, 0 };
        int ok = 1;
        for (i = 0; i < 6; i++)
            if (strcmp(subjects[i].code, code[i]) || strcmp(subjects[i].name, name[i]) || strcmp(subjects[i].faculty, fac[i]) ||
                strcmp(subjects[i].email, mail[i]) || subjects[i].credits != cr[i] || !NEAR(subjects[i].max_marks, 100) ||
                strcmp(subjects[i].type, "Theory") != 0) ok = 0;
        CHECK(ok, "subject codes, names, faculty, emails, credits");
        CHECK(subject_find("MA101") < 0 && subject_find("PH103") < 0, "old dummy subjects gone");
        CHECK(strcmp(subjects[2].units[0], "Introduction to Programming and C Fundamentals") == 0 &&
              strcmp(subjects[2].units[2], "Recursion, Preprocessor and Arrays") == 0 &&
              strcmp(subjects[5].units[4], "Watershed Management & Environmental Ethics") == 0, "syllabus units (with commas) stored");
        CHECK(subjects[5].credits == 0, "Environmental Science has zero credits");
    }

    /* unique rolls and names; leading zero kept; every student has 6 x 3 marks and valid maximums */
    {
        int dup = 0, full = 1, inrange = 1;
        for (i = 0; i < n_students; i++) {
            for (j = i + 1; j < n_students; j++)
                if (strcmp(students[i].roll, students[j].roll) == 0 || strcmp(students[i].name, students[j].name) == 0) dup = 1;
            for (j = 0; j < n_subjects; j++)
                for (k = 0; k < 3; k++) if (marks_find(students[i].roll, subjects[j].code, k) < 0) full = 0;
        }
        for (i = 0; i < n_marks; i++)
            if (marks[i].marks < 0 || marks[i].marks > exams[marks[i].exam].max_marks) inrange = 0;
        CHECK(!dup, "no duplicate roll numbers or names");
        CHECK(full, "every student has marks for every subject and exam");
        CHECK(inrange, "all marks within exam maximums");
        CHECK(strcmp(students[0].roll, "990000000056") == 0 && strcmp(students[0].name, "Aarav Mehta") == 0, "original student identity and roll format kept");
        CHECK(strcmp(students[54].roll, "990000000110") == 0, "roll numbers continue the existing format");
        student_exam_pct(students[0].roll, EXAM_ALL, &tot, &mx);
        CHECK(NEAR(mx, 600), "maximum overall marks 600");
        CHECK(NEAR(exam_subject_max(0, 0) + exam_subject_max(1, 0) + exam_subject_max(2, 0), 100), "subject maximum 100");
    }

    /* distribution of performance */
    {
        int fail1 = 0, none = 0, gradeseen[7] = { 0 }, top = 0, low = 0;
        static const char *gl[7] = { "S", "A", "B", "C", "D", "E", "F" };
        for (i = 0; i < n_students; i++) {
            if (student_exam_pass(students[i].roll, EXAM_ALL)) none++; else fail1++;
            for (j = 0; j < n_subjects; j++)
                for (k = 0; k < 7; k++) if (strcmp(grade_of(subject_pct(students[i].roll, j)), gl[k]) == 0) gradeseen[k]++;
            if (student_exam_pct(students[i].roll, EXAM_ALL, NULL, NULL) >= 90) top++;
            if (student_exam_pct(students[i].roll, EXAM_ALL, NULL, NULL) < 50) low++;
        }
        CHECK(fail1 >= 5 && none >= 25, "some students fail subjects, many pass everything");
        CHECK(top >= 1 && low >= 5, "outstanding and below-average students exist");
        for (k = 0, c = 0; k < 7; k++) if (gradeseen[k] > 0) c++;
        CHECK(c == 7, "every grade O..F occurs in the demo data");
    }

    /* =============== grades, grade points, SGPA (known answers) =============== */
    CHECK(strcmp(grade_of(100), "S") == 0 && strcmp(grade_of(90), "S") == 0 && strcmp(grade_of(89.5), "A") == 0 &&
          strcmp(grade_of(89.99), "A") == 0 && strcmp(grade_of(80), "A") == 0 && strcmp(grade_of(79.5), "B") == 0 &&
          strcmp(grade_of(70), "B") == 0 && strcmp(grade_of(69.5), "C") == 0 && strcmp(grade_of(60), "C") == 0 &&
          strcmp(grade_of(59.5), "D") == 0 && strcmp(grade_of(50), "D") == 0 && strcmp(grade_of(49.5), "E") == 0 &&
          strcmp(grade_of(40), "E") == 0 && strcmp(grade_of(39.5), "F") == 0 && strcmp(grade_of(0), "F") == 0, "grade boundaries S..F incl. decimals");
    CHECK(NEAR(grade_points_of(95), 10) && NEAR(grade_points_of(85), 9) && NEAR(grade_points_of(75), 8) && NEAR(grade_points_of(65), 7) &&
          NEAR(grade_points_of(55), 6) && NEAR(grade_points_of(45), 5) && NEAR(grade_points_of(39.5), 0) && NEAR(grade_points_of(89.5), 9) &&
          NEAR(grade_points_of(90), 10), "grade points 10/9/8/7/6/5/0");
    CHECK(strcmp(grade_performance("S"), "Outstanding") == 0 && strcmp(grade_performance("A"), "Excellent") == 0 &&
          strcmp(grade_performance("B"), "Very Good") == 0 && strcmp(grade_performance("C"), "Good") == 0 &&
          strcmp(grade_performance("D"), "Average") == 0 && strcmp(grade_performance("E"), "Pass") == 0 &&
          strcmp(grade_performance("F"), "Fail") == 0, "grade performance names");
    {
        char leg[300];
        grade_legend(leg, sizeof leg);
        CHECK(strstr(leg, "S 90-100 (10)") && strstr(leg, "A 80-<90 (9)") && strstr(leg, "E 40-<50 (5)") && strstr(leg, "F below 40") &&
              strstr(leg, "O ") == NULL && strstr(leg, "A+") == NULL && strstr(leg, "B+") == NULL, "grade legend shows only S..F");
        for (i = 0; i < 7; i++) CHECK(strlen(grade_name(i)) == 1 && strchr("SABCDEF", grade_name(i)[0]), "grade names are S..F");
    }
    memset(&s, 0, sizeof s);
    strcpy(s.name, "Known Answer"); strcpy(s.roll, "0262900"); strcpy(s.branch, "CSM"); strcpy(s.section, "B"); s.semester = 1;
    CHECK(student_add(&s, err), "add student");
    /* totals 85, 72, 95, 55, 45 and EVS 10 */
    CHECK(set_total("0262900", "U26MA101", 17, 17, 51) && set_total("0262900", "U26CH101", 14, 14, 44) &&
          set_total("0262900", "U26CS101", 19, 19, 57) && set_total("0262900", "U26EN101", 11, 11, 33) &&
          set_total("0262900", "U26EN103", 9, 9, 27) && set_total("0262900", "U26CH102", 2, 2, 6), "known marks entered");
    CHECK(NEAR(student_sgpa("0262900", &k, &c), (4 * 9 + 3 * 8 + 3 * 10 + 3 * 6 + 1 * 5) / 14.0) && c == 14 && k == 14,
          "SGPA = 113/14 = 8.07, EVS excluded");
    CHECK(NEAR(student_exam_pct("0262900", EXAM_ALL, &tot, &mx), (85 + 72 + 95 + 55 + 45 + 10) * 100.0 / 600) && NEAR(tot, 362) && NEAR(mx, 600),
          "overall percentage 362/600");
    CHECK(!student_exam_pass("0262900", EXAM_ALL) && subject_pass("0262900", 4) && !subject_pass("0262900", 5), "EVS fail makes result fail, UHV passes");
    CHECK(set_total("0262900", "U26CS101", 5, 5, 15), "PPS lowered to 25 (fail)");
    CHECK(NEAR(student_sgpa("0262900", &k, &c), (4 * 9 + 3 * 8 + 3 * 6 + 1 * 5) / 14.0) && k == 11, "failed credit subject gives 0 points and no earned credits");
    CHECK(NEAR(subject_pct("0262900", 0), 85) && NEAR(student_sgpa("0262899", NULL, NULL), -1), "subject percentage; no-data SGPA is -1");
    CHECK(NEAR(class_avg_sgpa(), class_avg_sgpa()) && class_avg_sgpa() > 0 && class_avg_sgpa() <= 10, "class average SGPA in range");

    /* marks validation */
    CHECK(!marks_set("0262900", "U26MA101", 0, 21, err), "CIE 1 above 20 rejected");
    CHECK(!marks_set("0262900", "U26MA101", 1, 20.5, err), "CIE 2 above 20 rejected");
    CHECK(!marks_set("0262900", "U26MA101", 2, 61, err), "SEE above 60 rejected");
    CHECK(!marks_set("0262900", "U26MA101", 2, -1, err), "negative marks rejected");
    CHECK(marks_set("0262900", "U26MA101", 2, 60, err) && marks_set("0262900", "U26MA101", 0, 0, err), "boundary marks accepted");
    c = n_marks;
    CHECK(marks_set("0262900", "U26MA101", 2, 51, err) && n_marks == c, "edit does not duplicate");
    CHECK(!marks_set("0262999", "U26MA101", 0, 5, err) && !marks_set("0262900", "NOPE", 0, 5, err) && !marks_set("0262900", "U26MA101", 9, 5, err), "unknown student/subject/exam rejected");
    student_delete(student_find("0262900"));
    CHECK(n_students == 55 && n_marks == 990, "student removed with all records");

    /* =============== consolidated marks sheet (one row per student) =============== */
    {
        SheetRow sr;
        int rows = 0, subj_fail_students = 0, k2;
        double expect, mk;
        for (i = 0; i < n_students; i++) if (marks_filter_match(students[i].roll, -1, -1, 0)) rows++;
        CHECK(rows == 55, "default marks sheet has 55 student rows (not 330)");
        marks_sheet_row(students[0].roll, EXAM_ALL, &sr);
        subject_total(students[0].roll, 0, &tot, NULL);
        CHECK(NEAR(sr.max, 600) && sr.any && NEAR(sr.cell[0], tot), "overall sheet: maximum 600, subject cell = subject total");
        for (j = 0, expect = 0; j < n_subjects; j++) { subject_total(students[0].roll, j, &tot, NULL); expect += tot; }
        CHECK(NEAR(sr.total, expect) && NEAR(sr.pct, expect * 100 / 600.0), "overall sheet: total and percentage");
        CHECK(NEAR(sr.sgpa, student_sgpa(students[0].roll, &k2, NULL)) && sr.earned == k2, "sheet SGPA is the overall SGPA");
        for (k = 0; k < 3; k++) {
            marks_sheet_row(students[0].roll, k, &sr);
            c = marks_find(students[0].roll, subjects[2].code, k);
            for (j = 0, expect = 0; j < n_subjects; j++) { mk = marks[marks_find(students[0].roll, subjects[j].code, k)].marks; expect += mk; }
            CHECK(NEAR(sr.max, k == 2 ? 360 : 120) && NEAR(sr.cell[2], marks[c].marks) && NEAR(sr.total, expect) &&
                  NEAR(sr.pct, expect * 100 / sr.max), "examination filter: CIE 1/CIE 2 max 120, SEE max 360, cells and totals");
            CHECK(NEAR(sr.sgpa, student_sgpa(students[0].roll, NULL, NULL)) && sr.pass == student_exam_pass(students[0].roll, EXAM_ALL),
                  "SGPA and result stay OVERALL whatever the examination filter");
        }
        /* filters */
        for (i = 0; i < n_students; i++) if (!student_exam_pass(students[i].roll, EXAM_ALL)) subj_fail_students++;
        for (c = 0, i = 0; i < n_students; i++) c += marks_filter_match(students[i].roll, -1, -1, 2);
        CHECK(c == subj_fail_students && c > 0, "Result filter Fail (all subjects) = overall failures");
        for (c = 0, i = 0; i < n_students; i++) c += marks_filter_match(students[i].roll, -1, -1, 1);
        CHECK(c == 55 - subj_fail_students, "Result filter Pass (all subjects) = overall passes");
        for (k = 0; k < 7; k++) {
            int cnt_any = 0, cnt_subj = 0;
            for (i = 0; i < n_students; i++) {
                int has = 0;
                for (j = 0; j < n_subjects; j++) if (grade_index(subject_pct(students[i].roll, j)) == k) has = 1;
                cnt_any += has;
                if (grade_index(subject_pct(students[i].roll, 2)) == k) cnt_subj++;
                CHECK(marks_filter_match(students[i].roll, -1, k, 0) == has, "grade filter (all subjects) = at least one subject");
                CHECK(marks_filter_match(students[i].roll, 2, k, 0) == (grade_index(subject_pct(students[i].roll, 2)) == k), "grade filter with a subject = that subject's grade");
            }
            CHECK(cnt_any >= cnt_subj, "all-subject grade filter never smaller than a single subject");
        }
        for (c = 0, i = 0; i < n_students; i++) c += marks_filter_match(students[i].roll, 5, -1, 2);
        for (k2 = 0, i = 0; i < n_students; i++) k2 += (subject_pct(students[i].roll, 5) >= 0 && !subject_pass(students[i].roll, 5));
        CHECK(c == k2, "Result filter with a subject = that subject's result");

        /* failed-subject count, per-subject flags and the examination filter (failure uses the COMPLETE total) */
        {
            int ok = 1, with_fail = 0, mismatch = 0;
            for (i = 0; i < n_students; i++) {
                int cf = 0, cp = 0;
                SheetRow a0, a1, a2, a3;
                marks_sheet_row(students[i].roll, EXAM_ALL, &a0);
                marks_sheet_row(students[i].roll, 0, &a1);
                marks_sheet_row(students[i].roll, 1, &a2);
                marks_sheet_row(students[i].roll, 2, &a3);
                for (j = 0; j < n_subjects; j++) {
                    int f = subject_pct(students[i].roll, j) >= 0 && !subject_pass(students[i].roll, j);
                    cf += f; cp += !f;
                    if (a0.fail[j] != f || a1.fail[j] != f || a2.fail[j] != f || a3.fail[j] != f) mismatch = 1;
                    if (a0.fail[j] && !NEAR(a0.subtotal[j], a1.cell[j] + a2.cell[j] + a3.cell[j])) ok = 0;
                    if (a0.fail[j] && a0.subtotal[j] >= 40) ok = 0;
                    if (!a0.fail[j] && a0.has[j] && a0.subtotal[j] < 40) ok = 0;
                }
                if (a0.failed != cf || a0.passed != cp || cf + cp != 6) ok = 0;
                if ((a0.failed == 0) != a0.pass) ok = 0;
                if (a0.failed > 0) with_fail++;
            }
            CHECK(ok, "failed/passed subject counts and subtotals are correct");
            CHECK(!mismatch, "failed-subject flags are identical for Overall, CIE 1, CIE 2 and SEE (judged on the total)");
            CHECK(with_fail == subj_fail_students, "students with failed subjects = overall Fail students");
        }
    }

    /* a student with a high aggregate but one failed subject is overall FAIL; failed credit subject = 0 points, 0 credits */
    memset(&s, 0, sizeof s);
    strcpy(s.name, "High Aggregate"); strcpy(s.roll, "0262901"); strcpy(s.branch, "CSM"); strcpy(s.section, "B"); s.semester = 1;
    CHECK(student_add(&s, err), "add student for the fail rule");
    CHECK(set_total("0262901", "U26MA101", 19, 19, 57) && set_total("0262901", "U26CH101", 19, 19, 57) &&
          set_total("0262901", "U26CS101", 19, 19, 57) && set_total("0262901", "U26EN101", 19, 19, 57) &&
          set_total("0262901", "U26EN103", 19, 19, 57) && set_total("0262901", "U26CH102", 19, 19, 57), "six subjects at 95");
    CHECK(student_exam_pass("0262901", EXAM_ALL) && NEAR(student_sgpa("0262901", &k, &c), 10.0) && k == 14 && c == 14, "all 95: SGPA 10, 14 credits earned");
    CHECK(set_total("0262901", "U26EN103", 3, 3, 9), "UHV (1 credit) lowered to 15");
    {
        double p = student_exam_pct("0262901", EXAM_ALL, NULL, NULL);
        CHECK(p > 80 && !student_exam_pass("0262901", EXAM_ALL), "aggregate above 80% but overall Fail because one subject failed");
        CHECK(NEAR(student_sgpa("0262901", &k, &c), (13 * 10.0) / 14.0) && k == 13 && c == 14, "failed credit subject: 0 points, 0 credits, denominator stays 14");
    }
    CHECK(set_total("0262901", "U26EN103", 19, 19, 57) && set_total("0262901", "U26CH102", 3, 3, 9), "EVS (0 credits) lowered to 15");
    CHECK(!student_exam_pass("0262901", EXAM_ALL) && NEAR(student_sgpa("0262901", &k, &c), 10.0) && k == 14, "failed EVS: overall Fail but SGPA unchanged");

    /* =============== editing: atomic validation, decimals, blank removes =============== */
    {
        double vals[MAX_SUBJECTS * 3] = { 0 };
        int given[MAX_SUBJECTS * 3] = { 0 }, before = n_marks;
        double snapshot[3], v;
        static MarkRec copy[MAX_MARKS];
        memcpy(copy, marks, sizeof(MarkRec) * n_marks);
        for (j = 0; j < n_subjects; j++) for (k = 0; k < 3; k++) { vals[j * 3 + k] = 10; given[j * 3 + k] = 1; }
        vals[4 * 3 + 2] = 61;                               /* one invalid value: Final above 60 */
        CHECK(!marks_save_student("0262901", vals, given, err) && strstr(err, "UHV") != NULL, "invalid entry rejected with the subject named");
        CHECK(n_marks == before && memcmp(copy, marks, sizeof(MarkRec) * n_marks) == 0, "nothing is partially saved");
        vals[4 * 3 + 2] = 30; vals[0 * 3 + 0] = -1;
        CHECK(!marks_save_student("0262901", vals, given, err), "negative marks rejected");
        vals[0 * 3 + 0] = 20.5;
        CHECK(!marks_save_student("0262901", vals, given, err), "CIE 1 above 20 rejected");
        vals[0 * 3 + 0] = 17.5; vals[0 * 3 + 1] = 18.25; vals[0 * 3 + 2] = 55.5;
        CHECK(marks_save_student("0262901", vals, given, err), "valid decimal marks saved");
        snapshot[0] = marks[marks_find("0262901", "U26MA101", 0)].marks;
        snapshot[1] = marks[marks_find("0262901", "U26MA101", 1)].marks;
        snapshot[2] = marks[marks_find("0262901", "U26MA101", 2)].marks;
        CHECK(NEAR(snapshot[0], 17.5) && NEAR(snapshot[1], 18.25) && NEAR(snapshot[2], 55.5), "decimal marks stored exactly");
        subject_total("0262901", 0, &v, NULL);
        CHECK(NEAR(v, 91.25) && strcmp(grade_of(subject_pct("0262901", 0)), "S") == 0, "total 91.25 -> grade S, recalculated automatically");
        CHECK(n_marks == before, "saving existing marks adds no records");
        given[0 * 3 + 1] = 0;                               /* blank UT-2 of M1 removes that mark */
        CHECK(marks_save_student("0262901", vals, given, err) && marks_find("0262901", "U26MA101", 1) < 0 && n_marks == before - 1, "blank box removes the mark");
        student_delete(student_find("0262901"));
        CHECK(n_students == 55 && n_marks == 990, "extra student removed again");
    }

    /* =============== ranking, class statistics =============== */
    CHECK(exam_ranking(EXAM_ALL, order, pct) == 55, "ranking covers all students");
    for (i = 1; i < 55; i++) if (pct[i] > pct[i - 1]) break;
    CHECK(i == 55 && student_rank(students[order[0]].roll, EXAM_ALL) == 1, "ranking sorted descending, top rank 1");
    CHECK(class_avg_pct(EXAM_ALL) > 40 && class_avg_pct(EXAM_ALL) < 90 && class_pass_pct(EXAM_ALL) > 50 && class_pass_pct(EXAM_ALL) < 100, "class statistics plausible");
    CHECK(NEAR(class_avg_pct(0) * 20 * 55 * 6 / 100.0 / (55 * 6), class_avg_pct(0) * 20 / 100.0), "per-exam average consistent");
    CHECK(latest_exam() == 2, "latest exam is the final");
    for (j = 0; j < n_subjects; j++) CHECK(subject_avg_pct(j, EXAM_ALL) > 30 && subject_avg_pct(j, EXAM_ALL) < 95, "subject average in range");

    /* =============== attendance =============== */
    {
        int held[6] = { 40, 30, 30, 30, 16, 16 }, ok = 1;
        for (j = 0; j < 6; j++) {
            att_stats(students[0].roll, subjects[j].code, &c, &a);
            if (c != held[j] || a > c || a < 0) ok = 0;
        }
        CHECK(ok, "classes conducted per subject, attended <= conducted");
        CHECK(n_atts == 55 * (40 + 30 + 30 + 30 + 16 + 16), "attendance record count");
        for (i = 0; i < n_students; i++) {
            double p = att_pct(students[i].roll, NULL);
            cat[att_category(p)]++;
        }
        CHECK(cat[0] > 0 && cat[1] > 0 && cat[2] > 0 && cat[3] > 0, "all four attendance categories occur");
        CHECK(att_category(90) == 0 && att_category(89.9) == 1 && att_category(80) == 1 && att_category(75) == 2 && att_category(74.9) == 3, "category boundaries");
        CHECK(strcmp(att_category_name(3), "Low") == 0, "category names");
    }
    c = n_atts;
    CHECK(att_set(students[0].roll, "U26MA101", "2026-09-01", 1, 1) || 1, "att_set");
    k = n_atts;
    CHECK(att_set(students[0].roll, "U26MA101", "2026-12-24", 1, 0) && n_atts == k + 1, "new session added");
    CHECK(att_set(students[0].roll, "U26MA101", "2026-12-24", 1, 1) && n_atts == k + 1, "no duplicate record for a session");
    att_delete_session("U26MA101", "2026-12-24", 1);
    CHECK(n_atts == c, "session deleted");
    CHECK(valid_date("2026-02-28") && !valid_date("2026-02-30") && !valid_date("26-1-1"), "date validation");
    {
        int low_any = 0;
        for (i = 0; i < n_students; i++) if (att_is_low(students[i].roll)) low_any++;
        CHECK(low_any >= 5 && low_any <= 30, "students below 75% are detected");
    }

    /* =============== one subject per period per day =============== */
    {
        int pr[MAX_STUDENTS], n0 = n_atts, i2, cc, aa, c_before, a_before;
        char own[16], er[200], rep[400];
        for (i2 = 0; i2 < n_students; i2++) pr[i2] = (i2 % 5 == 0) ? 0 : 1;
        CHECK(att_find_conflicts(rep, sizeof rep) == 0, "existing attendance has no conflicting date/period sessions");
        CHECK(!att_period_subject("2026-12-01", 1, own), "unused period has no subject");
        CHECK(att_save_session("U26MA101", "2026-12-01", 1, pr, er) && n_atts == n0 + n_students, "Period 1: Mathematics-I saved");
        CHECK(att_period_subject("2026-12-01", 1, own) && strcmp(own, "U26MA101") == 0, "Period 1 now belongs to Mathematics-I");
        CHECK(att_save_session("U26CS101", "2026-12-01", 2, pr, er) && att_period_subject("2026-12-01", 2, own) && strcmp(own, "U26CS101") == 0,
              "Period 2: PPS saved");
        n0 = n_atts;
        CHECK(!att_save_session("U26CS101", "2026-12-01", 1, pr, er) && strstr(er, "Mathematics-I") != NULL && n_atts == n0,
              "another subject cannot take Period 1 (refused with a message, nothing saved)");
        CHECK(!att_set(students[0].roll, "U26CH101", "2026-12-01", 1, 1) && n_atts == n0, "the data layer refuses a second subject in a period");
        CHECK(att_set(students[0].roll, "U26MA101", "2026-12-01", 1, 1) && n_atts == n0, "the owning subject can still be edited");
        CHECK(att_save_session("U26CH101", "2026-12-02", 1, pr, er) && att_period_subject("2026-12-02", 1, own) && strcmp(own, "U26CH101") == 0,
              "a different date allows a different subject in Period 1");
        n0 = n_atts;
        CHECK(att_save_session("U26MA101", "2026-12-01", 3, pr, er) && n_atts == n0 + n_students, "the same subject may be taught in several periods of a day");
        /* editing a saved session: no duplicates, no extra class */
        att_stats(students[0].roll, "U26MA101", &c_before, &a_before);
        n0 = n_atts;
        pr[0] = 1;                                                  /* student 0 was Absent, corrected to Present */
        CHECK(att_save_session("U26MA101", "2026-12-01", 1, pr, er) && n_atts == n0, "editing a session creates no duplicate records");
        att_stats(students[0].roll, "U26MA101", &cc, &aa);
        CHECK(cc == c_before && aa == a_before + (a_before == aa ? 0 : 1) - (a_before == aa ? 0 : 0), "classes conducted unchanged by an edit");
        CHECK(att_find(students[0].roll, "U26MA101", "2026-12-01", 1) >= 0 && atts[att_find(students[0].roll, "U26MA101", "2026-12-01", 1)].present == 1, "the corrected status is stored");
        /* validation is atomic */
        n0 = n_atts;
        pr[3] = -1;
        CHECK(!att_save_session("U26EN101", "2026-12-01", 4, pr, er) && n_atts == n0, "an unmarked student blocks the whole save");
        pr[3] = 1;
        CHECK(!att_save_session("U26EN101", "2026-13-01", 4, pr, er) && !att_save_session("U26EN101", "2026-12-01", 9, pr, er) &&
              !att_save_session("NOPE", "2026-12-01", 4, pr, er) && n_atts == n0, "invalid date, period or subject saves nothing");
        /* statistics are exact: one more class for every student per saved session */
        att_stats(students[1].roll, "U26MA101", &cc, &aa);
        CHECK(cc == 40 + 2 && aa >= 0 && aa <= cc, "classes conducted count each saved session once");
        /* deleting a session unlocks the period */
        att_delete_session("U26MA101", "2026-12-01", 1);
        CHECK(!att_period_subject("2026-12-01", 1, own), "deleting the session unlocks Period 1");
        CHECK(att_save_session("U26CH101", "2026-12-01", 1, pr, er) && att_period_subject("2026-12-01", 1, own) && strcmp(own, "U26CH101") == 0,
              "after deleting, the correct subject can be recorded");
        att_stats(students[1].roll, "U26MA101", &cc, &aa);
        CHECK(cc == 40 + 1, "the deleted session no longer counts for Mathematics-I");
        /* old conflicting data is reported, never changed */
        n0 = n_atts;
        strcpy(atts[n_atts].roll, students[0].roll); strcpy(atts[n_atts].subject, "U26PH999"); strcpy(atts[n_atts].date, "2026-12-01");
        atts[n_atts].session = 2; atts[n_atts].present = 1; n_atts++;
        CHECK(att_find_conflicts(rep, sizeof rep) == 1 && strstr(rep, "2026-12-01, period 2") != NULL && strstr(rep, "U26PH999") != NULL, "a conflicting old session is reported");
        CHECK(n_atts == n0 + 1, "reporting conflicts deletes nothing");
        n_atts--;
        /* the subject assignment is stored with the attendance and survives a restart */
        CHECK(storage_save_all() == 0, "save sessions");
        n_atts = 0;
        CHECK(storage_load_all() == 0 && att_period_subject("2026-12-02", 1, own) && strcmp(own, "U26CH101") == 0 &&
              att_period_subject("2026-12-01", 2, own) && strcmp(own, "U26CS101") == 0 &&
              !att_save_session("U26MA101", "2026-12-02", 1, pr, er), "saved subject assignments persist and stay locked after a restart");
        /* tidy up the test sessions */
        att_delete_session("U26CH101", "2026-12-02", 1); att_delete_session("U26CH101", "2026-12-01", 1);
        att_delete_session("U26CS101", "2026-12-01", 2); att_delete_session("U26MA101", "2026-12-01", 3);
        CHECK(n_atts == 8910, "original attendance records are intact after the lock tests");
    }

    /* =============== students and subjects =============== */
    memset(&s, 0, sizeof s);
    strcpy(s.name, "Test Person"); strcpy(s.roll, "990000000056"); strcpy(s.branch, "CSM"); strcpy(s.section, "B"); s.semester = 1;
    CHECK(!student_add(&s, err), "duplicate roll rejected");
    strcpy(s.roll, "0262999");
    CHECK(student_add(&s, err) && n_students == 56, "student added; counts follow the data");
    CHECK(student_search("test per", order) == 1 && student_search("2629", order) == 1, "search by name and roll");
    CHECK(exam_ranking(EXAM_ALL, order, pct) == 55, "student without marks is not ranked");
    idx = student_find("0262999");
    strcpy(s.roll, "0262888");
    CHECK(marks_set("0262999", "U26MA101", 0, 15, err) && student_update(idx, &s, err) && marks_find("0262888", "U26MA101", 0) >= 0, "roll rename cascades");
    student_delete(student_find("0262888"));
    CHECK(n_students == 55 && n_marks == 990, "delete removes marks");
    memset(&sub, 0, sizeof sub);
    strcpy(sub.code, "U26MA101"); strcpy(sub.name, "Dup"); sub.max_marks = 100; sub.pass_marks = 40; sub.credits = 3;
    CHECK(!subject_add(&sub, err), "duplicate subject code rejected");
    strcpy(sub.code, "ZZ1"); sub.pass_marks = 200;
    CHECK(!subject_add(&sub, err), "pass > max rejected");
    sub.pass_marks = 40; sub.credits = 0;
    CHECK(subject_add(&sub, err) && subject_find("ZZ1") == 6, "zero-credit subject can be added");
    subject_delete(6);
    CHECK(n_subjects == 6 && n_marks == 990, "subject deleted");

    /* =============== matrices =============== */
    for (i = 0; i < n_students; i++) stu[i] = i;
    mat_marks(&MA, EXAM_ALL, stu, n_students);
    CHECK(MA.rows == 55 && MA.cols == 6, "student-subject totals matrix is 55 x 6");
    subject_total(students[3].roll, 2, &tot, NULL);
    CHECK(NEAR(MA.v[3][2], tot), "matrix entry equals the subject total");
    mat_marks(&MB, 0, stu, n_students);
    CHECK(MB.rows == 55 && MB.cols == 6 && MB.v[0][0] <= 20, "exam matrix entries are out of 20");
    mat_transpose(&MA, &MT);
    CHECK(MT.rows == 6 && MT.cols == 55 && NEAR(MT.v[2][3], MA.v[3][2]), "transpose of 55 x 6 is 6 x 55");
    memset(&MA, 0, sizeof MA); MA.rows = 2; MA.cols = 3;
    MA.v[0][0] = 1; MA.v[0][1] = 2; MA.v[0][2] = 3; MA.v[1][0] = 4; MA.v[1][1] = 5; MA.v[1][2] = 6;
    mat_transpose(&MA, &MT);
    CHECK(MT.rows == 3 && MT.cols == 2 && NEAR(MT.v[2][0], 3) && NEAR(MT.v[1][1], 5), "transpose (known answer)");
    CHECK(mat_add(&MA, &MA, &MS) && NEAR(MS.v[1][2], 12), "addition");
    MB = MA; MB.cols = 2;
    CHECK(!mat_add(&MA, &MB, &MS), "addition of incompatible matrices refused");
    /* mathematical analyzer: matrix subtraction and the attendance predictor f(x) = 100 (A + x) / (T + x) */
    {
        Matrix SA, SB, SD;
        long x = -1;
        memset(&SA, 0, sizeof SA); memset(&SB, 0, sizeof SB);
        SA.rows = SB.rows = 1; SA.cols = SB.cols = 2;
        SA.v[0][0] = 10; SA.v[0][1] = 12; SB.v[0][0] = 14; SB.v[0][1] = 11;
        CHECK(mat_sub(&SB, &SA, &SD) && NEAR(SD.v[0][0], 4) && NEAR(SD.v[0][1], -1), "matrix subtraction D = B - A (known answer)");
        SB.cols = 3;
        CHECK(!mat_sub(&SB, &SA, &SD), "subtraction of incompatible matrices refused");
        CHECK(NEAR(att_projection(30, 50, 0), 60.0) && NEAR(att_projection(30, 50, 30), 75.0), "f(0) = 60 %, f(30) = 75 % for A = 30, T = 50");
        CHECK(att_classes_needed(30, 50, 75, &x) == ATT_NEED && x == 30, "30 of 50 -> 75 % needs exactly 30 more classes");
        CHECK(att_classes_needed(0, 10, 75, &x) == ATT_NEED && x == 30, "0 of 10 -> 75 % needs 30 more classes");
        CHECK(att_classes_needed(7, 10, 75, &x) == ATT_NEED && x == 2, "7 of 10 -> 75 % needs 2 more classes (boundary 9/12)");
        CHECK(att_classes_needed(3, 7, 80, &x) == ATT_NEED && x == 13, "3 of 7 -> 80 % needs 13 more classes (16/20)");
        CHECK(att_classes_needed(60, 100, 90, &x) == ATT_NEED && x == 300 && att_projection(60, 100, 299) < 90, "60 of 100 -> 90 % needs 300 classes, 299 are not enough");
        CHECK(att_classes_needed(40, 50, 75, &x) == ATT_DONE && x == 0, "target already achieved: no extra classes");
        CHECK(att_classes_needed(30, 40, 75, &x) == ATT_DONE, "exactly on the target counts as achieved");
        CHECK(att_classes_needed(10, 20, 100, &x) == ATT_IMPOSSIBLE, "100 % is impossible after missed classes");
        CHECK(att_classes_needed(20, 20, 100, &x) == ATT_DONE, "100 % with no missed class is already achieved");
        CHECK(att_classes_needed(0, 0, 75, &x) == ATT_NODATA && att_projection(0, 0, 0) < 0, "no attendance records is handled");
        CHECK(NEAR(att_slope(30, 50, 0), 0.8) && NEAR(att_slope(30, 50, 30), 0.3125) && NEAR(att_slope(50, 50, 5), 0.0), "derivative f'(x) = 100 (T - A) / (T + x)^2");
    }
    MA.cols = 2;
    mat_vec(&MA, w, out);
    CHECK(NEAR(out[0], 1.5) && NEAR(out[1], 4.5), "matrix x vector");
    CHECK(NEAR(perf_score(80, 90, 70, 30), 83.0), "P(x,y) = 0.7x + 0.3y");
    CHECK(strcmp(trend_label(2.5), "Improving") == 0 && strcmp(trend_label(-3), "Declining") == 0 && strcmp(trend_label(0.5), "Stable") == 0, "trend labels");
    {
        double tp[MAX_EXAMS];
        int te[MAX_EXAMS];
        CHECK(student_trend(students[0].roll, tp, te) == 3, "trend has three examinations");
    }

    /* =============== reports and CSV =============== */
    report_class(&REP, EXAM_ALL);       CHECK(REP.nrows == 55 && REP.ncols == 11, "class report rows");
    report_class(&REP, 0);              CHECK(REP.nrows == 55, "class report for one exam");
    report_student(&REP, students[0].roll); CHECK(REP.nrows == 7 && REP.ncols == 12 && strstr(REP.cell[6][0], "OVERALL") != NULL, "student report: 6 subjects + overall");
    report_set_teacher(0);
    report_subject(&REP, 2, EXAM_ALL);  CHECK(REP.nrows == 55 && strcmp(REP.cell[0][3], "Ms. PPS Faculty") == 0 && strstr(REP.prepared, "Demo Institute") != NULL, "subject report with faculty");
    report_exam(&REP, 2);               CHECK(REP.nrows == 55 && REP.ncols == 10, "examination-wise report");
    report_attendance(&REP);            CHECK(REP.nrows == 55 && REP.ncols == 10, "attendance report");
    c = REP.nrows;
    report_low_attendance(&REP);        CHECK(REP.nrows > 0 && REP.nrows < c, "low-attendance subset");
    report_grades(&REP);                CHECK(REP.nrows == 6, "grade report has one row per subject");
    report_sgpa(&REP);                  CHECK(REP.nrows == 55 && atof(REP.cell[0][3]) >= atof(REP.cell[54][3]), "SGPA report sorted");
    report_math(&REP, EXAM_ALL);        CHECK(REP.nrows == 55, "math report");
    CHECK(report_export_csv(&REP, "testdata/out.csv") == 0, "csv export");
    {
        FILE *f = fopen("testdata/out.csv", "r");
        char line[300];
        int lines = 0;
        while (f && fgets(line, sizeof line, f)) lines++;
        if (f) fclose(f);
        CHECK(lines == 55 + 4, "csv has title, prepared-by, demo note, header and 55 rows");
    }

    write_file("testdata/imp.csv", "Roll No,Name,Branch,Section,Semester\n0262101,Imp One,CSM,B,1\n0262102,Imp Two\n990000000056,Dup,CSM,B,1\n,NoRoll\n");
    CHECK(student_import_csv("testdata/imp.csv", &ad, &sk, prob) == 0 && ad == 2 && sk == 2, "student CSV import adds valid, skips dup/invalid");
    CHECK(student_import_csv("testdata/nofile.csv", &ad, &sk, prob) == 1, "missing import file handled");
    write_file("testdata/mk.csv", "roll,subject,ut1,ut2,final\n0262101,U26MA101,15,16,50\n0262102,U26CH101,25,10,10\n0262102,U26CH101,,,\n0262101,XXXX,1,1,1\n");
    CHECK(marks_import_csv("testdata/mk.csv", &ad, &sk, prob) == 0 && ad == 1 && sk == 3, "marks CSV import validates every value");
    CHECK(NEAR(student_exam_pct("0262101", EXAM_ALL, &tot, NULL), 81 * 100.0 / 100) && NEAR(tot, 81), "imported marks counted");
    student_delete(student_find("0262101")); student_delete(student_find("0262102"));
    /* marks export (one row per student, one column per subject) -> edit in a spreadsheet -> import */
    {
        int rows = 0, k0, nlines = 0, c, commas = 0;
        double old0, old2;
        char code[16], line[2048], err[160], first[2048] = "";
        FILE *fp;
        snprintf(code, sizeof code, "%s", subjects[0].code);
        k0 = marks_find("990000000056", code, 0); old0 = k0 >= 0 ? marks[k0].marks : 0;
        k0 = marks_find("990000000056", code, 2); old2 = k0 >= 0 ? marks[k0].marks : 0;
        CHECK(marks_export_csv("testdata/exp.csv", 0, &rows) == 0 && rows == n_students, "CIE 1 export writes one row per student");
        fp = fopen("testdata/exp.csv", "r");
        while (fp && fgets(line, sizeof line, fp)) { if (nlines == 0) snprintf(first, sizeof first, "%s", line); nlines++; }
        if (fp) fclose(fp);
        for (c = 0; first[c]; c++) if (first[c] == ',') commas++;
        CHECK(nlines == rows + 1 && commas == 1 + n_subjects, "export: header + one line per student, 2 + subjects columns");
        CHECK(strstr(first, "Mathematics-I (CIE 1)") != NULL && strstr(first, "Student Name") != NULL, "export: subject names are the column titles");
        CHECK(marks_export_csv("testdata/exp3.csv", 3, &rows) == 0, "export of all three examinations");
        fp = fopen("testdata/exp3.csv", "r"); commas = 0;
        if (fp && fgets(line, sizeof line, fp)) for (c = 0; line[c]; c++) if (line[c] == ',') commas++;
        if (fp) fclose(fp);
        CHECK(commas == 1 + 3 * n_subjects, "all-three export has 3 columns per subject");
        /* sheets from the previous version used UT1/Final in the column titles: still accepted */
        snprintf(line, sizeof line, "Roll No,Student Name,%s (ut1),%s (Final)\n990000000056,x,%g,%g\n", subjects[0].name, subjects[0].name,
                 old0 >= 19 ? 18.0 : 19.0, old2 >= 59 ? 58.0 : 59.0);
        write_file("testdata/exp2.csv", line);
        CHECK(marks_import_csv("testdata/exp2.csv", &ad, &sk, prob) == 0 && ad == 1 && sk == 0, "spreadsheet import accepts old UT1/Final column tags");
        k0 = marks_find("990000000056", code, 0);
        CHECK(k0 >= 0 && NEAR(marks[k0].marks, old0 >= 19 ? 18.0 : 19.0), "edited CIE 1 mark updated");
        k0 = marks_find("990000000056", code, 2);
        CHECK(k0 >= 0 && NEAR(marks[k0].marks, old2 >= 59 ? 58.0 : 59.0), "edited SEE mark updated");
        snprintf(line, sizeof line, "Roll No,Student Name,%s (CIE 1)\n990000000056,x,25\n0262999,y,10\n", subjects[0].name);
        write_file("testdata/exp4.csv", line);
        CHECK(marks_import_csv("testdata/exp4.csv", &ad, &sk, prob) == 0 && ad == 0 && sk == 2, "over-maximum mark and unknown roll are skipped");
        CHECK(marks_set("990000000056", code, 0, old0, err) && marks_set("990000000056", code, 2, old2, err), "test marks restored");
    }

    /* the demonstration marks and attendance were not changed by any of the tests above */
    {
        double s1 = 0, a1 = 0;
        for (i = 0; i < n_marks; i++) s1 += marks[i].marks * (1 + marks[i].exam) + (double)i * 1e-3;
        for (i = 0; i < n_atts; i++) a1 += atts[i].present * (i % 97 + 1);
        CHECK(n_marks == 990 && NEAR(s1, sum0), "all 990 original marks unchanged");
        CHECK(n_atts == 8910 && NEAR(a1, att0), "all attendance records unchanged");
    }

    /* =============== optional blood group (no effect on academic results) =============== */
    {
        double before_pct = student_exam_pct(students[0].roll, EXAM_ALL, NULL, NULL), before_sg = student_sgpa(students[0].roll, NULL, NULL);
        Student t2 = students[0];
        strcpy(t2.blood, "XY");
        CHECK(!student_update(0, &t2, err), "invalid blood group rejected");
        strcpy(t2.blood, "AB-");
        CHECK(student_update(0, &t2, err) && strcmp(students[0].blood, "AB-") == 0, "valid blood group saved");
        CHECK(NEAR(student_exam_pct(students[0].roll, EXAM_ALL, NULL, NULL), before_pct) && NEAR(student_sgpa(students[0].roll, NULL, NULL), before_sg),
              "blood group does not affect academic results");
        for (i = 0; i < 8; i++) { strcpy(t2.blood, BLOOD_GROUPS[i]); CHECK(student_update(0, &t2, err), "every listed blood group accepted"); }
        strcpy(t2.blood, "O+");
        student_update(0, &t2, err);
    }

    /* =============== persistence and idempotence =============== */
    {
        int ns = n_students, nm = n_marks, na = n_atts, nj = n_subjects;
        CHECK(storage_save_all() == 0, "save");
        n_students = n_marks = n_atts = n_subjects = 0;
        CHECK(storage_load_all() == 0, "reload");
        CHECK(n_students == ns && n_marks == nm && n_atts == na && n_subjects == nj && settings.data_version == DATA_VERSION, "data survives reload");
        CHECK(strcmp(students[0].roll, "990000000056") == 0 && total_units() == 30 && strcmp(subjects[2].units[2], "Recursion, Preprocessor and Arrays") == 0, "strings, units survive reload");
        CHECK(strcmp(students[0].blood, "O+") == 0 && students[1].blood[0] != 0 && blood_group_valid(students[1].blood), "blood groups survive reload");
        CHECK(NEAR(exams[2].max_marks, 60) && migration_note[0] == '\0', "exam maximums survive; no migration on current data");
        CHECK(storage_load_all() == 0 && n_students == 55 && n_marks == 990, "repeated start creates no duplicates");
    }

    /* =============== migration from the old (version 1) files =============== */
    {
        int before;
        system("mkdir testdata_old 2>nul");
        write_file("testdata_old/teachers.csv", "#h\nteacher,class123,Prof.PPS Faculty,FAC1024,CSE (AI & ML),pps.faculty@example.edu,CSM-B,1,2026-27\n");
        write_file("testdata_old/students.csv", "#h\n990000000056,Custom Name,CSM,B,1\n990000000057,Diya Nair,CSM,B,1\n0000123,Genuine Extra,CSM,B,1\n");
        write_file("testdata_old/subjects.csv", "#h\nMA101,Mathematics-I,100.0,35.0,4\nXX900,Manual Subject,50.0,20.0,2\n");
        write_file("testdata_old/exams.csv", "#h\nUnit Test 1\nUnit Test 2\nMid Term\n");
        write_file("testdata_old/marks.csv", "#h\n990000000056,MA101,0,77.00\n");
        write_file("testdata_old/attendance.csv", "#h\n990000000056,MA101,2026-09-01,1,1\n");
        write_file("testdata_old/settings.csv", "#h\natt_threshold,70.00\nweight_x,60.00\nweight_y,40.00\n");
        storage_set_dir("testdata_old");
        CHECK(storage_load_all() == 0, "old data migrates");
        CHECK(n_students == 56 || n_students == 55, "students expanded to 55 (plus any unusual extra)");
        CHECK(student_find("0000123") >= 0 && strcmp(students[student_find("990000000056")].name, "Custom Name") == 0, "existing student identities preserved");
        CHECK(students[student_find("990000000056")].blood[0] != 0 && students[student_find("0000123")].blood[0] != 0, "old 5-column student file loads; blood groups assigned by the update");
        CHECK(n_subjects == 6 && subject_find("MA101") < 0 && subject_find("XX900") < 0 && n_exams == 3, "old subjects/exams replaced");
        CHECK(n_marks == n_students * 18 && settings.data_version == DATA_VERSION, "new marks generated for every student");
        CHECK(NEAR(settings.att_threshold, 70) && NEAR(settings.weight_x, 60), "settings preserved");
        CHECK(strcmp(teachers[0].name, "Ms. PPS Faculty") == 0 && strcmp(teachers[0].password, "class123") == 0, "teacher updated, password kept");
        CHECK(strstr(migration_note, "NOT merged") != NULL, "conflict reported for non-demo old data");
        CHECK(count_backup_dirs() == 1, "one backup folder created");
        {
            FILE *f = fopen("testdata_old/migration_report.txt", "r");
            CHECK(f != NULL, "migration report written");
            if (f) fclose(f);
        }
        before = n_students;
        CHECK(storage_load_all() == 0 && n_students == before && n_marks == before * 18 && migration_note[0] == '\0', "second start does not migrate again");
        CHECK(count_backup_dirs() == 1, "no second backup");
        storage_set_dir("testdata");
    }

    /* =============== update from data version 2 (edited marks are never touched) =============== */
    {
        static MarkRec saved[MAX_MARKS];
        int nm, same = 1, blood_kept;
        char keep[8];
        system("mkdir testdata_v2 2>nul");
        storage_set_dir("testdata_v2");
        CHECK(storage_load_all() == 0 && n_students == 55, "fresh data folder for the version 2 test");
        marks[0].marks = marks[0].marks > 1 ? marks[0].marks - 0.5 : marks[0].marks + 0.5;      /* the teacher edits a mark */
        strcpy(students[3].blood, "AB-");                                                           /* an existing blood group */
        strcpy(students[4].blood, "");                                                              /* and one still unspecified */
        strcpy(keep, students[3].blood);
        memcpy(saved, marks, sizeof(MarkRec) * (size_t)n_marks);
        nm = n_marks;
        settings.data_version = 2;                                                                  /* pretend it is an old data folder */
        CHECK(storage_save_all() == 0, "save as a version 2 folder");
        CHECK(storage_load_all() == 0 && settings.data_version == DATA_VERSION, "version 2 data is updated to version 3");
        for (i = 0; i < nm; i++) if (fabs(saved[i].marks - marks[i].marks) > 1e-9 || strcmp(saved[i].roll, marks[i].roll) != 0) same = 0;
        CHECK(n_marks == nm && same, "edited marks are not changed by the update");
        blood_kept = strcmp(students[3].blood, keep) == 0;
        CHECK(blood_kept && students[4].blood[0] != '\0' && blood_group_valid(students[4].blood), "existing blood group kept, missing one assigned");
        CHECK(strstr(migration_note, "NOT changed") != NULL && count_dirs_with_prefix("testdata_v2_backup_v2_") == 1, "backup made and the edited marks reported");
        CHECK(storage_load_all() == 0 && migration_note[0] == '\0' && count_dirs_with_prefix("testdata_v2_backup_v2_") == 1, "update runs only once");
        storage_set_dir("testdata");
        storage_load_all();
    }

    /* =============== empty dataset safety =============== */
    n_students = n_marks = n_atts = 0;
    CHECK(class_avg_pct(EXAM_ALL) < 0 && class_att_pct() < 0 && latest_exam() == -1 && class_avg_sgpa() < 0, "empty dataset safe");
    CHECK(att_pct("nobody", NULL) < 0 && student_sgpa("nobody", NULL, NULL) < 0, "division by zero handled");
    report_class(&REP, EXAM_ALL); report_sgpa(&REP); report_grades(&REP); report_attendance(&REP);

    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
