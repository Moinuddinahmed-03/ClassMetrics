/*
 * math_analyzer.c : the Mathematics-I part of the project (calculations only;
 * the Mathematical Analyzer page in ui_math.c shows the formulas and steps).
 *
 *   Unit 1  Matrices                  : 2-D arrays, transpose, addition, matrix x vector
 *   Unit 2  Calculus of one variable  : finite-difference (discrete) rate of change
 *   Unit 3  Multivariable calculus    : P(x,y) = wx*x + wy*y and partial derivatives
 *
 * Every function uses the real stored records - nothing is hard-coded.
 */
#include "common.h"

/* ================= Unit 1 : matrix operations on 2-D arrays ================= */

/* Transpose: t[j][i] = a[i][j] */
void mat_transpose(const Matrix *a, Matrix *t)
{
    int i, j;
    memset(t, 0, sizeof *t);
    t->rows = a->cols;
    t->cols = a->rows;
    for (i = 0; i < a->rows; i++)
        for (j = 0; j < a->cols; j++)
            t->v[j][i] = a->v[i][j];
}

/* Addition is only defined when both matrices have the same order. */
int mat_add(const Matrix *a, const Matrix *b, Matrix *out)
{
    int i, j;
    if (a->rows != b->rows || a->cols != b->cols) return 0;
    memset(out, 0, sizeof *out);
    out->rows = a->rows;
    out->cols = a->cols;
    for (i = 0; i < a->rows; i++)
        for (j = 0; j < a->cols; j++)
            out->v[i][j] = a->v[i][j] + b->v[i][j];
    return 1;
}

/* Matrix x column-vector: out[i] = sum over j of a[i][j] * w[j] */
void mat_vec(const Matrix *a, const double *w, double *out)
{
    int i, j;
    for (i = 0; i < a->rows; i++) {
        out[i] = 0;
        for (j = 0; j < a->cols; j++)
            out[i] += a->v[i][j] * w[j];
    }
}

/*
 * Builds the marks matrix: rows = chosen students, columns = subjects.
 * exam >= 0 : marks of that examination;  exam = EXAM_ALL : subject totals (CIE 1 + CIE 2 + SEE).
 */
void mat_marks(Matrix *m, int exam, const int *stu, int nrows)
{
    int i, j, k;
    double t;
    memset(m, 0, sizeof *m);
    m->rows = nrows;
    m->cols = n_subjects;
    for (i = 0; i < nrows; i++)
        for (j = 0; j < n_subjects; j++) {
            if (exam < 0) {
                subject_total(students[stu[i]].roll, j, &t, NULL);
                m->v[i][j] = t;
            } else {
                k = marks_find(students[stu[i]].roll, subjects[j].code, exam);
                m->v[i][j] = (k >= 0) ? marks[k].marks : 0.0;
            }
        }
}

/* ================= Unit 2 : discrete rate of change ================= */

const char *trend_label(double rate)
{
    if (rate > 1.0) return "Improving";
    if (rate < -1.0) return "Declining";
    return "Stable";
}

/* ================= Unit 3 : P(x,y) = wx*x + wy*y ================= */

double perf_score(double x, double y, double wx, double wy)
{
    return (wx * x + wy * y) / 100.0;      /* weights are stored as percentages */
}

/* ================= matrix subtraction (D = B - A) ================= */

int mat_sub(const Matrix *a, const Matrix *b, Matrix *out)
{
    int i, j;
    if (a->rows != b->rows || a->cols != b->cols) return 0;
    memset(out, 0, sizeof *out);
    out->rows = a->rows;
    out->cols = a->cols;
    for (i = 0; i < a->rows; i++)
        for (j = 0; j < a->cols; j++)
            out->v[i][j] = a->v[i][j] - b->v[i][j];
    return 1;
}

/* ================= attendance recovery: f(x) = 100 (A + x) / (T + x) =================
 * A = classes attended, T = classes held, x = additional classes, all of them attended. */

double att_projection(int attended, int held, int extra)
{
    if (held + extra <= 0) return -1.0;                       /* no classes at all */
    return 100.0 * (attended + extra) / (double)(held + extra);
}

/* derivative of f:  f'(x) = 100 (T - A) / (T + x)^2  (percentage points gained by one more class) */
double att_slope(int attended, int held, int extra)
{
    double d = (double)(held + extra);
    if (d <= 0) return 0.0;
    return 100.0 * (held - attended) / (d * d);
}

/*
 * Smallest number of extra classes x with f(x) >= target.   f(x) >= t  <=>  x >= (t T - 100 A) / (100 - t).
 * Returns ATT_NEED (x in *extra), ATT_DONE (already reached, x = 0), ATT_IMPOSSIBLE (target of 100 % or more
 * after at least one missed class) or ATT_NODATA (no classes recorded).
 */
int att_classes_needed(int attended, int held, double target, long *extra)
{
    long x;
    *extra = 0;
    if (held <= 0) return ATT_NODATA;
    if (100.0 * attended / held + 1e-9 >= target) return ATT_DONE;
    if (target >= 100.0) return ATT_IMPOSSIBLE;
    x = (long)ceil((target * held - 100.0 * attended) / (100.0 - target) - 1e-9);
    if (x < 1) x = 1;
    while (att_projection(attended, held, (int)x) + 1e-9 < target) x++;       /* guard against rounding */
    while (x > 1 && att_projection(attended, held, (int)(x - 1)) + 1e-9 >= target) x--;
    *extra = x;
    return ATT_NEED;
}
