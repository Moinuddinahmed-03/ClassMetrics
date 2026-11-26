/*
 * ui_math.c : Mathematical Analyzer.
 *
 * A home page with six practical analysis cards; every card opens its own section:
 *   1 Marks Comparison      (matrix addition / subtraction / transpose)
 *   2 Performance Trends    (functions, rate of change, finite differences)
 *   3 Attendance Predictor  (f(x) = 100 (A + x) / (T + x), derivative, inequality)
 *   4 Combined Performance  (P(x, y) = 0.7 x + 0.3 y, partial derivatives)
 *   5 Academic Calculations (step-by-step result calculation)
 *   6 Class Performance     (mean, maximum, minimum, frequency distribution)
 * Every section ends with a collapsed panel "Mathematics Behind This Analysis".
 *
 * All numbers come from the stored records through the functions of math_analyzer.c, marks.c and
 * attendance.c - nothing is hard-coded.  The page is one GtkStack (home + sections); the results of a
 * section are rebuilt whenever its selection changes.
 */
#include "ui.h"

enum { S_HOME = 0, S_MARKS, S_TRENDS, S_ATT, S_COMB, S_CALC, S_CLASS, S_COUNT };

static const char *SEC_NAME[S_COUNT] = { "home", "marks", "trends", "attendance", "combined", "calcs", "class" };
static GtkWidget *stack;                     /* home + six sections */
static GtkWidget *res[S_COUNT];              /* results of a section (rebuilt) */
static GtkWidget *mth[S_COUNT];              /* content of its "Mathematics Behind This Analysis" panel */
static GtkWidget *c_stu[S_COUNT];            /* student selection of a section */
static GtkWidget *c_scope, *c_msub, *c_mmode;               /* Marks Comparison */
static GtkWidget *c_asub, *sc_target, *sc_future, *l_target, *l_future;   /* Attendance Predictor */
static GtkWidget *sc_x, *sc_y;                              /* Combined Performance what-if sliders */
static GtkWidget *c_cexam;                                  /* Class Performance */
static GtkWidget *l_cx, *l_cy;                              /* values beside the what-if sliders */
static int busy = 0, cur_sec = S_HOME;
static int first_show = 1;

#define PERF_WX 70.0       /* P(x, y) = 0.7 x + 0.3 y  (weights as percentages for perf_score) */
#define PERF_WY 30.0
#define NEED_BELOW 50.0    /* a subject below this percentage "requires improvement" */

/* ======================= small widget helpers ======================= */

static void destroy_child(GtkWidget *w, gpointer d) { (void)d; gtk_widget_destroy(w); }
static void clear_box(GtkWidget *b) { gtk_container_foreach(GTK_CONTAINER(b), destroy_child, NULL); }

static GtkWidget *wrap_label(const char *text, const char *cls)
{
    GtkWidget *l = ui_label(text, cls);
    gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_label_set_max_width_chars(GTK_LABEL(l), 120);
    return l;
}

/* White card with a heading and an optional explanation. */
static GtkWidget *section(GtkWidget *parent, const char *title, const char *explain)
{
    GtkWidget *inner, *card = ui_card(&inner);
    gtk_box_pack_start(GTK_BOX(inner), ui_label(title, "card-head"), FALSE, FALSE, 0);
    if (explain && explain[0]) gtk_box_pack_start(GTK_BOX(inner), wrap_label(explain, "hint"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(parent), card, FALSE, FALSE, 0);
    return inner;
}

static void formula(GtkWidget *parent, const char *text)
{
    GtkWidget *l = wrap_label(text, "formula");
    gtk_label_set_selectable(GTK_LABEL(l), TRUE);
    gtk_box_pack_start(GTK_BOX(parent), l, FALSE, FALSE, 0);
}

static void note(GtkWidget *parent, const char *text) { gtk_box_pack_start(GTK_BOX(parent), wrap_label(text, "note"), FALSE, FALSE, 0); }
static void hint(GtkWidget *parent, const char *text) { gtk_box_pack_start(GTK_BOX(parent), wrap_label(text, "hint"), FALSE, FALSE, 0); }

/* Row of result cards: per_row cards in each row. */
static GtkWidget *cards_grid(GtkWidget *parent)
{
    GtkWidget *g = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(g), 12);
    gtk_grid_set_column_spacing(GTK_GRID(g), 12);
    gtk_grid_set_column_homogeneous(GTK_GRID(g), TRUE);
    gtk_box_pack_start(GTK_BOX(parent), g, FALSE, FALSE, 0);
    return g;
}

static void add_card(GtkWidget *grid, int idx, int per_row, const char *cap, const char *val, const char *cls, const char *sub)
{
    GtkWidget *inner, *card = ui_card(&inner), *v = ui_label(val, "card-value");
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "stat-card");
    gtk_box_pack_start(GTK_BOX(inner), ui_label(cap, "card-label"), FALSE, FALSE, 0);
    if (strlen(val) > 11) gtk_style_context_add_class(gtk_widget_get_style_context(v), "card-value-sm");
    if (cls) gtk_style_context_add_class(gtk_widget_get_style_context(v), cls);
    gtk_label_set_ellipsize(GTK_LABEL(v), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign(GTK_LABEL(v), 0);
    gtk_box_pack_start(GTK_BOX(inner), v, FALSE, FALSE, 0);
    if (sub && sub[0]) gtk_box_pack_start(GTK_BOX(inner), wrap_label(sub, "hint"), FALSE, FALSE, 0);
    gtk_widget_set_hexpand(card, TRUE);
    gtk_grid_attach(GTK_GRID(grid), card, idx % per_row, idx / per_row, 1, 1);
}

/* Table built from labels in a grid. */
static GtkWidget *tbl_new(GtkWidget *parent, int ncols, const char **heads, int nleft)
{
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL), *g = gtk_grid_new();
    int c;
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(sw), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(sw), "tbl");
    for (c = 0; c < ncols; c++) {
        GtkWidget *l = gtk_label_new(heads[c]);
        gtk_label_set_xalign(GTK_LABEL(l), c < nleft ? 0.0f : 1.0f);
        gtk_style_context_add_class(gtk_widget_get_style_context(l), "mhead");
        gtk_widget_set_hexpand(l, TRUE);
        gtk_grid_attach(GTK_GRID(g), l, c, 0, 1, 1);
    }
    g_object_set_data(G_OBJECT(g), "ncols", GINT_TO_POINTER(ncols));
    g_object_set_data(G_OBJECT(g), "nleft", GINT_TO_POINTER(nleft));
    g_object_set_data(G_OBJECT(g), "rows", GINT_TO_POINTER(1));
    gtk_container_add(GTK_CONTAINER(sw), g);
    gtk_box_pack_start(GTK_BOX(parent), sw, FALSE, FALSE, 0);
    return g;
}

static void tbl_addv(GtkWidget *g, const char *cls, const char **cells)
{
    int ncols = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(g), "ncols"));
    int nleft = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(g), "nleft"));
    int r = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(g), "rows")), c;
    for (c = 0; c < ncols; c++) {
        GtkWidget *l = gtk_label_new(cells[c]);
        GtkStyleContext *sc = gtk_widget_get_style_context(l);
        gtk_label_set_xalign(GTK_LABEL(l), c < nleft ? 0.0f : 1.0f);
        gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
        gtk_style_context_add_class(sc, "mcell");
        if (r % 2 == 0) gtk_style_context_add_class(sc, "mzebra");
        if (cls) gtk_style_context_add_class(sc, cls);
        gtk_widget_set_hexpand(l, TRUE);
        gtk_grid_attach(GTK_GRID(g), l, c, r, 1, 1);
    }
    g_object_set_data(G_OBJECT(g), "rows", GINT_TO_POINTER(r + 1));
}

static void tbl_add(GtkWidget *g, const char *cls, ...)
{
    int ncols = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(g), "ncols")), c;
    const char *cells[16];
    va_list ap;
    va_start(ap, cls);
    for (c = 0; c < ncols && c < 16; c++) cells[c] = va_arg(ap, const char *);
    va_end(ap);
    tbl_addv(g, cls, cells);
}

/* index of the student chosen in a combo (-1 when there are no students) */
static int stu_sel(GtkWidget *c)
{
    int a = gtk_combo_box_get_active(GTK_COMBO_BOX(c));
    if (n_students <= 0) return -1;
    return (a < 0 || a >= n_students) ? 0 : a;
}

static const char *sname(int j) { return subjects[j].short_name[0] ? subjects[j].short_name : subjects[j].code; }

static const char *change_cls(double d) { return d > 0.05 ? "good" : d < -0.05 ? "warn" : NULL; }

/* ======================= charts (drawn with Cairo; the data is copied into the widget) ======================= */

static void rgb(cairo_t *cr, unsigned hex) { cairo_set_source_rgb(cr, ((hex >> 16) & 255) / 255.0, ((hex >> 8) & 255) / 255.0, (hex & 255) / 255.0); }

static void txt(cairo_t *cr, const char *s, double x, double y, int center, double size, int bold)
{
    cairo_text_extents_t e;
    cairo_select_font_face(cr, "Segoe UI", CAIRO_FONT_SLANT_NORMAL, bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cairo_text_extents(cr, s, &e);
    cairo_move_to(cr, center ? x - e.width / 2 - e.x_bearing : x, y);
    cairo_show_text(cr, s);
}

static void rounded_top(cairo_t *cr, double x, double y, double w, double h, double r)
{
    if (h < 1) h = 1;
    if (r > h) r = h;
    if (r > w / 2) r = w / 2;
    cairo_new_sub_path(cr);
    cairo_move_to(cr, x, y + h);
    cairo_line_to(cr, x, y + r);
    cairo_arc(cr, x + r, y + r, r, G_PI, 1.5 * G_PI);
    cairo_line_to(cr, x + w - r, y);
    cairo_arc(cr, x + w - r, y + r, r, 1.5 * G_PI, 2 * G_PI);
    cairo_line_to(cr, x + w, y + h);
    cairo_close_path(cr);
}

/* ---- grouped bars: up to 3 series per subject / label, values are percentages (negative = missing) ---- */
typedef struct { int ns, ng; double v[3][12]; char lab[12][24]; char sname[3][20]; char title[90]; } Group;
static const unsigned SERIES_COL[3] = { 0x6842F5, 0x3B82F6, 0x14B8A6 };

static gboolean draw_group(GtkWidget *w, cairo_t *cr, gpointer d)
{
    Group *g = d;
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w), i, s, k;
    double px = 38, py = 44, pw = W - 52, ph = H - 84, slot, bw, x, bh;
    char t[24];
    rgb(cr, 0x17213D); txt(cr, g->title, 8, 20, 0, 13, 1);
    for (k = 0; k < g->ns; k++) {                              /* legend */
        double lx = W - 12 - (g->ns - k) * 92;
        rgb(cr, SERIES_COL[k]); cairo_rectangle(cr, lx, 11, 11, 11); cairo_fill(cr);
        rgb(cr, 0x475569); txt(cr, g->sname[k], lx + 15, 21, 0, 11, 0);
    }
    for (k = 0; k <= 4; k++) {                                 /* grid + axis */
        double y = py + ph - ph * k / 4.0;
        rgb(cr, 0xE2E8F0); cairo_set_line_width(cr, 1); cairo_move_to(cr, px, y + 0.5); cairo_line_to(cr, px + pw, y + 0.5); cairo_stroke(cr);
        snprintf(t, sizeof t, "%d", k * 25);
        rgb(cr, 0x64748B); txt(cr, t, px - 8, y + 4, 0, 10, 0);
        cairo_move_to(cr, 0, 0);
    }
    if (g->ng == 0) { rgb(cr, 0x94A3B8); txt(cr, "No data yet", W / 2.0, H / 2.0, 1, 13, 0); return FALSE; }
    slot = pw / g->ng;
    bw = (slot * 0.74) / g->ns;
    if (bw > 38) bw = 38;
    for (i = 0; i < g->ng; i++) {
        for (s = 0; s < g->ns; s++) {
            double v = g->v[s][i];
            if (v < 0) continue;
            if (v > 100) v = 100;
            x = px + slot * i + (slot - bw * g->ns) / 2 + bw * s;
            bh = ph * v / 100.0;
            rgb(cr, SERIES_COL[s]);
            rounded_top(cr, x + 1, py + ph - bh, bw - 2, bh, 5);
            cairo_fill(cr);
            if (bw >= 20) { snprintf(t, sizeof t, "%.0f", g->v[s][i]); rgb(cr, 0x17213D); txt(cr, t, x + bw / 2, py + ph - bh - 4, 1, 10, 0); }
        }
        rgb(cr, 0x475569);
        txt(cr, g->lab[i], px + slot * i + slot / 2, py + ph + 17, 1, 11, 0);
    }
    return FALSE;
}

static void group_chart(GtkWidget *parent, const Group *src, int h)
{
    Group *g = g_malloc(sizeof *g);
    GtkWidget *da = gtk_drawing_area_new();
    *g = *src;
    gtk_widget_set_size_request(da, 300, h);
    gtk_widget_set_hexpand(da, TRUE);
    g_object_set_data_full(G_OBJECT(da), "g", g, g_free);
    g_signal_connect(da, "draw", G_CALLBACK(draw_group), g);
    gtk_box_pack_start(GTK_BOX(parent), da, FALSE, FALSE, 0);
}

/* ---- single-series percentage bars / line (the shared bar and line charts of the application) ---- */
typedef struct { int kind, n; double thr, val[64]; char lab[64][24]; char title[96]; } Chart;

static gboolean draw_chart(GtkWidget *w, cairo_t *cr, gpointer d)
{
    Chart *c = d;
    const char *l[64];
    int i;
    for (i = 0; i < c->n; i++) l[i] = c->lab[i];
    if (c->kind == 0) ui_draw_bars(cr, gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w), l, c->val, c->n, c->thr, c->title);
    else ui_draw_line(cr, gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w), l, c->val, c->n, c->title);
    return FALSE;
}

static void chart_add(GtkWidget *parent, int kind, int n, const char **labs, const double *vals, double thr, const char *title, int h)
{
    Chart *c = g_malloc0(sizeof *c);
    GtkWidget *da = gtk_drawing_area_new();
    int i;
    c->kind = kind; c->thr = thr; c->n = n > 64 ? 64 : n;
    snprintf(c->title, sizeof c->title, "%s", title);
    for (i = 0; i < c->n; i++) { c->val[i] = vals[i]; snprintf(c->lab[i], sizeof c->lab[i], "%s", labs[i]); }
    gtk_widget_set_size_request(da, 300, h);
    gtk_widget_set_hexpand(da, TRUE);
    g_object_set_data_full(G_OBJECT(da), "chart", c, g_free);
    ui_chart_attach_anim(da);
    g_signal_connect(da, "draw", G_CALLBACK(draw_chart), c);
    gtk_box_pack_start(GTK_BOX(parent), da, FALSE, FALSE, 0);
}

/* ---- frequency bars: counts (not percentages) ---- */
typedef struct { int n; double val[12]; char lab[12][24]; char title[90]; } Hist;

static gboolean draw_hist(GtkWidget *w, cairo_t *cr, gpointer d)
{
    Hist *h = d;
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w), i, k;
    double px = 34, py = 40, pw = W - 48, ph = H - 76, mx = 1, slot, bw, bh;
    char t[24];
    for (i = 0; i < h->n; i++) if (h->val[i] > mx) mx = h->val[i];
    mx = ceil(mx / 5.0) * 5.0 < mx ? mx : ceil(mx / 5.0) * 5.0;
    rgb(cr, 0x17213D); txt(cr, h->title, 8, 20, 0, 13, 1);
    for (k = 0; k <= 4; k++) {
        double y = py + ph - ph * k / 4.0;
        rgb(cr, 0xE2E8F0); cairo_set_line_width(cr, 1); cairo_move_to(cr, px, y + 0.5); cairo_line_to(cr, px + pw, y + 0.5); cairo_stroke(cr);
        snprintf(t, sizeof t, "%.0f", mx * k / 4.0);
        rgb(cr, 0x64748B); txt(cr, t, px - 8, y + 4, 0, 10, 0);
    }
    slot = h->n ? pw / h->n : pw;
    bw = slot * 0.56 > 46 ? 46 : slot * 0.56;
    for (i = 0; i < h->n; i++) {
        double x = px + slot * i + (slot - bw) / 2;
        bh = ph * h->val[i] / mx;
        rgb(cr, i == h->n - 1 ? 0xDC2626 : 0x6842F5);                  /* the last class is F (fail) */
        rounded_top(cr, x, py + ph - bh, bw, bh, 6); cairo_fill(cr);
        snprintf(t, sizeof t, "%.0f", h->val[i]);
        rgb(cr, 0x17213D); txt(cr, t, x + bw / 2, py + ph - bh - 4, 1, 11, 0);
        rgb(cr, 0x475569); txt(cr, h->lab[i], x + bw / 2, py + ph + 17, 1, 11, 0);
    }
    return FALSE;
}

static void hist_chart(GtkWidget *parent, const Hist *src, int h)
{
    Hist *g = g_malloc(sizeof *g);
    GtkWidget *da = gtk_drawing_area_new();
    *g = *src;
    gtk_widget_set_size_request(da, 300, h);
    gtk_widget_set_hexpand(da, TRUE);
    g_object_set_data_full(G_OBJECT(da), "h", g, g_free);
    g_signal_connect(da, "draw", G_CALLBACK(draw_hist), g);
    gtk_box_pack_start(GTK_BOX(parent), da, FALSE, FALSE, 0);
}

/* ---- the function f(x) = 100 (A + x) / (T + x) of the Attendance Predictor ---- */
typedef struct { int A, T, status, x_now, xmax; long needed; double target; } Curve;

static gboolean draw_curve(GtkWidget *w, cairo_t *cr, gpointer d)
{
    Curve *c = d;
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w), k, i;
    double px = 42, py = 38, pw = W - 62, ph = H - 76, y, dash[2] = { 6, 4 }, cur = 100.0 * c->A / c->T, lo = cur < c->target ? cur : c->target;
    double ylo = floor((lo - 10.0) / 10.0) * 10.0;                       /* the vertical axis starts below the lower of the two lines */
    char t[48];
    if (ylo < 0) ylo = 0;
    if (ylo > 75) ylo = 75;
    rgb(cr, 0x17213D); txt(cr, "Projected attendance  f(x) = 100 (A + x) / (T + x)", 8, 20, 0, 13, 1);
    for (k = 0; k <= 4; k++) {
        y = py + ph - ph * k / 4.0;
        rgb(cr, 0xE2E8F0); cairo_set_line_width(cr, 1); cairo_move_to(cr, px, y + 0.5); cairo_line_to(cr, px + pw, y + 0.5); cairo_stroke(cr);
        snprintf(t, sizeof t, "%.0f%%", ylo + (100.0 - ylo) * k / 4.0);
        rgb(cr, 0x64748B); txt(cr, t, px - 8, y + 4, 0, 10, 0);
    }
    for (k = 0; k <= 5; k++) {                                              /* x axis labels */
        snprintf(t, sizeof t, "%d", c->xmax * k / 5);
        rgb(cr, 0x64748B); txt(cr, t, px + pw * k / 5.0, py + ph + 16, 1, 10, 0);
    }
    txt(cr, "additional classes attended (x)", px + pw / 2, py + ph + 32, 1, 10.5, 0);
    y = py + ph - ph * (c->target - ylo) / (100.0 - ylo);                   /* target */
    rgb(cr, 0xF59E0B); cairo_set_line_width(cr, 1.5); cairo_set_dash(cr, dash, 2, 0);
    cairo_move_to(cr, px, y + 0.5); cairo_line_to(cr, px + pw, y + 0.5); cairo_stroke(cr); cairo_set_dash(cr, NULL, 0, 0);
    snprintf(t, sizeof t, "target %.0f%%", c->target);
    txt(cr, t, px + pw - 72, y - 5, 0, 10.5, 1);
    rgb(cr, 0x6842F5); cairo_set_line_width(cr, 2.5);                       /* the curve */
    for (i = 0; i <= 120; i++) {
        double x = c->xmax * i / 120.0, f = 100.0 * (c->A + x) / (c->T + x);
        double sx = px + pw * x / c->xmax, sy = py + ph - ph * (f - ylo) / (100.0 - ylo);
        if (i == 0) cairo_move_to(cr, sx, sy); else cairo_line_to(cr, sx, sy);
    }
    cairo_stroke(cr);
    if (c->status == ATT_NEED && c->needed <= c->xmax) {                    /* where the target is reached */
        double sx = px + pw * c->needed / c->xmax;
        rgb(cr, 0xF59E0B); cairo_set_dash(cr, dash, 2, 0); cairo_move_to(cr, sx, py); cairo_line_to(cr, sx, py + ph); cairo_stroke(cr); cairo_set_dash(cr, NULL, 0, 0);
        snprintf(t, sizeof t, "x = %ld", c->needed);
        txt(cr, t, sx + 5, py + 12, 0, 10.5, 1);
    }
    {                                                                       /* the chosen x */
        double f = 100.0 * (c->A + c->x_now) / (c->T + c->x_now);
        double sx = px + pw * (c->x_now > c->xmax ? c->xmax : c->x_now) / c->xmax, sy = py + ph - ph * (f - ylo) / (100.0 - ylo);
        rgb(cr, f + 1e-9 >= c->target ? 0x16A34A : 0xDC2626);
        cairo_arc(cr, sx, sy, 5.5, 0, 2 * G_PI); cairo_fill(cr);
    }
    return FALSE;
}

static void curve_chart(GtkWidget *parent, const Curve *src)
{
    Curve *c = g_malloc(sizeof *c);
    GtkWidget *da = gtk_drawing_area_new();
    *c = *src;
    gtk_widget_set_size_request(da, 300, 250);
    gtk_widget_set_hexpand(da, TRUE);
    g_object_set_data_full(G_OBJECT(da), "c", c, g_free);
    g_signal_connect(da, "draw", G_CALLBACK(draw_curve), c);
    gtk_box_pack_start(GTK_BOX(parent), da, FALSE, FALSE, 0);
}

/* ---- stacked bar: marks contribution + attendance contribution of P(x, y) ---- */
typedef struct { double a, b; } Contrib;

static gboolean draw_contrib(GtkWidget *w, cairo_t *cr, gpointer d)
{
    Contrib *c = d;
    double W = gtk_widget_get_allocated_width(w) - 4, tot = c->a + c->b, wa = tot > 0 ? W * c->a / tot : 0;
    char t[80];
    rgb(cr, 0x6842F5); cairo_rectangle(cr, 2, 6, wa, 30); cairo_fill(cr);
    rgb(cr, 0x14213D); cairo_rectangle(cr, 2 + wa, 6, W - wa, 30); cairo_fill(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    snprintf(t, sizeof t, "0.7 x  =  %.2f", c->a);
    txt(cr, t, 10, 26, 0, 12, 0);
    snprintf(t, sizeof t, "0.3 y  =  %.2f", c->b);
    txt(cr, t, 2 + wa + 8, 26, 0, 12, 0);
    return FALSE;
}

static void contrib_add(GtkWidget *parent, double a, double b)
{
    Contrib *c = g_malloc(sizeof *c);
    GtkWidget *da = gtk_drawing_area_new();
    c->a = a; c->b = b;
    gtk_widget_set_size_request(da, 300, 42);
    g_object_set_data_full(G_OBJECT(da), "c", c, g_free);
    g_signal_connect(da, "draw", G_CALLBACK(draw_contrib), c);
    gtk_box_pack_start(GTK_BOX(parent), da, FALSE, FALSE, 0);
}

/* ======================= "Mathematics Behind This Analysis" panel ======================= */

static GtkWidget *mp_begin(GtkWidget *box, int n, const char *concept, const char *def, const char *why, const char *form)
{
    GtkWidget *inner, *card = ui_card(&inner);
    char t[160];
    snprintf(t, sizeof t, "%d.  %s", n, concept);
    gtk_box_pack_start(GTK_BOX(inner), ui_label(t, "card-head"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Definition in simple language", "card-label"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), wrap_label(def, NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Why it is useful in ClassMetrics", "card-label"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), wrap_label(why, NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Mathematical formula", "card-label"), FALSE, FALSE, 0);
    formula(inner, form);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Step-by-step calculation using the actual records", "card-label"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), card, FALSE, FALSE, 0);
    return inner;
}

static void mp_end(GtkWidget *inner, const char *result, const char *where)
{
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Final result and interpretation", "card-label"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), wrap_label(result, NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Where this concept is applied in ClassMetrics", "card-label"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), wrap_label(where, "hint"), FALSE, FALSE, 0);
}

/* ======================= navigation ======================= */

static void rebuild_section(int s);

static void show_section(int s)
{
    GtkWidget *sw;
    cur_sec = s;
    gtk_stack_set_visible_child_name(GTK_STACK(stack), SEC_NAME[s]);
    if (s != S_HOME) rebuild_section(s);
    sw = gtk_widget_get_ancestor(stack, GTK_TYPE_SCROLLED_WINDOW);          /* every section opens at its top */
    if (sw) gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sw)), 0);
}

static void on_open(GtkWidget *w, gpointer d) { (void)w; show_section(GPOINTER_TO_INT(d)); }
static void on_back(GtkWidget *w, gpointer d) { (void)w; (void)d; show_section(S_HOME); }
static void on_change(GtkWidget *w, gpointer d) { (void)w; if (!busy) rebuild_section(GPOINTER_TO_INT(d)); }

/* ======================= section 1 : Marks Comparison ======================= */

static void short_name(char *out, size_t cap, int i, int idx) { snprintf(out, cap, "Stu %d - %.18s", i + 1, students[idx].name); }

/* rows_are_students = 1: rows are students, columns are subjects (A, B, S, D). */
static void matrix_table(GtkWidget *parent, const Matrix *m, int rows_are_students)
{
    const char *heads[MAT_MAX + 1];
    char hb[MAT_MAX + 1][24], cell[MAT_MAX][20], lab[40];
    GtkWidget *g;
    int i, j;
    heads[0] = rows_are_students ? "Student" : "Subject";
    for (j = 0; j < m->cols; j++) {
        if (rows_are_students) snprintf(hb[j], sizeof hb[j], "%.8s", sname(j));
        else snprintf(hb[j], sizeof hb[j], "Stu %d", j + 1);
        heads[j + 1] = hb[j];
    }
    g = tbl_new(parent, m->cols + 1, heads, 1);
    for (i = 0; i < m->rows; i++) {
        const char *cells[MAT_MAX + 1];
        if (rows_are_students) short_name(lab, sizeof lab, i, i); else snprintf(lab, sizeof lab, "%.14s", sname(i));
        cells[0] = lab;
        for (j = 0; j < m->cols; j++) { snprintf(cell[j], sizeof cell[j], "%.1f", m->v[i][j]); cells[j + 1] = cell[j]; }
        tbl_addv(g, NULL, cells);
    }
}

static Matrix MA, MB, MS, MD, MT;       /* large (200 × 200): static, not on the stack */

static GtkWidget *matrix_page(GtkWidget *nb, const char *tab, const char *text, const Matrix *m, int rows_are_students)
{
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL), *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 10);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    hint(box, text);
    matrix_table(box, m, rows_are_students);
    gtk_container_add(GTK_CONTAINER(sw), box);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), sw, gtk_label_new(tab));
    return sw;
}

static int shot_dtab = 0;
static gboolean shot_dialog_close(gpointer d)
{
    ui_shot_save(GTK_WIDGET(d), "math_full_matrix");
    gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CLOSE);
    return G_SOURCE_REMOVE;
}

/* the detailed demonstration: the complete matrices of the whole class */
static void on_full_matrix(GtkWidget *w, gpointer d)
{
    int all[MAX_STUDENTS], i;
    char t[160];
    GtkWidget *dlg, *nb;
    (void)w; (void)d;
    if (n_students == 0 || n_exams < 2) { ui_info("There are no marks to show."); return; }
    for (i = 0; i < n_students; i++) all[i] = i;
    mat_marks(&MA, 0, all, n_students);
    mat_marks(&MB, 1, all, n_students);
    mat_add(&MA, &MB, &MS);
    mat_sub(&MB, &MA, &MD);
    mat_transpose(&MA, &MT);
    if (MT.cols > 10) MT.cols = 10;
    snprintf(t, sizeof t, "Full marks matrices  (%d students x %d subjects)", n_students, n_subjects);
    dlg = gtk_dialog_new_with_buttons(t, GTK_WINDOW(g_window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "Close", GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 980, 620);
    nb = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), nb, TRUE, TRUE, 0);
    snprintf(t, sizeof t, "Matrix A: %s marks.  Rows = students, columns = subjects, entry a(i, j) = marks of student i in subject j.", exams[0].name);
    matrix_page(nb, "A", t, &MA, 1);
    snprintf(t, sizeof t, "Matrix B: %s marks, same order as A.", exams[1].name);
    matrix_page(nb, "B", t, &MB, 1);
    matrix_page(nb, "A + B", "Matrix addition S = A + B: element by element. Total marks of both tests (out of the combined maximum).", &MS, 1);
    matrix_page(nb, "B - A", "Matrix subtraction D = B - A: positive = improvement from the first to the second test, negative = decline.", &MD, 1);
    matrix_page(nb, "Transpose of A", "Transpose A^T: rows and columns interchanged, so subjects become rows. Only the first 10 students are shown here.", &MT, 0);
    gtk_widget_show_all(dlg);
    if (getenv("CLASSMETRICS_SHOT_DIR")) { gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), shot_dtab); g_timeout_add(1200, shot_dialog_close, dlg); }
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

static void vec_table(GtkWidget *parent, int nrows, const char **rowname, double (*val)[MAX_SUBJECTS], int ncols)
{
    const char *heads[MAX_SUBJECTS + 1];
    char hb[MAX_SUBJECTS][12], cell[MAX_SUBJECTS][16];
    GtkWidget *g;
    int r, j;
    heads[0] = "";
    for (j = 0; j < ncols; j++) { snprintf(hb[j], sizeof hb[j], "%.8s", sname(j)); heads[j + 1] = hb[j]; }
    g = tbl_new(parent, ncols + 1, heads, 1);
    for (r = 0; r < nrows; r++) {
        const char *cells[MAX_SUBJECTS + 1];
        cells[0] = rowname[r];
        for (j = 0; j < ncols; j++) { snprintf(cell[j], sizeof cell[j], "%.1f", val[r][j]); cells[j + 1] = cell[j]; }
        tbl_addv(g, NULL, cells);
    }
}

static void marks_math_panel(GtkWidget *box, int ms)
{
    static double v[4][MAX_SUBJECTS];
    const char *rn[4] = { "A  (CIE 1)", "B  (CIE 2)", "S = A + B", "D = B - A" };
    char t[900], f[300];
    GtkWidget *in;
    int j, one = ms, mj = 0;
    double best = -1e9;
    if (ms < 0 || n_exams < 2 || student_exam_pct(students[ms].roll, 0, NULL, NULL) < 0) { hint(box, "The mathematics needs recorded marks. Select a student with marks."); return; }
    mat_marks(&MA, 0, &one, 1);
    mat_marks(&MB, 1, &one, 1);
    mat_add(&MA, &MB, &MS);
    mat_sub(&MB, &MA, &MD);
    mat_transpose(&MA, &MT);
    for (j = 0; j < n_subjects; j++) {
        v[0][j] = MA.v[0][j]; v[1][j] = MB.v[0][j]; v[2][j] = MS.v[0][j]; v[3][j] = MD.v[0][j];
        if (MD.v[0][j] > best) { best = MD.v[0][j]; mj = j; }
    }
    snprintf(t, sizeof t, "All examples below use the records of %s (roll no. %s).", students[ms].name, students[ms].roll);
    hint(box, t);

    snprintf(f, sizeof f, "A = [ a(i, j) ]      order = students × subjects = %d × %d", n_students, n_subjects);
    in = mp_begin(box, 1, "Matrices",
                  "A matrix is a table of numbers in rows and columns. Each row is one student and each column is one subject, so a(i, j) is the marks of student i in subject j.",
                  "All marks of the class sit in one structure, so the same operations compare students, subjects and tests.", f);
    snprintf(t, sizeof t, "The row of %s is a 1 × %d matrix. The matrix of the whole class has order %d × %d.", students[ms].name, n_subjects, n_students, n_subjects);
    hint(in, t);
    vec_table(in, 2, rn, v, n_subjects);
    snprintf(t, sizeof t, "%s scored %.1f in %s and %.1f in %s for %s.", students[ms].name, v[0][0], exams[0].name, v[1][0], exams[1].name, subjects[0].name);
    mp_end(in, t, "Marks Comparison (this page, tables and charts), Class Performance (averages are column means) and the Reports module.");

    in = mp_begin(box, 2, "Matrix addition",
                  "Two matrices of the same order are added element by element: each entry of the sum is the sum of the two entries in the same position.",
                  "Adding the two internal tests gives the total marks scored in both tests for every subject.", "S(i, j) = A(i, j) + B(i, j)");
    vec_table(in, 3, rn, v, n_subjects);
    snprintf(t, sizeof t, "For %s:  S(%s) = %.1f + %.1f = %.1f.", subjects[0].name, sname(0), v[0][0], v[1][0], v[2][0]);
    formula(in, t);
    snprintf(t, sizeof t, "Combined marks are out of %.0f (two tests of %.0f). Matrices of different order cannot be added; the program refuses such an addition.", exams[0].max_marks + exams[1].max_marks, exams[0].max_marks);
    mp_end(in, t, "The 'Combined' column and the combined chart of Marks Comparison.");

    in = mp_begin(box, 3, "Matrix subtraction",
                  "Subtracting two matrices of the same order gives, element by element, how much one is larger than the other.",
                  "D = B - A shows, for every subject, whether the second test is better (positive) or worse (negative) than the first.", "D(i, j) = B(i, j) - A(i, j)");
    snprintf(t, sizeof t, "For %s:  D(%s) = %.1f - %.1f = %+.1f marks.", subjects[mj].name, sname(mj), v[1][mj], v[0][mj], v[3][mj]);
    formula(in, t);
    snprintf(t, sizeof t, "The largest change for %s is %+.1f marks in %s. Green values in the page are improvements, red values are declines.", students[ms].name, best, subjects[mj].name);
    mp_end(in, t, "Improvement / decline indicators and the status column of Marks Comparison.");

    in = mp_begin(box, 4, "Matrix transpose",
                  "The transpose of a matrix swaps its rows and columns. A matrix with m rows and n columns becomes a matrix with n rows and m columns.",
                  "It lets us read the same marks with subjects as rows, for example to see how all students did in one subject.", "(A^T)(j, i) = A(i, j)      order m × n  becomes  n × m");
    {
        const char *heads[2] = { "Subject", "Entry of A^T" };
        GtkWidget *g = tbl_new(in, 2, heads, 1);
        for (j = 0; j < n_subjects; j++) { char a[24], b[48]; snprintf(a, sizeof a, "%.14s", sname(j)); snprintf(b, sizeof b, "%.1f", MT.v[j][0]); tbl_add(g, NULL, a, b); }
    }
    snprintf(t, sizeof t, "The 1 × %d row of %s becomes a %d × 1 column. For the whole class the order changes from %d × %d to %d × %d.", n_subjects, students[ms].name, n_subjects, n_students, n_subjects, n_subjects, n_students);
    mp_end(in, t, "View Full Matrix (Transpose tab) and any report that lists subjects as rows.");
}

static void build_marks(void)
{
    GtkWidget *box = res[S_MARKS], *in, *grid, *g, *row;
    int scope = gtk_combo_box_get_active(GTK_COMBO_BOX(c_scope)), mode = gtk_combo_box_get_active(GTK_COMBO_BOX(c_mmode));
    int sj = gtk_combo_box_get_active(GTK_COMBO_BOX(c_msub)) - 1, si = stu_sel(c_stu[S_MARKS]);
    int js[MAX_SUBJECTS], nj = 0, j, r, all[MAX_STUDENTS], nrows, k1, k2, k3;
    static Matrix C3;
    double ma[MAX_SUBJECTS], mb[MAX_SUBJECTS], mc[MAX_SUBJECTS], cnt[MAX_SUBJECTS], cnt3[MAX_SUBJECTS], pa[MAX_SUBJECTS], pb[MAX_SUBJECTS], pc[MAX_SUBJECTS], dpp[MAX_SUBJECTS];
    double sa = 0, sb = 0, ssum = 0, bestd = -1e9;
    int used = 0, bestj = -1, have3 = n_exams >= 3;
    int *stu;
    char t[300], h1[40], h2[40], h3[40];
    Group G;

    if (n_students == 0 || n_subjects == 0) { note(box, "There are no students or subjects yet. Add records first."); return; }
    if (n_exams < 2) { note(box, "At least two examinations (CIE 1 and CIE 2) are needed for a comparison."); return; }
    if (scope == 1) { for (r = 0; r < n_students; r++) all[r] = r; stu = all; nrows = n_students; }
    else { all[0] = si; stu = all; nrows = 1; }
    mat_marks(&MA, 0, stu, nrows);
    mat_marks(&MB, 1, stu, nrows);
    mat_add(&MA, &MB, &MS);
    mat_sub(&MB, &MA, &MD);
    if (have3) mat_marks(&C3, 2, stu, nrows);
    if (sj >= 0 && sj < n_subjects) js[nj++] = sj; else for (j = 0; j < n_subjects; j++) js[nj++] = j;

    for (j = 0; j < n_subjects; j++) { ma[j] = mb[j] = mc[j] = cnt[j] = cnt3[j] = 0; }
    for (r = 0; r < nrows; r++)                                      /* column means of the matrices (only rows that have marks) */
        for (j = 0; j < n_subjects; j++) {
            k1 = marks_find(students[stu[r]].roll, subjects[j].code, 0);
            k2 = marks_find(students[stu[r]].roll, subjects[j].code, 1);
            if (k1 >= 0 && k2 >= 0) { ma[j] += MA.v[r][j]; mb[j] += MB.v[r][j]; cnt[j] += 1; }
            k3 = have3 ? marks_find(students[stu[r]].roll, subjects[j].code, 2) : -1;
            if (k3 >= 0) { mc[j] += C3.v[r][j]; cnt3[j] += 1; }
        }
    for (j = 0; j < n_subjects; j++) {
        if (cnt[j] > 0) { ma[j] /= cnt[j]; mb[j] /= cnt[j]; }
        if (cnt3[j] > 0) mc[j] /= cnt3[j];
        pa[j] = cnt[j] > 0 ? pct_of(ma[j], exams[0].max_marks) : -1;
        pb[j] = cnt[j] > 0 ? pct_of(mb[j], exams[1].max_marks) : -1;
        pc[j] = cnt3[j] > 0 ? pct_of(mc[j], exams[2].max_marks) : -1;
        dpp[j] = cnt[j] > 0 ? pb[j] - pa[j] : 0;
    }
    for (r = 0; r < nj; r++) {
        j = js[r];
        if (cnt[j] <= 0) continue;
        sa += pa[j]; sb += pb[j]; ssum += ma[j] + mb[j]; used++;
        if (dpp[j] > bestd) { bestd = dpp[j]; bestj = j; }
    }

    if (scope == 1) snprintf(t, sizeof t, "Whole class (average of %d students)", n_students);
    else snprintf(t, sizeof t, "%s  (roll no. %s)", students[si].name, students[si].roll);
    hint(box, t);
    if (used == 0) { note(box, "No marks are recorded for this selection."); }
    else {
        double chg = (sb - sa) / used;
        char v1[24], v2[24], v3[24], v4[24], s1[80];
        grid = cards_grid(box);
        snprintf(v1, sizeof v1, "%.1f%%", sa / used);
        snprintf(v2, sizeof v2, "%.1f%%", sb / used);
        snprintf(v3, sizeof v3, "%+.1f pp", chg);
        snprintf(v4, sizeof v4, "%.1f / %.0f", ssum / used, exams[0].max_marks + exams[1].max_marks);
        snprintf(h1, sizeof h1, "%s average", exams[0].name); snprintf(h2, sizeof h2, "%s average", exams[1].name);
        snprintf(t, sizeof t, "mean of %s", used > 1 ? "the subjects shown" : "this subject");
        add_card(grid, 0, 4, h1, v1, NULL, t);
        add_card(grid, 1, 4, h2, v2, NULL, t);
        add_card(grid, 2, 4, "Change", v3, change_cls(chg), chg > 0.05 ? "improvement (green)" : chg < -0.05 ? "decline (red)" : "no change");
        snprintf(s1, sizeof s1, "%s + %s combined", exams[0].name, exams[1].name);
        add_card(grid, 3, 4, s1, v4, NULL, "average marks per subject");
        if (bestj >= 0 && nj > 1) {
            snprintf(t, sizeof t, "Subject-wise: the %s is %s (%+.1f pp). Declines are shown in red in the table below.", bestd >= 0 ? "biggest improvement" : "smallest decline", subjects[bestj].name, bestd);
            hint(box, t);
        }
    }

    /* chart */
    memset(&G, 0, sizeof G);
    G.ng = nj > 12 ? 12 : nj;
    for (r = 0; r < G.ng; r++) {
        j = js[r];
        snprintf(G.lab[r], sizeof G.lab[r], "%.10s", sname(j));
        if (mode == 1) G.v[0][r] = cnt[j] > 0 ? pct_of(ma[j] + mb[j], exams[0].max_marks + exams[1].max_marks) : -1;
        else { G.v[0][r] = pa[j]; G.v[1][r] = pb[j]; G.v[2][r] = pc[j]; }
    }
    if (mode == 1) { G.ns = 1; snprintf(G.sname[0], sizeof G.sname[0], "combined %%"); snprintf(G.title, sizeof G.title, "%s + %s combined (percentage of %.0f)", exams[0].name, exams[1].name, exams[0].max_marks + exams[1].max_marks); }
    else if (mode == 2 && have3) { G.ns = 3; for (r = 0; r < 3; r++) snprintf(G.sname[r], sizeof G.sname[r], "%s", exams[r].name); snprintf(G.title, sizeof G.title, "All examinations (each as a percentage of its maximum)"); }
    else { G.ns = 2; snprintf(G.sname[0], sizeof G.sname[0], "%s", exams[0].name); snprintf(G.sname[1], sizeof G.sname[1], "%s", exams[1].name); snprintf(G.title, sizeof G.title, "%s and %s (percentage of the maximum marks)", exams[0].name, exams[1].name); }
    in = section(box, "Comparison chart", "Marks are converted to percentages so tests with different maximum marks can be compared.");
    group_chart(in, &G, 250);

    /* small table */
    in = section(box, "Subject-wise improvement or decline", "Green = improvement, red = decline.");
    {
        const char *heads[7];
        int nc = (mode == 2 && have3) ? 7 : 6;
        snprintf(h1, sizeof h1, "%s (/%.0f)", exams[0].name, exams[0].max_marks);
        snprintf(h2, sizeof h2, "%s (/%.0f)", exams[1].name, exams[1].max_marks);
        snprintf(h3, sizeof h3, "Combined (/%.0f)", exams[0].max_marks + exams[1].max_marks);
        heads[0] = "Subject"; heads[1] = h1; heads[2] = h2; heads[3] = h3; heads[4] = "Change (pp)"; heads[5] = "Status";
        if (nc == 7) heads[6] = "SEE %";
        g = tbl_new(in, nc, heads, 1);
        for (r = 0; r < nj; r++) {
            char c1[16], c2[16], c3[16], c4[16], c5[16];
            const char *cls;
            j = js[r];
            if (cnt[j] <= 0) { tbl_add(g, NULL, subjects[j].name, "-", "-", "-", "-", "no marks", "-"); continue; }
            snprintf(c1, sizeof c1, "%.1f", ma[j]); snprintf(c2, sizeof c2, "%.1f", mb[j]); snprintf(c3, sizeof c3, "%.1f", ma[j] + mb[j]);
            snprintf(c4, sizeof c4, "%+.1f", dpp[j]);
            snprintf(c5, sizeof c5, "%s", pc[j] >= 0 ? "" : "-");
            if (pc[j] >= 0) snprintf(c5, sizeof c5, "%.1f", pc[j]);
            cls = change_cls(dpp[j]);
            if (nc == 7) tbl_add(g, cls, subjects[j].name, c1, c2, c3, c4, dpp[j] > 0.05 ? "Improved" : dpp[j] < -0.05 ? "Declined" : "No change", c5);
            else tbl_add(g, cls, subjects[j].name, c1, c2, c3, c4, dpp[j] > 0.05 ? "Improved" : dpp[j] < -0.05 ? "Declined" : "No change");
        }
    }
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(row), ui_button("View Full Matrix", "accent", G_CALLBACK(on_full_matrix), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), wrap_label("Opens the complete matrices A, B, A + B, B - A and the transpose for a detailed demonstration.", "hint"), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(in), row, FALSE, FALSE, 4);

    marks_math_panel(mth[S_MARKS], scope == 0 ? si : (n_students > 0 ? 0 : -1));
}

/* ======================= section 2 : Performance Trends ======================= */

static void build_trends(void)
{
    GtkWidget *box = res[S_TRENDS], *in, *grid, *g;
    int si = stu_sel(c_stu[S_TRENDS]), n = 0, e, j, ne;
    int exi[MAX_EXAMS];
    double pe[MAX_EXAMS], tot[MAX_EXAMS], mxm[MAX_EXAMS], avg = 0, latest = 0, d, sp[MAX_SUBJECTS][3];
    const char *labs[MAX_EXAMS];
    char t[600], lb[MAX_EXAMS][24], a[32], b[32], c[32];
    int best = -1, need[MAX_SUBJECTS], nneed = 0;
    double bestp = -1;
    const char *roll;

    if (si < 0) { note(box, "There are no students yet."); return; }
    roll = students[si].roll;
    ne = n_exams > MAX_EXAMS ? MAX_EXAMS : n_exams;
    for (e = 0; e < ne; e++) {
        double p = student_exam_pct(roll, e, &tot[n], &mxm[n]);
        if (p < 0) continue;
        pe[n] = p; exi[n] = e; snprintf(lb[n], sizeof lb[n], "%s", exams[e].name); labs[n] = lb[n]; n++;
    }
    snprintf(t, sizeof t, "%s  (roll no. %s)", students[si].name, roll);
    hint(box, t);
    if (n == 0) { note(box, "No marks are recorded for this student."); hint(mth[S_TRENDS], "The mathematics needs recorded marks. Select a student with marks."); return; }

    if (n >= 2) {
        avg = (pe[n - 1] - pe[0]) / (double)(exi[n - 1] - exi[0]);
        latest = (pe[n - 1] - pe[n - 2]) / (double)(exi[n - 1] - exi[n - 2]);
    }
    for (j = 0; j < n_subjects; j++) {
        double ps = subject_pct(roll, j);
        int first = -1, last = -1;
        sp[j][0] = sp[j][1] = sp[j][2] = -1;
        for (e = 0; e < 3 && e < n_exams; e++) {
            int k = marks_find(roll, subjects[j].code, e);
            if (k >= 0) { sp[j][e] = pct_of(marks[k].marks, exams[e].max_marks); if (first < 0) first = e; last = e; }
        }
        if (ps > bestp) { bestp = ps; best = j; }
        if (ps >= 0 && (ps < NEED_BELOW || (first >= 0 && last > first && sp[j][last] - sp[j][first] <= -5.0))) need[nneed++] = j;
    }

    grid = cards_grid(box);
    snprintf(a, sizeof a, "%s", n >= 2 ? trend_label(avg) : "-");
    add_card(grid, 0, 4, "Overall trend", a, n >= 2 ? (avg > 1 ? "good" : avg < -1 ? "warn" : "amber") : NULL, "improving above +1, declining below -1 pp per exam");
    snprintf(b, sizeof b, n >= 2 ? "%+.2f pp" : "-", avg);
    add_card(grid, 1, 4, "Average rate of change", b, n >= 2 ? change_cls(avg) : NULL, "percentage points per examination");
    snprintf(c, sizeof c, "%.14s", best >= 0 ? sname(best) : "-");
    snprintf(t, sizeof t, "%s - %.1f%%", best >= 0 ? subjects[best].name : "", bestp);
    add_card(grid, 2, 4, "Best-performing subject", c, "good", t);
    if (nneed == 0) add_card(grid, 3, 4, "Subjects to improve", "None", "good", "all subjects are above 50% and stable");
    else {
        char names[160] = "";
        for (j = 0; j < nneed; j++) { char x[24]; snprintf(x, sizeof x, "%s%.8s", j ? ", " : "", sname(need[j])); if (strlen(names) + strlen(x) < sizeof names) strcat(names, x); }
        snprintf(a, sizeof a, "%d subject%s", nneed, nneed > 1 ? "s" : "");
        add_card(grid, 3, 4, "Subjects to improve", a, "warn", names);
    }

    in = section(box, "Performance line graph", "Every examination is converted to a percentage of its own maximum before comparing (CIE 1 and CIE 2 are out of 20, SEE is out of 60).");
    chart_add(in, 1, n, labs, pe, 0, "Overall marks percentage by examination", 230);

    in = section(box, "Examination-wise percentages", NULL);
    {
        const char *heads[5] = { "Examination", "Marks obtained", "Maximum", "Percentage", "Change (pp)" };
        g = tbl_new(in, 5, heads, 1);
        for (e = 0; e < n; e++) {
            char m1[16], m2[16], m3[16], m4[16];
            snprintf(m1, sizeof m1, "%.1f", tot[e]); snprintf(m2, sizeof m2, "%.0f", mxm[e]); snprintf(m3, sizeof m3, "%.2f%%", pe[e]);
            if (e == 0) snprintf(m4, sizeof m4, "-"); else snprintf(m4, sizeof m4, "%+.2f", pe[e] - pe[e - 1]);
            tbl_add(g, e ? change_cls(pe[e] - pe[e - 1]) : NULL, lb[e], m1, m2, m3, m4);
        }
    }
    in = section(box, "Subject-wise performance", "Percentage of each subject in every examination and the change from the first to the last one.");
    {
        char h[5][24];
        const char *heads[6];
        heads[0] = "Subject";
        for (e = 0; e < 3; e++) { if (e < n_exams) snprintf(h[e], sizeof h[e], "%s %%", exams[e].name); else snprintf(h[e], sizeof h[e], "-"); heads[1 + e] = h[e]; }
        heads[4] = "Change (pp)"; heads[5] = "Total %";
        g = tbl_new(in, 6, heads, 1);
        for (j = 0; j < n_subjects; j++) {
            char v[3][16], ch[16], tp[16];
            double first = -1, last = -1, ps = subject_pct(roll, j);
            for (e = 0; e < 3; e++) {
                if (sp[j][e] >= 0) { snprintf(v[e], sizeof v[e], "%.1f", sp[j][e]); if (first < 0) first = sp[j][e]; last = sp[j][e]; }
                else snprintf(v[e], sizeof v[e], "-");
            }
            if (first >= 0 && last >= 0 && first != last) snprintf(ch, sizeof ch, "%+.1f", last - first); else snprintf(ch, sizeof ch, "-");
            snprintf(tp, sizeof tp, ps >= 0 ? "%.1f" : "-", ps);
            tbl_add(g, ps >= 0 && ps < NEED_BELOW ? "warn" : (first >= 0 ? change_cls(last - first) : NULL), subjects[j].name, v[0], v[1], v[2], ch, tp);
        }
    }
    in = section(box, "Simple performance interpretation", NULL);
    if (n < 2) snprintf(t, sizeof t, "Only one examination has marks, so a trend cannot be calculated yet (%s: %.1f%%).", lb[0], pe[0]);
    else {
        const char *lab = trend_label(avg);
        snprintf(t, sizeof t, "%s's overall percentage moved from %.1f%% in %s to %.1f%% in %s, a change of %+.1f percentage points (average %+.2f per examination, latest %+.2f). Trend: %s. Best subject: %s. %s",
                 students[si].name, pe[0], lb[0], pe[n - 1], lb[n - 1], pe[n - 1] - pe[0], avg, latest, lab, best >= 0 ? subjects[best].name : "-",
                 nneed ? "Some subjects need attention (below 50% or falling by 5 points or more)." : "No subject needs special attention.");
    }
    d = 0; (void)d;
    gtk_box_pack_start(GTK_BOX(in), wrap_label(t, NULL), FALSE, FALSE, 0);
    note(in, "The rate of change here is a finite difference between examinations. It is an approximation, not a differentiation of a continuous function.");

    /* ---- Mathematics Behind This Analysis ---- */
    {
        GtkWidget *mb = mth[S_TRENDS];
        char f[300];
        snprintf(t, sizeof t, "All examples use the records of %s.", students[si].name);
        hint(mb, t);
        in = mp_begin(mb, 1, "Functions",
                      "A function gives exactly one output for every input. Here the input is the examination number t (1, 2, 3) and the output is the marks percentage p(t).",
                      "Writing performance as a function p(t) lets us talk about how it changes from one examination to the next.", "p(t) = marks obtained in examination t / maximum marks × 100");
        { GtkWidget *tg; const char *hd[4] = { "t", "Examination", "Marks / maximum", "p(t)" }; tg = tbl_new(in, 4, hd, 2);
          for (e = 0; e < n; e++) { char s1[8], s2[32], s3[20]; snprintf(s1, sizeof s1, "%d", exi[e] + 1); snprintf(s2, sizeof s2, "%.1f / %.0f", tot[e], mxm[e]); snprintf(s3, sizeof s3, "%.2f%%", pe[e]); tbl_add(tg, NULL, s1, lb[e], s2, s3); } }
        snprintf(t, sizeof t, "p(1) = %.2f%%%s", pe[0], n > 1 ? "; p(2), p(3) ... follow in the same way for the next examinations." : ".");
        mp_end(in, t, "The line graph and the examination-wise table on this page. Percentages make examinations with different maximum marks comparable.");

        if (n >= 2) {
            snprintf(f, sizeof f, "rate = ( p(t2) - p(t1) ) / ( t2 - t1 )   percentage points per examination");
            in = mp_begin(mb, 2, "Rate of change",
                          "The rate of change tells how quickly a quantity changes. A positive rate means the percentage is rising, a negative rate means it is falling.",
                          "A teacher can see at once whether a student is improving or declining, not only the latest percentage.", f);
            { GtkWidget *tg; const char *hd[5] = { "From", "To", "p(t1)", "p(t2)", "Rate (pp / exam)" }; tg = tbl_new(in, 5, hd, 2);
              for (e = 1; e < n; e++) { char s1[16], s2[16], s3[16], s4[24]; snprintf(s1, sizeof s1, "%.2f", pe[e - 1]); snprintf(s2, sizeof s2, "%.2f", pe[e]); snprintf(s3, sizeof s3, "%+.2f", (pe[e] - pe[e - 1]) / (exi[e] - exi[e - 1])); snprintf(s4, sizeof s4, "%s", "");
                tbl_add(tg, change_cls(pe[e] - pe[e - 1]), lb[e - 1], lb[e], s1, s2, s3); (void)s4; } }
            snprintf(t, sizeof t, "Average rate = (%.2f - %.2f) / (%d - %d) = %+.2f pp per examination, so the trend is %s.", pe[n - 1], pe[0], exi[n - 1] + 1, exi[0] + 1, avg, trend_label(avg));
            mp_end(in, t, "The 'Overall trend' and 'Average rate of change' cards.");

            in = mp_begin(mb, 3, "Finite differences",
                          "When a quantity is known only at a few points, the difference between neighbouring values divided by the step between them is a finite difference. It approximates the slope between those points.",
                          "We have only three examinations, so there is no smooth curve to differentiate. A finite difference is the correct and honest tool.", "Δp / Δt = ( p(t + 1) - p(t) ) / 1");
            snprintf(t, sizeof t, "Latest step: Δp = %.2f - %.2f = %+.2f pp over Δt = %d examination(s), rate = %+.2f pp per examination.", pe[n - 1], pe[n - 2], pe[n - 1] - pe[n - 2], exi[n - 1] - exi[n - 2], latest);
            formula(in, t);
            mp_end(in, "A finite difference is NOT the same as differentiation: differentiation needs a continuous function and gives the exact instantaneous rate, while the finite difference only gives the average rate between two examinations.", "Every rate shown in Performance Trends.");

            in = mp_begin(mb, 4, "Applications of calculus",
                          "Calculus studies change. The derivative dp/dt of a smooth function is the exact rate of change; the sign of the derivative tells whether the function is increasing or decreasing.",
                          "The finite difference is the discrete version of this idea, so the interpretation is the same: positive means improving, negative means declining.", "dp/dt  ~  Δp / Δt   (approximation from the examination records)");
            snprintf(t, sizeof t, "Rule used by the application: rate > +1 -> Improving, rate < -1 -> Declining, otherwise Stable. For %s the average rate is %+.2f, so: %s.", students[si].name, avg, trend_label(avg));
            formula(in, t);
            mp_end(in, "The sign and size of the rate give a simple, meaningful interpretation of academic progress.", "Overall trend card and the performance interpretation in this section.");
        } else hint(mb, "Marks of at least two examinations are needed for the rate of change.");
    }
}

/* ======================= section 3 : Attendance Predictor ======================= */

static void on_att_scale(GtkWidget *w, gpointer d)
{
    char t[48];
    (void)w; (void)d;
    snprintf(t, sizeof t, "%.0f %%", gtk_range_get_value(GTK_RANGE(sc_target)));
    gtk_label_set_text(GTK_LABEL(l_target), t);
    snprintf(t, sizeof t, "%.0f classes", gtk_range_get_value(GTK_RANGE(sc_future)));
    gtk_label_set_text(GTK_LABEL(l_future), t);
    if (!busy) rebuild_section(S_ATT);
}

static void att_math_panel(GtkWidget *box, int A, int T, double target, int status, long need, int xf)
{
    GtkWidget *in;
    char t[700], f[300];
    double cur = T > 0 ? 100.0 * A / T : 0;
    if (T <= 0) { hint(box, "The mathematics needs at least one recorded class for this student and subject."); return; }

    snprintf(f, sizeof f, "f(x) = 100 × (A + x) / (T + x)      A = classes attended,  T = classes held,  x = extra classes attended");
    in = mp_begin(box, 1, "Functions",
                  "A function turns an input into exactly one output. Here the input is x, the number of additional classes the student attends, and the output is the attendance percentage after those classes.",
                  "It predicts the attendance percentage for any number of future classes, assuming the student attends all of them.", f);
    {
        GtkWidget *g;
        const char *hd[3] = { "Additional classes x", "Calculation", "f(x)" };
        g = tbl_new(in, 3, hd, 2);
        { char s1[16], s2[80], s3[16]; snprintf(s1, sizeof s1, "0"); snprintf(s2, sizeof s2, "100 × %d / %d", A, T); snprintf(s3, sizeof s3, "%.2f%%", att_projection(A, T, 0)); tbl_add(g, NULL, s1, s2, s3); }
        if (xf > 0) { char s1[16], s2[80], s3[16]; snprintf(s1, sizeof s1, "%d", xf); snprintf(s2, sizeof s2, "100 × (%d + %d) / (%d + %d)", A, xf, T, xf); snprintf(s3, sizeof s3, "%.2f%%", att_projection(A, T, xf)); tbl_add(g, NULL, s1, s2, s3); }
        if (status == ATT_NEED) { char s1[16], s2[80], s3[16]; snprintf(s1, sizeof s1, "%ld", need); snprintf(s2, sizeof s2, "100 × (%d + %ld) / (%d + %ld)", A, need, T, need); snprintf(s3, sizeof s3, "%.2f%%", att_projection(A, T, (int)need)); tbl_add(g, "good", s1, s2, s3); }
    }
    snprintf(t, sizeof t, "Currently f(0) = %.2f%%. %s", cur, status == ATT_NEED ? "Attending the required number of classes lifts the percentage to the target." : status == ATT_DONE ? "The target is already reached." : "The target cannot be reached.");
    mp_end(in, t, "Projected percentage, progress bar and curve of the Attendance Predictor.");

    snprintf(f, sizeof f, "f'(x) = 100 (T - A) / (T + x)^2          (quotient rule applied to f(x))");
    in = mp_begin(box, 2, "Differentiation",
                  "The derivative f'(x) is the exact instantaneous rate of change of f. It tells how many percentage points one more attended class adds at that moment.",
                  "It shows why recovery is slow: the derivative is positive when the student has missed classes and becomes smaller as x grows, so each extra class helps less than the previous one.", f);
    snprintf(t, sizeof t, "f'(0) = 100 × (%d - %d) / %d^2 = %.4f pp per class.   f'(%d) = 100 × (%d - %d) / (%d + %d)^2 = %.4f pp per class.", T, A, T, att_slope(A, T, 0), xf, T, A, T, xf, att_slope(A, T, xf));
    formula(in, t);
    snprintf(t, sizeof t, "%s", T == A ? "Because no class was missed (T = A), f'(x) = 0: attendance is already 100% and cannot grow." : "The derivative is positive and decreasing: the first extra classes help most, and every further class adds a little less (diminishing returns).");
    mp_end(in, t, "The 'gain per extra class' line of the projection.");

    snprintf(f, sizeof f, "f(x) >= t   <=>   x >= ( t T - 100 A ) / ( 100 - t )       (for t < 100)");
    in = mp_begin(box, 3, "Inequalities",
                  "An inequality states that one quantity is at least (or at most) another. Solving it gives all values that satisfy it; here we need the smallest whole number of classes.",
                  "The teacher wants to know the minimum number of classes needed to reach the target attendance.", f);
    snprintf(t, sizeof t, "100 (%d + x) / (%d + x) >= %.0f   =>   100 × %d + 100 x >= %.0f × %d + %.0f x   =>   x (100 - %.0f) >= %.0f × %d - 100 × %d", A, T, target, A, target, T, target, target, target, T, A);
    formula(in, t);
    if (status == ATT_NEED) {
        snprintf(t, sizeof t, "x >= (%.0f × %d - 100 × %d) / (100 - %.0f) = %.2f   =>   x = %ld classes (rounded up to a whole number).", target, T, A, target, (target * T - 100.0 * A) / (100.0 - target), need);
        formula(in, t);
        snprintf(t, sizeof t, "Check: f(%ld) = %.2f%% >= %.0f%%, while f(%ld) = %.2f%% is below the target.", need, att_projection(A, T, (int)need), target, need - 1, att_projection(A, T, (int)(need - 1)));
        mp_end(in, t, "The 'Additional classes required' card.");
    } else if (status == ATT_DONE) {
        snprintf(t, sizeof t, "The current attendance %.2f%% is already at least %.0f%%, so x = 0 satisfies the inequality.", cur, target);
        mp_end(in, t, "The 'Additional classes required' card (target achieved).");
    } else {
        mp_end(in, "For a target of 100% the inequality has no solution after a missed class: f(x) gets closer to 100% but never equals it. Choose a target below 100%.", "The 'Additional classes required' card (target impossible).");
    }

    snprintf(f, sizeof f, "average rate of change = ( f(x2) - f(x1) ) / ( x2 - x1 )");
    in = mp_begin(box, 4, "Rate of change",
                  "The rate of change measures how fast one quantity changes when another changes. The average rate is taken over an interval, the derivative gives the rate at one point.",
                  "It explains how quickly a student can recover and why recovering a large shortage takes many classes.", f);
    if (status == ATT_NEED) {
        snprintf(t, sizeof t, "Average rate from x = 0 to x = %ld:  (%.2f - %.2f) / %ld = %.4f pp per class, while the rate at x = 0 is f'(0) = %.4f pp per class (the rate keeps falling).", need, att_projection(A, T, (int)need), cur, need, (att_projection(A, T, (int)need) - cur) / need, att_slope(A, T, 0));
        formula(in, t);
        mp_end(in, "Each extra class raises attendance by less than the one before, so the average rate over the whole recovery is smaller than the starting rate.", "Gain per extra class in the Attendance Predictor.");
    } else {
        snprintf(t, sizeof t, "The rate at x = 0 is f'(0) = %.4f pp per class.", att_slope(A, T, 0));
        formula(in, t);
        mp_end(in, status == ATT_DONE ? "No recovery is needed because the target is already reached." : "A recovery rate cannot be computed for an unreachable target.", "Gain per extra class in the Attendance Predictor.");
    }
}

static void build_att(void)
{
    GtkWidget *box = res[S_ATT], *in, *grid, *pb;
    int si = stu_sel(c_stu[S_ATT]), sj = gtk_combo_box_get_active(GTK_COMBO_BOX(c_asub)) - 1, T = 0, A = 0, status, xf;
    long need = 0;
    double target, cur, proj;
    const char *code, *roll;
    char t[500], v[40];
    Curve C;

    if (si < 0) { note(box, "There are no students yet."); return; }
    roll = students[si].roll;
    code = (sj >= 0 && sj < n_subjects) ? subjects[sj].code : NULL;
    att_stats(roll, code, &T, &A);
    target = gtk_range_get_value(GTK_RANGE(sc_target));
    xf = (int)(gtk_range_get_value(GTK_RANGE(sc_future)) + 0.5);
    status = att_classes_needed(A, T, target, &need);
    cur = T > 0 ? 100.0 * A / T : 0;

    snprintf(t, sizeof t, "%s  (roll no. %s)  -  %s", students[si].name, roll, code ? subjects[sj].name : "all subjects");
    hint(box, t);
    grid = cards_grid(box);
    snprintf(v, sizeof v, "%d", T);               add_card(grid, 0, 3, "Total classes conducted", v, NULL, "T");
    snprintf(v, sizeof v, "%d", A);               add_card(grid, 1, 3, "Classes attended", v, "good", "A");
    snprintf(v, sizeof v, "%d", T - A);           add_card(grid, 2, 3, "Classes missed", v, T - A > 0 ? "warn" : "good", "T - A");
    snprintf(v, sizeof v, T > 0 ? "%.2f%%" : "-", cur);
    add_card(grid, 3, 3, "Current attendance", v, T > 0 ? (cur + 1e-9 >= target ? "good" : "warn") : NULL, "A / T × 100");
    snprintf(v, sizeof v, "%.0f%%", target);      add_card(grid, 4, 3, "Required attendance", v, NULL, "adjustable target");
    if (status == ATT_NEED) { snprintf(v, sizeof v, "%ld classes", need); add_card(grid, 5, 3, "Additional classes required", v, "warn", "consecutive classes, all attended"); }
    else if (status == ATT_DONE) add_card(grid, 5, 3, "Additional classes required", "0", "good", "target already achieved");
    else if (status == ATT_IMPOSSIBLE) add_card(grid, 5, 3, "Additional classes required", "Not possible", "warn", "100% cannot be reached");
    else add_card(grid, 5, 3, "Additional classes required", "No records", NULL, "no classes recorded");

    if (status == ATT_NODATA) { note(box, "There are no attendance records for this student and subject, so a prediction is not possible."); att_math_panel(mth[S_ATT], A, T, target, status, need, xf); return; }
    if (status == ATT_NEED) snprintf(t, sizeof t, "To reach %.0f%% the student must attend the next %ld consecutive classes without missing any: f(%ld) = %.2f%%.", target, need, need, att_projection(A, T, (int)need));
    else if (status == ATT_DONE) snprintf(t, sizeof t, "The target of %.0f%% is already achieved (current attendance %.2f%%). No additional classes are required.", target, cur);
    else snprintf(t, sizeof t, "A target of %.0f%% is mathematically impossible: after %d missed class%s the percentage can approach 100%% but never reach it. Choose a lower target.", target, T - A, T - A == 1 ? "" : "es");
    in = section(box, "Prediction", NULL);
    gtk_box_pack_start(GTK_BOX(in), wrap_label(t, NULL), FALSE, FALSE, 0);
    note(in, "This prediction assumes that the student attends EVERY additional class. Each missed class lowers the result.");

    proj = att_projection(A, T, xf);
    in = section(box, "Projected attendance", NULL);
    snprintf(t, sizeof t, "If the student attends the next %d class%s:", xf, xf == 1 ? "" : "es");
    hint(in, t);
    snprintf(v, sizeof v, "%.2f%%", proj);
    {
        GtkWidget *val = ui_label(v, "card-value");
        gtk_style_context_add_class(gtk_widget_get_style_context(val), proj + 1e-9 >= target ? "good" : "warn");
        gtk_label_set_xalign(GTK_LABEL(val), 0);
        gtk_box_pack_start(GTK_BOX(in), val, FALSE, FALSE, 0);
    }
    pb = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pb), proj / 100.0 > 1 ? 1 : proj / 100.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(pb), proj + 1e-9 >= target ? "att-good" : "att-low");
    gtk_box_pack_start(GTK_BOX(in), pb, FALSE, FALSE, 2);
    snprintf(t, sizeof t, "%s  (target %.0f%%).   Gain per extra class right now: f'(%d) = %.3f percentage points.",
             proj + 1e-9 >= target ? "Target reached" : "Below the target", target, xf, att_slope(A, T, xf));
    hint(in, t);

    memset(&C, 0, sizeof C);
    C.A = A; C.T = T; C.status = status; C.x_now = xf; C.needed = need; C.target = target;
    C.xmax = 60;
    if (status == ATT_NEED) C.xmax = need <= 300 ? (int)(need * 1.15) + 5 : 300;
    if (C.xmax < xf + 5) C.xmax = xf + 5;
    if (C.xmax < 20) C.xmax = 20;
    in = section(box, "Attendance curve", "The curve shows the projected percentage for every number of additional classes; the dot is the value chosen with the slider.");
    curve_chart(in, &C);

    att_math_panel(mth[S_ATT], A, T, target, status, need, xf);
}

/* ======================= section 4 : Combined Performance ======================= */

static const char *interp(double P, const char **cls)
{
    if (P >= 75) { *cls = "good"; return "Strong"; }
    if (P >= 60) { *cls = "good"; return "Satisfactory"; }
    if (P >= 40) { *cls = "amber"; return "Needs improvement"; }
    *cls = "warn";
    return "At risk";
}

static double comb_actual_x(int si) { double p = student_exam_pct(students[si].roll, EXAM_ALL, NULL, NULL); return p < 0 ? 0 : p; }
static double comb_actual_y(int si) { double p = att_pct(students[si].roll, NULL); return p < 0 ? 0 : p; }

/* the sliders start at the real marks and attendance of the selected student */
static void comb_reset(void)
{
    int si = stu_sel(c_stu[S_COMB]);
    if (si < 0) return;
    busy++;
    gtk_range_set_value(GTK_RANGE(sc_x), comb_actual_x(si));
    gtk_range_set_value(GTK_RANGE(sc_y), comb_actual_y(si));
    busy--;
}

static void on_comb_student(GtkWidget *w, gpointer d) { (void)w; (void)d; if (busy) return; comb_reset(); rebuild_section(S_COMB); }
static void on_comb_reset(GtkWidget *w, gpointer d) { (void)w; (void)d; comb_reset(); rebuild_section(S_COMB); }

static void comb_math_panel(GtkWidget *box, int si, double x0, double y0, double P0, double xw, double yw, double Pw)
{
    GtkWidget *in;
    char t[700];
    const char *cls;
    snprintf(t, sizeof t, "All examples use the records of %s: marks percentage x = %.2f, attendance percentage y = %.2f.", students[si].name, x0, y0);
    hint(box, t);

    in = mp_begin(box, 1, "Functions of several variables",
                  "A function of two variables gives one output for every pair (x, y). Here x is the marks percentage and y is the attendance percentage, and the output is the combined score P.",
                  "Marks and attendance are two different measures; a function of two variables combines them into one number.", "P(x, y) = 0.7 x + 0.3 y");
    snprintf(t, sizeof t, "P(%.2f, %.2f) = 0.7 × %.2f + 0.3 × %.2f = %.2f + %.2f = %.2f", x0, y0, x0, y0, 0.7 * x0, 0.3 * y0, P0);
    formula(in, t);
    snprintf(t, sizeof t, "The combined score is %.2f, which is interpreted as: %s.", P0, interp(P0, &cls));
    mp_end(in, t, "Combined score card and the what-if score of Combined Performance.");

    in = mp_begin(box, 2, "Partial derivatives",
                  "A partial derivative measures how a function changes when ONE variable changes and the other is held constant.",
                  "It tells the teacher how much one more percentage point of marks (or of attendance) is worth in the combined score.", "dP/dx = 0.7        dP/dy = 0.3   (written with the partial-derivative symbol: \xE2\x88\x82P/\xE2\x88\x82x = 0.7,  \xE2\x88\x82P/\xE2\x88\x82y = 0.3)");
    snprintf(t, sizeof t, "Changing the sliders from (%.2f, %.2f) to (%.2f, %.2f):  \xCE\x94P = 0.7 × (%+.2f) + 0.3 × (%+.2f) = %+.2f,   and P(new) - P(old) = %.2f - %.2f = %+.2f.", x0, y0, xw, yw, xw - x0, yw - y0, 0.7 * (xw - x0) + 0.3 * (yw - y0), Pw, P0, Pw - P0);
    formula(in, t);
    mp_end(in, "Raising marks by 1 point raises P by 0.7, while raising attendance by 1 point raises P by only 0.3: marks have the larger influence in this model. The two results agree exactly because P is linear.", "The change shown under 'What-if analysis'.");

    in = mp_begin(box, 3, "Weighted functions",
                  "A weighted function gives each part an importance (weight). The weights here are 0.7 for marks and 0.3 for attendance, and they add up to 1.",
                  "Marks are treated as more important than attendance, but attendance still counts.", "P = w1 x + w2 y,    w1 + w2 = 0.7 + 0.3 = 1");
    snprintf(t, sizeof t, "Marks part = 0.7 × %.2f = %.2f      Attendance part = 0.3 × %.2f = %.2f      P = %.2f + %.2f = %.2f", x0, 0.7 * x0, y0, 0.3 * y0, 0.7 * x0, 0.3 * y0, P0);
    formula(in, t);
    mp_end(in, "P is a weighted average of the two percentages, so it always lies between them and between 0 and 100. It is a project-defined analytical score, not the college's official grading system, and it never changes grades, SGPA or pass / fail results.", "The contribution bar of Combined Performance.");
}

static void build_comb(void)
{
    GtkWidget *box = res[S_COMB], *in, *grid;
    int si = stu_sel(c_stu[S_COMB]), has_x, has_y;
    double x0, y0, P0, xw, yw, Pw;
    const char *cls, *ip, *cls2;
    char t[500], v[40];

    if (si < 0) { note(box, "There are no students yet."); return; }
    x0 = comb_actual_x(si); y0 = comb_actual_y(si);
    P0 = perf_score(x0, y0, PERF_WX, PERF_WY);
    xw = gtk_range_get_value(GTK_RANGE(sc_x)); yw = gtk_range_get_value(GTK_RANGE(sc_y));
    Pw = perf_score(xw, yw, PERF_WX, PERF_WY);
    ip = interp(P0, &cls);
    has_x = student_exam_pct(students[si].roll, EXAM_ALL, NULL, NULL) >= 0;
    has_y = att_pct(students[si].roll, NULL) >= 0;
    if (!has_x || !has_y) { ip = "Not enough data"; cls = NULL; }

    snprintf(t, sizeof t, "%s  (roll no. %s)", students[si].name, students[si].roll);
    hint(box, t);
    if (!has_x || !has_y) note(box, "Marks or attendance are missing for this student, so the actual combined score cannot be calculated. The what-if sliders still work with the values you choose.");
    grid = cards_grid(box);
    snprintf(v, sizeof v, has_x ? "%.2f%%" : "-", x0);  add_card(grid, 0, 3, "Overall marks percentage", v, has_x ? (x0 >= 50 ? "good" : "warn") : NULL, "x  (all examinations)");
    snprintf(v, sizeof v, att_pct(students[si].roll, NULL) >= 0 ? "%.2f%%" : "-", y0);
    add_card(grid, 1, 3, "Attendance percentage", v, att_pct(students[si].roll, NULL) >= 0 ? (att_is_low(students[si].roll) ? "warn" : "good") : NULL, "y  (all subjects)");
    snprintf(v, sizeof v, (has_x && has_y) ? "%.2f" : "-", P0);    add_card(grid, 2, 3, "Combined performance score", v, cls, "P(x, y) = 0.7 x + 0.3 y");
    add_card(grid, 3, 3, "Academic performance", ip, cls, "75+ strong, 60+ satisfactory, 40+ needs improvement");
    {
        double yy = att_pct(students[si].roll, NULL);
        cls2 = yy < 0 ? NULL : att_is_low(students[si].roll) ? "warn" : "good";
        add_card(grid, 4, 3, "Attendance status", yy < 0 ? "No records" : att_category_name(att_category(yy)), cls2, yy < 0 ? "" : (att_is_low(students[si].roll) ? "below the attendance threshold" : "at or above the threshold"));
    }
    snprintf(v, sizeof v, "0.7 / 0.3");   add_card(grid, 5, 3, "Weights (marks / attendance)", v, NULL, "fixed in this analysis");

    in = section(box, "What-if analysis", "Move the sliders above to see how a different marks or attendance percentage would change the combined score. Nothing is saved.");
    snprintf(v, sizeof v, "%.1f %%", xw); gtk_label_set_text(GTK_LABEL(l_cx), v);
    snprintf(v, sizeof v, "%.1f %%", yw); gtk_label_set_text(GTK_LABEL(l_cy), v);
    snprintf(t, sizeof t, "What-if values:  marks x = %.1f%%,  attendance y = %.1f%%", xw, yw);
    hint(in, t);
    {
        const char *c2;
        const char *ip2 = interp(Pw, &c2);
        GtkWidget *val;
        snprintf(v, sizeof v, "P = %.2f", Pw);
        val = ui_label(v, "card-value");
        gtk_style_context_add_class(gtk_widget_get_style_context(val), c2);
        gtk_label_set_xalign(GTK_LABEL(val), 0);
        gtk_box_pack_start(GTK_BOX(in), val, FALSE, FALSE, 0);
        snprintf(t, sizeof t, "Interpretation: %s.   Change from the actual score: %+.2f  (= 0.7 × %+.1f + 0.3 × %+.1f).", ip2, Pw - P0, xw - x0, yw - y0);
        hint(in, t);
    }
    contrib_add(in, 0.7 * xw, 0.3 * yw);
    snprintf(t, sizeof t, "Partial derivatives: \xE2\x88\x82P/\xE2\x88\x82x = 0.7 (one more mark percentage point adds 0.7 to P),   \xE2\x88\x82P/\xE2\x88\x82y = 0.3 (one more attendance percentage point adds 0.3).");
    formula(in, t);
    note(in, "This is a project-defined analytical score, NOT the college's official grading system. It does not affect actual grades, SGPA or pass / fail results.");

    comb_math_panel(mth[S_COMB], si, x0, y0, P0, xw, yw, Pw);
}

/* ======================= section 5 : Academic Calculations ======================= */

static void calcs_math_panel(GtkWidget *box, int si, int ns, const double *tot, const double *pct, const double *gp, const int *pass, double gtot, double gmax, double sgpa_num, int sgpa_den, double sgpa, double opct)
{
    GtkWidget *in;
    char t[900], s1[400] = "";
    int j;
    snprintf(t, sizeof t, "All examples use the records of %s (roll no. %s).", students[si].name, students[si].roll);
    hint(box, t);

    in = mp_begin(box, 1, "Arithmetic operations",
                  "Addition, subtraction, multiplication and division are the basic operations used in every calculation of the application.",
                  "Totals are found by adding, percentages by dividing and multiplying, and grade points by multiplying credits with points.", "subject total = CIE 1 + CIE 2 + SEE        percentage = total / maximum \xC3\x97 100");
    snprintf(t, sizeof t, "%s:  %.1f + %.1f + %.1f = %.1f  (out of %.0f),  percentage = %.1f / %.0f \xC3\x97 100 = %.2f%%.", subjects[0].name,
             marks_find(students[si].roll, subjects[0].code, 0) >= 0 ? marks[marks_find(students[si].roll, subjects[0].code, 0)].marks : 0.0,
             n_exams > 1 && marks_find(students[si].roll, subjects[0].code, 1) >= 0 ? marks[marks_find(students[si].roll, subjects[0].code, 1)].marks : 0.0,
             n_exams > 2 && marks_find(students[si].roll, subjects[0].code, 2) >= 0 ? marks[marks_find(students[si].roll, subjects[0].code, 2)].marks : 0.0,
             tot[0], subjects[0].max_marks, tot[0], subjects[0].max_marks, pct[0]);
    formula(in, t);
    mp_end(in, "Each subject total is the sum of its three examination marks.", "Total marks, subject totals and every percentage in ClassMetrics.");

    in = mp_begin(box, 2, "Percentages",
                  "A percentage expresses a part as a number out of 100: part divided by whole, multiplied by 100.",
                  "Percentages let us compare marks and attendance of different sizes and decide grades with one set of limits.", "percentage = ( part / whole ) \xC3\x97 100");
    snprintf(t, sizeof t, "Overall: %.1f / %.0f \xC3\x97 100 = %.2f%%.", gtot, gmax, opct);
    formula(in, t);
    mp_end(in, "The overall percentage is the part of the total possible marks the student earned. It selects the grade scale position; subject grades use each subject's own percentage.", "Overall percentage, subject grades, attendance percentage and the class pass percentage.");

    in = mp_begin(box, 3, "Weighted averages",
                  "In a weighted average every value counts according to its weight. A subject with more credits has more influence on the SGPA.",
                  "A 4-credit subject should matter more than a 1-credit subject, so SGPA is a weighted average of grade points.", "SGPA = \xCE\xA3 ( credit \xC3\x97 grade points ) / \xCE\xA3 credits");
    for (j = 0; j < ns; j++) {
        char x[48];
        if (subjects[j].credits <= 0) continue;
        snprintf(x, sizeof x, "%s%d\xC3\x97%.0f", s1[0] ? " + " : "", subjects[j].credits, pass[j] ? gp[j] : 0.0);
        if (strlen(s1) + strlen(x) < sizeof s1) strcat(s1, x);
    }
    snprintf(t, sizeof t, "SGPA = (%s) / %d = %.0f / %d = %.2f", s1, sgpa_den, sgpa_num, sgpa_den, sgpa);
    formula(in, t);
    mp_end(in, "A subject with zero credits (such as Environmental Science) is left out of the SGPA; a failed credit subject contributes 0 grade points but its credits stay in the denominator.", "SGPA and earned credits in this section, the Marks sheet and View Student.");

    in = mp_begin(box, 4, "Summation",
                  "Summation (the symbol \xCE\xA3) means adding a list of numbers one after another.",
                  "Totals over subjects, credits, and grade points are all sums.", "total marks = \xCE\xA3 over all subjects of the subject total");
    s1[0] = '\0';
    for (j = 0; j < ns; j++) { char x[24]; snprintf(x, sizeof x, "%s%.1f", j ? " + " : "", tot[j]); if (strlen(s1) + strlen(x) < sizeof s1) strcat(s1, x); }
    snprintf(t, sizeof t, "%s = %.1f   (out of %.0f)", s1, gtot, gmax);
    formula(in, t);
    mp_end(in, "The same summation idea gives the total credits and the sum of credit \xC3\x97 grade-point products used in the SGPA.", "Every total shown by the application.");
}

static void build_calcs(void)
{
    GtkWidget *box = res[S_CALC], *in, *grid, *g;
    int si = stu_sel(c_stu[S_CALC]), j, earned = 0, ccount = 0, npass = 0, ns = n_subjects;
    const char *roll;
    double tot[MAX_SUBJECTS], pct[MAX_SUBJECTS], gp[MAX_SUBJECTS], gtot = 0, gmax = 0, num = 0, opct, sgpa, att;
    int pass[MAX_SUBJECTS];
    char t[900], v[40], x[200], leg[300];

    if (si < 0) { note(box, "There are no students yet."); return; }
    if (n_subjects == 0) { note(box, "There are no subjects yet."); return; }
    roll = students[si].roll;
    for (j = 0; j < ns; j++) {
        double mx = 0;
        tot[j] = 0;
        subject_total(roll, j, &tot[j], &mx);
        pct[j] = subject_pct(roll, j);
        if (pct[j] < 0) pct[j] = 0;
        gp[j] = grade_points_of(pct[j]);
        pass[j] = subject_pass(roll, j);
        gtot += tot[j]; gmax += subjects[j].max_marks;
        if (pass[j]) npass++;
        if (subjects[j].credits > 0) { ccount += subjects[j].credits; if (pass[j]) { earned += subjects[j].credits; num += subjects[j].credits * gp[j]; } }
    }
    opct = student_exam_pct(roll, EXAM_ALL, NULL, NULL);
    if (opct < 0) {
        snprintf(t, sizeof t, "%s  (roll no. %s)", students[si].name, roll);
        hint(box, t);
        note(box, "No marks are recorded for this student, so there is nothing to calculate yet.");
        hint(mth[S_CALC], "The mathematics needs recorded marks. Select a student with marks.");
        return;
    }
    sgpa = student_sgpa(roll, NULL, NULL);
    att = att_pct(roll, NULL);
    snprintf(t, sizeof t, "%s  (roll no. %s)", students[si].name, roll);
    hint(box, t);

    grid = cards_grid(box);
    snprintf(v, sizeof v, "%.1f / %.0f", gtot, gmax);        add_card(grid, 0, 3, "Total marks", v, NULL, "sum of the subject totals");
    snprintf(v, sizeof v, opct >= 0 ? "%.2f%%" : "-", opct); add_card(grid, 1, 3, "Overall percentage", v, opct >= 50 ? "good" : "warn", "total / maximum × 100");
    snprintf(v, sizeof v, sgpa >= 0 ? "%.2f" : "-", sgpa);   add_card(grid, 2, 3, "SGPA", v, sgpa >= 6 ? "good" : "warn", "out of 10, weighted by credits");
    snprintf(v, sizeof v, "%d / %d", earned, ccount);        add_card(grid, 3, 3, "Earned credits", v, earned == ccount ? "good" : "warn", "credits of the passed subjects");
    add_card(grid, 4, 3, "Result", student_exam_pass(roll, EXAM_ALL) ? "Pass" : "Fail", student_exam_pass(roll, EXAM_ALL) ? "good" : "warn", "every subject must be passed");
    snprintf(v, sizeof v, att >= 0 ? "%.2f%%" : "-", att);   add_card(grid, 5, 3, "Attendance", v, att >= 0 ? (att_is_low(roll) ? "warn" : "good") : NULL, "attended / held × 100");

    in = section(box, "Step 1 - Subject totals, percentages, grades and grade points", "Each subject total is CIE 1 + CIE 2 + SEE (out of 100). The grade comes from the subject percentage; the grade points from the grade.");
    {
        char h[3][16];
        const char *heads[11];
        for (j = 0; j < 3; j++) { if (j < n_exams) snprintf(h[j], sizeof h[j], "%s", exams[j].name); else snprintf(h[j], sizeof h[j], "-"); }
        heads[0] = "Subject"; heads[1] = h[0]; heads[2] = h[1]; heads[3] = h[2]; heads[4] = "Total"; heads[5] = "%"; heads[6] = "Grade"; heads[7] = "Points"; heads[8] = "Credits"; heads[9] = "Cr × Pts"; heads[10] = "Result";
        g = tbl_new(in, 11, heads, 1);
        for (j = 0; j < ns; j++) {
            char m[3][12], c4[12], c5[12], c6[8], c7[8], c8[8], c9[12];
            int e, k;
            for (e = 0; e < 3; e++) { k = e < n_exams ? marks_find(roll, subjects[j].code, e) : -1; if (k >= 0) snprintf(m[e], sizeof m[e], "%.1f", marks[k].marks); else snprintf(m[e], sizeof m[e], "-"); }
            snprintf(c4, sizeof c4, "%.1f", tot[j]); snprintf(c5, sizeof c5, "%.1f", pct[j]); snprintf(c6, sizeof c6, "%s", grade_of(pct[j]));
            snprintf(c7, sizeof c7, "%.0f", pass[j] ? gp[j] : 0.0); snprintf(c8, sizeof c8, "%d", subjects[j].credits); snprintf(c9, sizeof c9, "%.0f", pass[j] && subjects[j].credits > 0 ? subjects[j].credits * gp[j] : 0.0);
            tbl_add(g, pass[j] ? NULL : "warn", sname(j), m[0], m[1], m[2], c4, c5, c6, c7, c8, c9, pass[j] ? "Pass" : "Fail");
        }
    }
    grade_legend(leg, sizeof leg);
    snprintf(t, sizeof t, "Grades: %s.  A subject is passed when its total is at least its pass marks.", leg);
    hint(in, t);

    in = section(box, "Step 2 - Total marks and overall percentage", NULL);
    snprintf(x, sizeof x, "Total marks = sum of the %d subject totals = %.1f out of %.0f", ns, gtot, gmax);
    formula(in, x);
    snprintf(x, sizeof x, "Percentage = %.1f / %.0f \xC3\x97 100 = %.2f%%", gtot, gmax, gmax > 0 ? gtot * 100.0 / gmax : 0.0);
    formula(in, x);

    in = section(box, "Step 3 - SGPA and earned credits", "SGPA is a weighted average: every subject counts according to its credits. Zero-credit subjects are excluded.");
    {
        char s1[300] = "";
        for (j = 0; j < ns; j++) { char y[40]; if (subjects[j].credits <= 0) continue; snprintf(y, sizeof y, "%s%d\xC3\x97%.0f", s1[0] ? " + " : "", subjects[j].credits, pass[j] ? gp[j] : 0.0); if (strlen(s1) + strlen(y) < sizeof s1) strcat(s1, y); }
        snprintf(t, sizeof t, "SGPA = ( %s ) / %d = %.0f / %d = %.2f", s1, ccount, num, ccount, ccount > 0 ? num / ccount : 0.0);
        formula(in, t);
    }
    snprintf(t, sizeof t, "Earned credits = credits of the passed subjects = %d out of %d.", earned, ccount);
    hint(in, t);
    if (sgpa >= 0 && ccount > 0 && fabs(sgpa - num / ccount) > 0.005) note(in, "The SGPA value of the application differs from this hand calculation - please check the subject data.");

    in = section(box, "Step 4 - Pass / Fail and attendance", NULL);
    snprintf(t, sizeof t, "Rule: every subject must be passed.  Subjects passed: %d of %d.  Overall result: %s.", npass, ns, student_exam_pass(roll, EXAM_ALL) ? "PASS" : "FAIL");
    formula(in, t);
    {
        int T, A;
        att_stats(roll, NULL, &T, &A);
        if (T > 0) snprintf(t, sizeof t, "Attendance = attended / held \xC3\x97 100 = %d / %d \xC3\x97 100 = %.2f%%.   %s", A, T, att, att_is_low(roll) ? "Below the attendance threshold." : "At or above the attendance threshold.");
        else snprintf(t, sizeof t, "No attendance records for this student.");
        formula(in, t);
    }
    note(in, "These calculations reuse the grading system, subject credits and pass / fail rules of ClassMetrics. Nothing here changes the stored data.");

    calcs_math_panel(mth[S_CALC], si, ns, tot, pct, gp, pass, gtot, gmax, num, ccount, ccount > 0 ? num / ccount : 0.0, gmax > 0 ? gtot * 100.0 / gmax : 0.0);
}

/* ======================= section 6 : Class Performance ======================= */

static void build_class(void)
{
    GtkWidget *box = res[S_CLASS], *in, *grid, *g;
    int e = ui_exam_sel(c_cexam, 1), order[MAX_STUDENTS], n, i, j, cnt[7] = { 0, 0, 0, 0, 0, 0, 0 }, nat = 0;
    double pc[MAX_STUDENTS], avg, hi = 0, lo = 0, sum = 0, passp, sg;
    char t[600], v[48];
    Hist H;
    const char *labs[MAX_SUBJECTS];
    double sv[MAX_SUBJECTS];
    char lb[MAX_SUBJECTS][16];

    if (n_students == 0) { note(box, "There are no students yet."); return; }
    if (e == -2) e = EXAM_ALL;
    n = exam_ranking(e, order, pc);
    if (n == 0) { note(box, "No marks are recorded yet."); return; }
    for (i = 0; i < n; i++) sum += pc[i];
    avg = class_avg_pct(e); hi = pc[0]; lo = pc[n - 1];
    passp = class_pass_pct(e); sg = class_avg_sgpa();
    snprintf(t, sizeof t, "Examination: %s   |   %d students with marks", e >= 0 ? exams[e].name : "all examinations (overall total)", n);
    hint(box, t);

    grid = cards_grid(box);
    snprintf(v, sizeof v, "%.2f%%", avg);     add_card(grid, 0, 3, "Class average", v, avg >= 50 ? "good" : "warn", "arithmetic mean of the percentages");
    snprintf(v, sizeof v, "%.2f%%", hi);      add_card(grid, 1, 3, "Highest marks", v, "good", students[order[0]].name);
    snprintf(v, sizeof v, "%.2f%%", lo);      add_card(grid, 2, 3, "Lowest marks", v, lo >= 40 ? NULL : "warn", students[order[n - 1]].name);
    snprintf(v, sizeof v, "%.1f%%", passp);   add_card(grid, 3, 3, "Class pass percentage", v, passp >= 60 ? "good" : "warn", "students who passed every subject");
    snprintf(v, sizeof v, sg >= 0 ? "%.2f" : "-", sg); add_card(grid, 4, 3, "Average SGPA", v, NULL, "out of 10");
    snprintf(v, sizeof v, "%d", n_students);  add_card(grid, 5, 3, "Students", v, NULL, "in the class list");

    for (j = 0; j < n_subjects && j < MAX_SUBJECTS; j++) { sv[j] = subject_avg_pct(j, e); snprintf(lb[j], sizeof lb[j], "%.8s", sname(j)); labs[j] = lb[j]; if (sv[j] < 0) sv[j] = 0; }
    in = section(box, "Subject-wise averages", "Average percentage of the class in every subject. Green = at or above the pass percentage, red = below.");
    chart_add(in, 0, n_subjects, labs, sv, n_subjects > 0 && subjects[0].max_marks > 0 ? subjects[0].pass_marks * 100.0 / subjects[0].max_marks : 40, "Class average % per subject", 240);

    for (i = 0; i < n; i++) cnt[grade_index(pc[i])]++;
    memset(&H, 0, sizeof H);
    H.n = 7;
    for (i = 0; i < 7; i++) { H.val[i] = cnt[i]; snprintf(H.lab[i], sizeof H.lab[i], "%s", grade_name(i)); }
    snprintf(H.title, sizeof H.title, "Grade distribution (students per grade of the overall percentage)");
    in = section(box, "Grade distribution", "How many students fall into each grade band. The last bar (F) is the fail class.");
    hist_chart(in, &H, 230);

    in = section(box, "Top-performing students", NULL);
    {
        const char *heads[5] = { "Rank", "Student", "Percentage", "SGPA", "Result" };
        g = tbl_new(in, 5, heads, 2);
        for (i = 0; i < n && i < 5; i++) {
            char a[8], b[56], c[16], d[16];
            double sgp = student_sgpa(students[order[i]].roll, NULL, NULL);
            snprintf(a, sizeof a, "%d", student_rank(students[order[i]].roll, e)); snprintf(b, sizeof b, "%s", students[order[i]].name);
            snprintf(c, sizeof c, "%.2f%%", pc[i]); snprintf(d, sizeof d, sgp >= 0 ? "%.2f" : "-", sgp);
            tbl_add(g, "good", a, b, c, d, student_exam_pass(students[order[i]].roll, EXAM_ALL) ? "Pass" : "Fail");
        }
    }

    in = section(box, "Students requiring academic attention", "A student who failed at least one subject or has an overall percentage below 50%. Lowest percentages first.");
    {
        const char *heads[5] = { "Student", "Percentage", "Subjects failed", "Attendance", "Reason" };
        int shown = 0;
        g = tbl_new(in, 5, heads, 1);
        for (i = n - 1; i >= 0; i--) {
            const char *roll = students[order[i]].roll;
            int fails = 0, pass = student_exam_pass(roll, EXAM_ALL);
            double ap = att_pct(roll, NULL), op = student_exam_pct(roll, EXAM_ALL, NULL, NULL);
            for (j = 0; j < n_subjects; j++) if (subject_pct(roll, j) >= 0 && !subject_pass(roll, j)) fails++;
            if (pass && op >= 50) continue;
            nat++;
            if (shown >= 8) continue;
            {
                char a[56], b[16], c[8], d[16], r[64];
                snprintf(a, sizeof a, "%s", students[order[i]].name); snprintf(b, sizeof b, "%.2f%%", op); snprintf(c, sizeof c, "%d", fails);
                snprintf(d, sizeof d, ap >= 0 ? "%.1f%%" : "-", ap);
                snprintf(r, sizeof r, "%s%s", !pass ? "failed subject" : "", op < 50 ? (pass ? "below 50%" : ", below 50%") : "");
                tbl_add(g, "warn", a, b, c, d, r);
                shown++;
            }
        }
        if (nat == 0) hint(in, "No student currently requires academic attention.");
        else if (nat > shown) { snprintf(t, sizeof t, "... and %d more student%s (see Reports > Class performance).", nat - shown, nat - shown == 1 ? "" : "s"); hint(in, t); }
    }

    /* ---- Mathematics Behind This Analysis ---- */
    {
        GtkWidget *mb = mth[S_CLASS], *inn;
        char f[300];
        snprintf(t, sizeof t, "All examples use the class records for: %s.", e >= 0 ? exams[e].name : "all examinations");
        hint(mb, t);
        inn = mp_begin(mb, 1, "Arithmetic mean",
                       "The arithmetic mean is the sum of the values divided by how many values there are.",
                       "The class average summarises the whole class in one number and is the base for comparing a student with the class.", "mean = ( x1 + x2 + ... + xN ) / N");
        snprintf(t, sizeof t, "mean = %.2f / %d = %.2f%%     (sum of the %d percentages = %.2f)", sum, n, sum / n, n, sum);
        formula(inn, t);
        mp_end(inn, avg >= 50 ? "The class average is above 50%, so the class is performing satisfactorily on average." : "The class average is below 50%: the class needs support.", "Class average card; subject-wise averages are the mean of each subject.");

        inn = mp_begin(mb, 2, "Maximum and minimum",
                       "The maximum is the largest value in a list and the minimum is the smallest.",
                       "They show the best and the weakest result in the class and the spread between them.", "max = largest percentage,   min = smallest percentage,   range = max - min");
        snprintf(t, sizeof t, "max = %.2f%% (%s),   min = %.2f%% (%s),   range = %.2f - %.2f = %.2f percentage points.", hi, students[order[0]].name, lo, students[order[n - 1]].name, hi, lo, hi - lo);
        formula(inn, t);
        mp_end(inn, "A wide range means large differences between students; the minimum identifies who needs support.", "Highest marks, lowest marks and the ranking tables.");

        inn = mp_begin(mb, 3, "Percentages",
                       "A percentage is part divided by whole, multiplied by 100.",
                       "The pass percentage tells what share of the class passed every subject.", "pass percentage = ( students who passed all subjects / all students ) \xC3\x97 100");
        {
            int pn = 0;
            for (i = 0; i < n; i++) if (student_exam_pass(students[order[i]].roll, e)) pn++;
            snprintf(t, sizeof t, "pass percentage = %d / %d \xC3\x97 100 = %.1f%%", pn, n, n ? pn * 100.0 / n : 0.0);
            formula(inn, t);
        }
        mp_end(inn, passp >= 60 ? "More than half of the class passed every subject." : "Fewer than 60% of the class passed every subject.", "Class pass percentage card and the dashboard.");

        snprintf(f, sizeof f, "relative frequency = frequency / N       (frequency = number of students in a grade band)");
        inn = mp_begin(mb, 4, "Frequency distribution",
                       "A frequency distribution counts how many values fall into each group (here, each grade).",
                       "It shows the shape of the results: whether most students are in high grades, in the middle, or failing.", f);
        {
            const char *hd[3] = { "Grade", "Students (frequency)", "Relative frequency" };
            GtkWidget *tg = tbl_new(inn, 3, hd, 1);
            for (i = 0; i < 7; i++) { char a[8], b[8], c[16]; snprintf(a, sizeof a, "%s", grade_name(i)); snprintf(b, sizeof b, "%d", cnt[i]); snprintf(c, sizeof c, "%.1f%%", n ? cnt[i] * 100.0 / n : 0.0); tbl_add(tg, i == 6 ? "warn" : NULL, a, b, c); }
        }
        mp_end(inn, "The frequencies add up to the number of students; the bar chart above draws exactly these counts.", "Grade distribution chart.");
    }
}

/* ======================= scaffold, home page, refresh ======================= */

static void rebuild_section(int s)
{
    if (s <= S_HOME || s >= S_COUNT || !res[s]) return;
    clear_box(res[s]);
    clear_box(mth[s]);
    switch (s) {
        case S_MARKS:  build_marks(); break;
        case S_TRENDS: build_trends(); break;
        case S_ATT:    build_att(); break;
        case S_COMB:   build_comb(); break;
        case S_CALC:   build_calcs(); break;
        default:       build_class(); break;
    }
    gtk_widget_show_all(res[s]);
    gtk_widget_show_all(mth[s]);
}

static GtkWidget *combo_new(const char *a, const char *b, const char *c, int sec)
{
    GtkWidget *cb = gtk_combo_box_text_new();
    if (a) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cb), a);
    if (b) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cb), b);
    if (c) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cb), c);
    if (a) gtk_combo_box_set_active(GTK_COMBO_BOX(cb), 0);
    g_signal_connect(cb, "changed", G_CALLBACK(on_change), GINT_TO_POINTER(sec));
    return cb;
}

static GtkWidget *scale_new(double lo, double hi, double val)
{
    GtkWidget *sc = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, lo, hi, 1);
    gtk_scale_set_digits(GTK_SCALE(sc), 0);
    gtk_scale_set_draw_value(GTK_SCALE(sc), FALSE);
    gtk_range_set_value(GTK_RANGE(sc), val);
    gtk_widget_set_size_request(sc, 180, -1);
    gtk_widget_set_hexpand(sc, TRUE);
    return sc;
}

static void on_scope(GtkWidget *w, gpointer d)
{
    (void)w; (void)d;
    gtk_widget_set_sensitive(c_stu[S_MARKS], gtk_combo_box_get_active(GTK_COMBO_BOX(c_scope)) == 0);
    if (!busy) rebuild_section(S_MARKS);
}

static GtkWidget *make_section(int s, const char *title, const char *sub, GtkWidget *controls)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12), *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14), *ex, *inner, *card;
    gtk_box_pack_start(GTK_BOX(top), ui_button("\xE2\x86\x90  Back to Analyzer", NULL, G_CALLBACK(on_back), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(top), ui_title(title, sub), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(page), top, FALSE, FALSE, 0);
    if (controls) {
        card = ui_card(&inner);
        gtk_box_pack_start(GTK_BOX(inner), controls, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(page), card, FALSE, FALSE, 0);
    }
    res[s] = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_pack_start(GTK_BOX(page), res[s], FALSE, FALSE, 0);
    card = ui_card(&inner);
    ex = gtk_expander_new(NULL);
    gtk_expander_set_label_widget(GTK_EXPANDER(ex), ui_label("Mathematics Behind This Analysis", "card-head"));
    gtk_expander_set_expanded(GTK_EXPANDER(ex), FALSE);                    /* collapsed by default */
    mth[s] = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(ex), mth[s]);
    gtk_box_pack_start(GTK_BOX(inner), ex, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), card, FALSE, FALSE, 0);
    gtk_stack_add_named(GTK_STACK(stack), page, SEC_NAME[s]);
    return page;
}

static GtkWidget *control_row(void) { GtkWidget *r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12); return r; }

static GtkWidget *home_card(int sec, int icon, const char *fg, const char *bg, const char *title, const char *desc)
{
    GtkWidget *inner, *card = ui_card(&inner), *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12), *b;
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "stat-card");
    gtk_box_pack_start(GTK_BOX(top), ui_icon_new(icon, 46, fg, bg), FALSE, FALSE, 0);
    {
        GtkWidget *tl = wrap_label(title, "card-head"), *dl = wrap_label(desc, "hint");
        gtk_label_set_max_width_chars(GTK_LABEL(tl), 22);
        gtk_label_set_max_width_chars(GTK_LABEL(dl), 34);
        gtk_box_pack_start(GTK_BOX(top), tl, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(inner), top, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(inner), dl, TRUE, TRUE, 4);
    }
    b = ui_button("Explore Analysis", "accent", G_CALLBACK(on_open), GINT_TO_POINTER(sec));
    gtk_widget_set_halign(b, GTK_ALIGN_START);
    gtk_box_pack_end(GTK_BOX(inner), b, FALSE, FALSE, 4);
    gtk_widget_set_hexpand(card, TRUE);
    return card;
}

static GtkWidget *build_home(void)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14), *fb = gtk_flow_box_new();
    gtk_box_pack_start(GTK_BOX(page), ui_title("Mathematical Analyzer", "Explore, compare and understand academic performance through mathematics."), FALSE, FALSE, 0);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(fb), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(fb), TRUE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(fb), 1);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(fb), 3);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(fb), 14);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(fb), 14);
    gtk_container_add(GTK_CONTAINER(fb), home_card(S_MARKS, ICON_MARKS, "#6842F5", "#EDE9FE", "Marks Comparison", "Compare marks across subjects and examinations."));
    gtk_container_add(GTK_CONTAINER(fb), home_card(S_TRENDS, ICON_PERFORMANCE, "#2563EB", "#DBEAFE", "Performance Trends", "Understand academic improvement and decline."));
    gtk_container_add(GTK_CONTAINER(fb), home_card(S_ATT, ICON_ATTENDANCE, "#16A34A", "#DCFCE7", "Attendance Predictor", "Calculate the classes required to achieve an attendance target."));
    gtk_container_add(GTK_CONTAINER(fb), home_card(S_COMB, ICON_MATH, "#7C3AED", "#EDE9FE", "Combined Performance", "Analyse marks and attendance together."));
    gtk_container_add(GTK_CONTAINER(fb), home_card(S_CALC, ICON_CREDITS, "#F59E0B", "#FEF3C7", "Academic Calculations", "Understand percentages, grades, SGPA and credits."));
    gtk_container_add(GTK_CONTAINER(fb), home_card(S_CLASS, ICON_TROPHY, "#0E9AA7", "#CCFBF1", "Class Performance", "Analyse class averages, rankings and subject-wise performance."));
    gtk_box_pack_start(GTK_BOX(page), fb, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), wrap_label("Every analysis uses the real student records stored in ClassMetrics and updates automatically when they change. Each section explains its mathematics in a panel at the bottom.", "hint"), FALSE, FALSE, 0);
    return page;
}

GtkWidget *math_build(void)
{
    GtkWidget *r;

    stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_stack_set_hhomogeneous(GTK_STACK(stack), FALSE);
    gtk_stack_set_vhomogeneous(GTK_STACK(stack), FALSE);
    gtk_stack_add_named(GTK_STACK(stack), build_home(), SEC_NAME[S_HOME]);

    c_stu[S_MARKS] = combo_new(NULL, NULL, NULL, S_MARKS);
    c_stu[S_TRENDS] = combo_new(NULL, NULL, NULL, S_TRENDS);
    c_stu[S_ATT] = combo_new(NULL, NULL, NULL, S_ATT);
    c_stu[S_CALC] = combo_new(NULL, NULL, NULL, S_CALC);
    c_stu[S_COMB] = gtk_combo_box_text_new();
    g_signal_connect(c_stu[S_COMB], "changed", G_CALLBACK(on_comb_student), NULL);

    /* 1 Marks Comparison */
    c_scope = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c_scope), "Selected student");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c_scope), "Whole class (average)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(c_scope), 0);
    g_signal_connect(c_scope, "changed", G_CALLBACK(on_scope), NULL);
    c_msub = combo_new(NULL, NULL, NULL, S_MARKS);
    c_mmode = combo_new("CIE 1 vs CIE 2", "CIE 1 + CIE 2 combined", "All examinations (as %)", S_MARKS);
    r = control_row();
    gtk_box_pack_start(GTK_BOX(r), ui_labeled("Show", c_scope), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(r), ui_labeled("Student", c_stu[S_MARKS]), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(r), ui_labeled("Subject", c_msub), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(r), ui_labeled("Comparison", c_mmode), FALSE, FALSE, 0);
    make_section(S_MARKS, "Marks Comparison", "Compare marks across subjects and examinations.", r);

    /* 2 Performance Trends */
    r = control_row();
    gtk_box_pack_start(GTK_BOX(r), ui_labeled("Student", c_stu[S_TRENDS]), FALSE, FALSE, 0);
    make_section(S_TRENDS, "Performance Trends", "Understand academic improvement and decline.", r);

    /* 3 Attendance Predictor */
    c_asub = combo_new(NULL, NULL, NULL, S_ATT);
    sc_target = scale_new(1, 100, 75);
    sc_future = scale_new(0, 100, 10);
    l_target = ui_label("75 %", "card-label"); l_future = ui_label("10 classes", "card-label");
    g_signal_connect(sc_target, "value-changed", G_CALLBACK(on_att_scale), NULL);
    g_signal_connect(sc_future, "value-changed", G_CALLBACK(on_att_scale), NULL);
    r = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(r), 16); gtk_grid_set_row_spacing(GTK_GRID(r), 10);
    gtk_grid_attach(GTK_GRID(r), ui_labeled("Student", c_stu[S_ATT]), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(r), ui_labeled("Subject", c_asub), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(r), ui_label("Required attendance target", "field-label"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(r), sc_target, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(r), l_target, 2, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(r), ui_label("Future classes attended (x)", "field-label"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(r), sc_future, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(r), l_future, 2, 2, 1, 1);
    make_section(S_ATT, "Attendance Predictor", "Calculate the classes required to achieve an attendance target.", r);

    /* 4 Combined Performance */
    sc_x = scale_new(0, 100, 50);
    sc_y = scale_new(0, 100, 50);
    g_signal_connect(sc_x, "value-changed", G_CALLBACK(on_change), GINT_TO_POINTER(S_COMB));
    g_signal_connect(sc_y, "value-changed", G_CALLBACK(on_change), GINT_TO_POINTER(S_COMB));
    r = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(r), 16); gtk_grid_set_row_spacing(GTK_GRID(r), 10);
    gtk_grid_attach(GTK_GRID(r), ui_labeled("Student", c_stu[S_COMB]), 0, 0, 1, 1);
    { GtkWidget *rb = ui_button("Reset to actual values", NULL, G_CALLBACK(on_comb_reset), NULL); gtk_widget_set_halign(rb, GTK_ALIGN_START); gtk_grid_attach(GTK_GRID(r), rb, 1, 0, 1, 1); }
    l_cx = ui_label("", "card-label"); l_cy = ui_label("", "card-label");
    gtk_grid_attach(GTK_GRID(r), ui_label("What-if marks percentage (x)", "field-label"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(r), sc_x, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(r), l_cx, 2, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(r), ui_label("What-if attendance percentage (y)", "field-label"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(r), sc_y, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(r), l_cy, 2, 2, 1, 1);
    make_section(S_COMB, "Combined Performance", "Analyse marks and attendance together.", r);

    /* 5 Academic Calculations */
    r = control_row();
    gtk_box_pack_start(GTK_BOX(r), ui_labeled("Student", c_stu[S_CALC]), FALSE, FALSE, 0);
    make_section(S_CALC, "Academic Calculations", "Understand percentages, grades, SGPA and credits.", r);

    /* 6 Class Performance */
    c_cexam = gtk_combo_box_text_new();
    g_signal_connect(c_cexam, "changed", G_CALLBACK(on_change), GINT_TO_POINTER(S_CLASS));
    r = control_row();
    gtk_box_pack_start(GTK_BOX(r), ui_labeled("Examination", c_cexam), FALSE, FALSE, 0);
    make_section(S_CLASS, "Class Performance", "Analyse class averages, rankings and subject-wise performance.", r);

    gtk_widget_show_all(stack);
    gtk_stack_set_visible_child_name(GTK_STACK(stack), SEC_NAME[S_HOME]);
    return stack;
}

static int shot_scroll = 0;
static gboolean shot_scroll_idle(gpointer d)
{
    GtkWidget *sw = gtk_widget_get_ancestor(stack, GTK_TYPE_SCROLLED_WINDOW);
    static int tries = 0;
    (void)d;
    if (sw) {
        GtkAdjustment *a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sw));
        gtk_adjustment_set_value(a, shot_scroll);
        if (gtk_adjustment_get_value(a) >= shot_scroll - 1 || gtk_adjustment_get_upper(a) - gtk_adjustment_get_page_size(a) < shot_scroll - 1) { if (++tries > 12 || gtk_adjustment_get_value(a) >= shot_scroll - 1) return G_SOURCE_REMOVE; }
    }
    return G_SOURCE_CONTINUE;
}

static int shot_dialog_flag = 0;
static gboolean shot_open_dialog(gpointer d) { (void)d; on_full_matrix(NULL, NULL); return G_SOURCE_REMOVE; }
static void shot_map(GtkWidget *w, gpointer d)
{
    (void)w; (void)d;
    if (shot_scroll > 0) g_timeout_add(150, shot_scroll_idle, NULL);
    if (shot_dialog_flag) g_timeout_add(500, shot_open_dialog, NULL);
}

/* screenshot aid (like CLASSMETRICS_TAB): CLASSMETRICS_MATH_STATE="scope=1;mode=2;mstu=3;target=80;..." sets the controls */
static void apply_shot_state(void)
{
    const char *st = getenv("CLASSMETRICS_MATH_STATE");
    char buf[400], *tok, *save = NULL;
    if (!st || !getenv("CLASSMETRICS_SHOT_DIR")) return;
    snprintf(buf, sizeof buf, "%s", st);
    for (tok = strtok_r(buf, ";", &save); tok; tok = strtok_r(NULL, ";", &save)) {
        char *eq = strchr(tok, '=');
        int v;
        if (!eq) continue;
        *eq = ' '; v = atoi(eq + 1);
        if (!strcmp(tok, "scope")) gtk_combo_box_set_active(GTK_COMBO_BOX(c_scope), v);
        else if (!strcmp(tok, "mode")) gtk_combo_box_set_active(GTK_COMBO_BOX(c_mmode), v);
        else if (!strcmp(tok, "msub")) gtk_combo_box_set_active(GTK_COMBO_BOX(c_msub), v);
        else if (!strcmp(tok, "mstu")) gtk_combo_box_set_active(GTK_COMBO_BOX(c_stu[S_MARKS]), v);
        else if (!strcmp(tok, "tstu")) gtk_combo_box_set_active(GTK_COMBO_BOX(c_stu[S_TRENDS]), v);
        else if (!strcmp(tok, "astu")) gtk_combo_box_set_active(GTK_COMBO_BOX(c_stu[S_ATT]), v);
        else if (!strcmp(tok, "asub")) gtk_combo_box_set_active(GTK_COMBO_BOX(c_asub), v);
        else if (!strcmp(tok, "target")) gtk_range_set_value(GTK_RANGE(sc_target), v);
        else if (!strcmp(tok, "future")) gtk_range_set_value(GTK_RANGE(sc_future), v);
        else if (!strcmp(tok, "kstu")) gtk_combo_box_set_active(GTK_COMBO_BOX(c_stu[S_CALC]), v);
        else if (!strcmp(tok, "exam")) gtk_combo_box_set_active(GTK_COMBO_BOX(c_cexam), v);
        else if (!strcmp(tok, "cstu")) { gtk_combo_box_set_active(GTK_COMBO_BOX(c_stu[S_COMB]), v); comb_reset(); }
        else if (!strcmp(tok, "cx")) gtk_range_set_value(GTK_RANGE(sc_x), v);
        else if (!strcmp(tok, "cy")) gtk_range_set_value(GTK_RANGE(sc_y), v);
        else if (!strcmp(tok, "scroll")) { shot_scroll = v; if (!shot_dialog_flag) g_signal_connect(stack, "map", G_CALLBACK(shot_map), NULL); }
        else if (!strcmp(tok, "dialog")) { shot_dialog_flag = 1; shot_dtab = v; g_signal_connect(stack, "map", G_CALLBACK(shot_map), NULL); }
        else if (!strcmp(tok, "open")) { GtkWidget *ex = gtk_widget_get_parent(mth[S_COUNT > v && v > 0 ? v : S_MARKS]); if (GTK_IS_EXPANDER(ex)) gtk_expander_set_expanded(GTK_EXPANDER(ex), TRUE); }
    }
    on_att_scale(NULL, NULL);
}

void math_refresh(void)
{
    int s;
    busy++;
    for (s = S_MARKS; s <= S_CALC; s++) ui_combo_students(GTK_COMBO_BOX_TEXT(c_stu[s]));
    ui_combo_subjects(GTK_COMBO_BOX_TEXT(c_msub), 1);
    ui_combo_subjects(GTK_COMBO_BOX_TEXT(c_asub), 1);
    ui_combo_exams_ex(GTK_COMBO_BOX_TEXT(c_cexam), 1);
    on_att_scale(NULL, NULL);                      /* the labels beside the sliders */
    busy--;
    comb_reset();                                  /* what-if sliders start at the real values */
    busy++;
    if (first_show) apply_shot_state();
    gtk_widget_set_sensitive(c_stu[S_MARKS], gtk_combo_box_get_active(GTK_COMBO_BOX(c_scope)) == 0);
    busy--;
    if (first_show) {                              /* screenshot aid: CLASSMETRICS_TAB = 0 (home) ... 6 (section) */
        const char *tb = getenv("CLASSMETRICS_TAB");
        first_show = 0;
        if (tb && getenv("CLASSMETRICS_SHOT_DIR")) { s = atoi(tb); if (s >= 0 && s < S_COUNT) { cur_sec = s; show_section(s); return; } }
    }
    if (cur_sec != S_HOME) rebuild_section(cur_sec);
}
