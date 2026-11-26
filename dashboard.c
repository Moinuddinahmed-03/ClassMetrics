/*
 * dashboard.c : ClassMetrics 2.0 dashboard (bento grid).
 * Every number is calculated from the stored records each time the page is refreshed -
 * nothing is hard-coded.  The count-up and chart animations only change how a value is
 * REVEALED; the final value is always the real calculated one.
 *
 * Marks statistics use the OVERALL result of each student (CIE 1 + CIE 2 + SEE).
 */
#include "ui.h"

#define NCARDS 8

typedef struct {
    GtkWidget *val, *sub, *icon;
    double num;  int dec;  char suffix[4];  int numeric;
} Stat;

static Stat st[NCARDS];
static GtkWidget *hero_title, *hero_sub, *hero_chips, *l_att_summary, *att_pills, *top_box, *ring;
static GtkListStore *attn_store;
static GtkWidget *l_attn_count;
static double ring_frac = 0, ring_shown = 0;
static int ring_good = 1;

/* chart data (filled by dashboard_refresh, read by the draw callbacks) */
static const char *bar_lab[MAX_SUBJECTS];  static double bar_val[MAX_SUBJECTS];  static int bar_n;
static const char *trd_lab[MAX_EXAMS];     static double trd_val[MAX_EXAMS];     static int trd_n;
static const char *att_lab[MAX_SUBJECTS];  static double att_val[MAX_SUBJECTS];  static int att_n;

static gboolean draw_bars_cb(GtkWidget *w, cairo_t *cr, gpointer d)
{
    (void)d;
    ui_draw_bars(cr, gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w),
                 bar_lab, bar_val, bar_n, 50.0, "Class average % per subject (dashed line = 50%)");
    return FALSE;
}
static gboolean draw_trend_cb(GtkWidget *w, cairo_t *cr, gpointer d)
{
    (void)d;
    ui_draw_line(cr, gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w),
                 trd_lab, trd_val, trd_n, "Class average % per examination");
    return FALSE;
}
static gboolean draw_att_cb(GtkWidget *w, cairo_t *cr, gpointer d)
{
    (void)d;
    ui_draw_bars(cr, gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w),
                 att_lab, att_val, att_n, settings.att_threshold, "Attendance % per subject (dashed line = threshold)");
    return FALSE;
}

/* circular attendance indicator */
static gboolean draw_ring(GtkWidget *w, cairo_t *cr, gpointer d)
{
    double s = gtk_widget_get_allocated_width(w), c = s / 2, r = s / 2 - 5;
    (void)d;
    cairo_set_line_width(cr, 6);
    cairo_set_source_rgb(cr, 0.886, 0.910, 0.941);                       /* #E2E8F0 track */
    cairo_arc(cr, c, c, r, 0, 2 * G_PI); cairo_stroke(cr);
    if (ring_shown > 0) {
        if (ring_good) cairo_set_source_rgb(cr, 0.086, 0.639, 0.290); else cairo_set_source_rgb(cr, 0.863, 0.149, 0.149);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_arc(cr, c, c, r, -G_PI / 2, -G_PI / 2 + 2 * G_PI * (ring_shown > 1 ? 1 : ring_shown));
        cairo_stroke(cr);
    }
    return FALSE;
}

static void ring_step(double p, gpointer d) { (void)d; ring_shown = ring_frac * p; }

/* ---------- stat cards ---------- */
static GtkWidget *make_card(const char *caption, int icon, const char *fg, const char *bg, int idx, int with_ring)
{
    GtkWidget *inner, *card = ui_card(&inner), *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10), *col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "stat-card");
    st[idx].icon = ui_icon_new(icon, 40, fg, bg);
    gtk_box_pack_start(GTK_BOX(top), st[idx].icon, FALSE, FALSE, 0);
    {
        GtkWidget *l = ui_label(caption, "card-label");
        gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
        gtk_box_pack_start(GTK_BOX(top), l, TRUE, TRUE, 0);
    }
    gtk_box_pack_start(GTK_BOX(inner), top, FALSE, FALSE, 0);
    st[idx].val = ui_label("-", "card-value");
    gtk_label_set_ellipsize(GTK_LABEL(st[idx].val), PANGO_ELLIPSIZE_END);
    if (idx == 7) gtk_style_context_add_class(gtk_widget_get_style_context(st[idx].val), "card-value-sm");
    st[idx].sub = ui_label("", "hint");
    gtk_label_set_ellipsize(GTK_LABEL(st[idx].sub), PANGO_ELLIPSIZE_END);
    if (with_ring) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        ring = gtk_drawing_area_new();
        gtk_widget_set_size_request(ring, 58, 58);
        g_signal_connect(ring, "draw", G_CALLBACK(draw_ring), NULL);
        gtk_box_pack_start(GTK_BOX(col), st[idx].val, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(col), st[idx].sub, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row), col, TRUE, TRUE, 0);
        gtk_box_pack_end(GTK_BOX(row), ring, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);
    } else {
        gtk_box_pack_start(GTK_BOX(inner), st[idx].val, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(inner), st[idx].sub, FALSE, FALSE, 0);
    }
    gtk_widget_set_hexpand(card, TRUE);
    return card;
}

/* Sets the final text at once; numeric values are revealed by a count-up when the page opens. */
static void set_card(int i, const char *text, const char *sub, int good, int numeric, double num, int dec, const char *suffix)
{
    GtkStyleContext *sc = gtk_widget_get_style_context(st[i].val);
    gtk_label_set_text(GTK_LABEL(st[i].val), text);
    gtk_label_set_text(GTK_LABEL(st[i].sub), sub);
    gtk_style_context_remove_class(sc, "good");
    gtk_style_context_remove_class(sc, "warn");
    if (good == 1) gtk_style_context_add_class(sc, "good");
    if (good == 0) gtk_style_context_add_class(sc, "warn");
    st[i].numeric = numeric; st[i].num = num; st[i].dec = dec;
    snprintf(st[i].suffix, sizeof st[i].suffix, "%s", suffix ? suffix : "");
}

/* Replays the entrance animations (count-up numbers, attendance ring, icons) after the page was shown. */
static gboolean replay(gpointer d)
{
    int i;
    (void)d;
    for (i = 0; i < NCARDS; i++) {
        if (st[i].numeric) ui_countup(st[i].val, st[i].num, st[i].dec, st[i].suffix);
        ui_icon_animate(st[i].icon);
    }
    ui_animate(ring, "ring", 520, ring_step, NULL, NULL);
    return G_SOURCE_REMOVE;
}

static void on_page_map(GtkWidget *w, gpointer d) { (void)w; (void)d; g_idle_add(replay, NULL); }

static GtkWidget *chip(const char *text)
{
    GtkWidget *b = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(b), "hero-chip");
    gtk_box_pack_start(GTK_BOX(b), gtk_label_new(text), FALSE, FALSE, 0);
    return b;
}

static GtkWidget *section_head(int icon, const char *fg, const char *bg, const char *title, GtkWidget **right_slot)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(row), ui_icon_new(icon, 32, fg, bg), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), ui_label(title, "card-head"), FALSE, FALSE, 0);
    if (right_slot) { *right_slot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6); gtk_box_pack_end(GTK_BOX(row), *right_slot, FALSE, FALSE, 0); }
    return row;
}

GtkWidget *dashboard_build(void)
{
    static const char *caps[NCARDS] = { "Total Students", "Class Average", "Average SGPA", "Class Attendance", "Pass Percentage",
                                        "Attendance Below Threshold", "Need Academic Attention", "Class Topper" };
    static const int icons[NCARDS] = { ICON_STUDENTS, ICON_PERFORMANCE, ICON_GRAD, ICON_ATTENDANCE, ICON_CHECK_CIRCLE, ICON_WARNING, ICON_CROSS_CIRCLE, ICON_TROPHY };
    static const char *fgs[NCARDS] = { "#6842F5", "#6842F5", "#6842F5", "#16A34A", "#16A34A", "#F59E0B", "#DC2626", "#F59E0B" };
    static const char *bgs[NCARDS] = { "#EDE9FE", "#EDE9FE", "#EDE9FE", "#DCFCE7", "#DCFCE7", "#FEF3C7", "#FEE2E2", "#FEF3C7" };
    static const char *heads[6] = { "Roll No", "Name", "Overall %", "SGPA", "Attendance %", "Reason" };
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16), *hero, *grid, *row, *inner, *c, *left, *slot;
    int i;

    /* ---- header: academic overview ---- */
    hero = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_style_context_add_class(gtk_widget_get_style_context(hero), "hero-card");
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_box_pack_start(GTK_BOX(row), ui_icon_new(ICON_DASHBOARD, 46, "#FFFFFF", "#7C5CF7"), FALSE, FALSE, 0);
    left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    hero_title = ui_label("Academic Overview", "hero-title");
    hero_sub = ui_label("", "hero-sub");
    gtk_label_set_line_wrap(GTK_LABEL(hero_sub), TRUE);
    gtk_box_pack_start(GTK_BOX(left), hero_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(left), hero_sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), left, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hero), row, FALSE, FALSE, 0);
    hero_chips = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(hero_chips, 8);
    gtk_box_pack_start(GTK_BOX(hero), hero_chips, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), hero, FALSE, FALSE, 0);

    /* ---- bento grid of the eight statistics ---- */
    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 14);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 14);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    for (i = 0; i < NCARDS; i++)
        gtk_grid_attach(GTK_GRID(grid), make_card(caps[i], icons[i], fgs[i], bgs[i], i, i == 3), i % 4, i / 4, 1, 1);
    gtk_box_pack_start(GTK_BOX(page), grid, FALSE, FALSE, 0);

    /* ---- charts ---- */
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_box_set_homogeneous(GTK_BOX(row), TRUE);
    c = ui_card(&inner);
    gtk_box_pack_start(GTK_BOX(inner), section_head(ICON_MARKS, "#6842F5", "#EDE9FE", "Subject-wise performance", NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), ui_chart_area(G_CALLBACK(draw_bars_cb), 300, 250), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), c, TRUE, TRUE, 0);
    c = ui_card(&inner);
    gtk_box_pack_start(GTK_BOX(inner), section_head(ICON_PERFORMANCE, "#6842F5", "#EDE9FE", "Examination trend", NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), ui_chart_area(G_CALLBACK(draw_trend_cb), 300, 250), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), c, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(page), row, FALSE, FALSE, 0);

    /* ---- attendance summary + top performers ---- */
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_box_set_homogeneous(GTK_BOX(row), TRUE);
    c = ui_card(&inner);
    gtk_box_pack_start(GTK_BOX(inner), section_head(ICON_ATTENDANCE, "#16A34A", "#DCFCE7", "Attendance summary", NULL), FALSE, FALSE, 0);
    att_pills = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(inner), att_pills, FALSE, FALSE, 0);
    l_att_summary = ui_label("", "hint");
    gtk_label_set_line_wrap(GTK_LABEL(l_att_summary), TRUE);
    gtk_box_pack_start(GTK_BOX(inner), l_att_summary, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), ui_chart_area(G_CALLBACK(draw_att_cb), 300, 220), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), c, TRUE, TRUE, 0);
    c = ui_card(&inner);
    gtk_box_pack_start(GTK_BOX(inner), section_head(ICON_TROPHY, "#F59E0B", "#FEF3C7", "Top-performing students", NULL), FALSE, FALSE, 0);
    top_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_pack_start(GTK_BOX(inner), top_box, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), c, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(page), row, FALSE, FALSE, 0);

    /* ---- students requiring attention ---- */
    c = ui_card(&inner);
    gtk_box_pack_start(GTK_BOX(inner), section_head(ICON_WARNING, "#DC2626", "#FEE2E2", "Students requiring attention", &slot), FALSE, FALSE, 0);
    l_attn_count = ui_badge("0", "badge-bad");
    gtk_box_pack_start(GTK_BOX(slot), l_attn_count, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Failed a subject, overall marks below 50%, or attendance below the threshold.", "hint"), FALSE, FALSE, 0);
    attn_store = ui_store_new(6);
    gtk_box_pack_start(GTK_BOX(inner), ui_tree_new(attn_store, heads, 6, NULL, 280), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(page), c, FALSE, FALSE, 0);

    g_signal_connect(page, "map", G_CALLBACK(on_page_map), NULL);       /* entrance animations when the page opens */
    return page;
}

static void clear_box(GtkWidget *b) { gtk_container_foreach(GTK_CONTAINER(b), (GtkCallback)gtk_widget_destroy, NULL); }

void dashboard_refresh(void)
{
    int i, e, j, low = 0, attn = 0, order[MAX_STUDENTS], n, cat[4] = { 0, 0, 0, 0 };
    double pct[MAX_STUDENTS], x, y, v, g;
    char t[260], sub[160], xs[16], ys[16], gs[16], r[160];
    Teacher *tc = &teachers[g_teacher];

    /* ---- header ---- */
    snprintf(t, sizeof t, "Welcome, %s", tc->name);
    gtk_label_set_text(GTK_LABEL(hero_title), t);
    snprintf(t, sizeof t, "%s%s%s%s  -  marks, attendance and academic performance, calculated live from the stored records.",
             tc->subject[0] ? tc->subject : "Faculty", tc->subject_code[0] ? " (" : "", tc->subject_code[0] ? tc->subject_code : "",
             tc->subject_code[0] ? ")" : "");
    gtk_label_set_text(GTK_LABEL(hero_sub), t);
    clear_box(hero_chips);
    snprintf(t, sizeof t, "Class %s", tc->cls);                gtk_box_pack_start(GTK_BOX(hero_chips), chip(t), FALSE, FALSE, 0);
    snprintf(t, sizeof t, "Semester %d", tc->semester);        gtk_box_pack_start(GTK_BOX(hero_chips), chip(t), FALSE, FALSE, 0);
    snprintf(t, sizeof t, "Academic year %s", tc->year);       gtk_box_pack_start(GTK_BOX(hero_chips), chip(t), FALSE, FALSE, 0);
    snprintf(t, sizeof t, "%d subjects  |  %d credits", n_subjects, total_credits()); gtk_box_pack_start(GTK_BOX(hero_chips), chip(t), FALSE, FALSE, 0);
    if (tc->institution[0]) gtk_box_pack_start(GTK_BOX(hero_chips), chip(tc->institution), FALSE, FALSE, 0);
    gtk_widget_show_all(hero_chips);

    for (i = 0; i < n_students; i++) {
        y = att_pct(students[i].roll, NULL);
        if (y >= 0) cat[att_category(y)]++;
        if (att_is_low(students[i].roll)) low++;
    }

    /* ---- the eight statistics ---- */
    snprintf(t, sizeof t, "%d", n_students);
    snprintf(sub, sizeof sub, "%d subjects, %d credits", n_subjects, total_credits());
    set_card(0, t, sub, -1, 1, n_students, 0, "");
    v = class_avg_pct(EXAM_ALL);
    if (v >= 0) { snprintf(t, sizeof t, "%.1f%%", v); set_card(1, t, "CIE 1 + CIE 2 + SEE", v >= 50, 1, v, 1, "%"); } else set_card(1, "-", "no marks yet", -1, 0, 0, 0, "");
    v = class_avg_sgpa();
    if (v >= 0) { snprintf(t, sizeof t, "%.2f", v); set_card(2, t, "out of 10, credit weighted", v >= 5, 1, v, 2, ""); } else set_card(2, "-", "no marks yet", -1, 0, 0, 0, "");
    v = class_att_pct();
    if (v >= 0) {
        snprintf(t, sizeof t, "%.1f%%", v);
        set_card(3, t, "all subjects", v >= settings.att_threshold, 1, v, 1, "%");
        ring_frac = v / 100.0; ring_good = v >= settings.att_threshold; ring_shown = ring_frac;
    } else { set_card(3, "-", "no attendance yet", -1, 0, 0, 0, ""); ring_frac = ring_shown = 0; }
    v = class_pass_pct(EXAM_ALL);
    if (v >= 0) { snprintf(t, sizeof t, "%.1f%%", v); set_card(4, t, "passed every subject", v >= 75, 1, v, 1, "%"); } else set_card(4, "-", "no marks yet", -1, 0, 0, 0, "");
    snprintf(t, sizeof t, "%d", low);
    set_card(5, t, "students below the threshold", low == 0, 1, low, 0, "");

    /* ---- ranking, top performers, attention list ---- */
    n = exam_ranking(EXAM_ALL, order, pct);
    if (n > 0) { snprintf(t, sizeof t, "%s", students[order[0]].name); snprintf(sub, sizeof sub, "%.1f%% overall", pct[0]); set_card(7, t, sub, 1, 0, 0, 0, ""); }
    else set_card(7, "-", "", -1, 0, 0, 0, "");

    clear_box(top_box);
    for (i = 0; i < n && i < 5; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10), *col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), *l;
        GdkPixbuf *pb = ui_avatar_pixbuf(students[order[i]].name, 36, i == 0 ? "#FEF3C7" : "#EDE9FE", i == 0 ? "#B45309" : "#5B34D6");
        char rk[8];
        g = student_sgpa(students[order[i]].roll, NULL, NULL);
        snprintf(rk, sizeof rk, "%d", student_rank(students[order[i]].roll, EXAM_ALL));
        gtk_style_context_add_class(gtk_widget_get_style_context(row), "rank-row");
        l = ui_label(rk, "rank-badge");
        if (i == 0) gtk_style_context_add_class(gtk_widget_get_style_context(l), "rank-badge-1");
        gtk_widget_set_valign(l, GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(row), l, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row), gtk_image_new_from_pixbuf(pb), FALSE, FALSE, 0);
        g_object_unref(pb);
        l = ui_label(students[order[i]].name, "info-card-value");
        gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
        gtk_box_pack_start(GTK_BOX(col), l, FALSE, FALSE, 0);
        snprintf(sub, sizeof sub, "Roll %s", students[order[i]].roll);
        gtk_box_pack_start(GTK_BOX(col), ui_label(sub, "hint"), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row), col, TRUE, TRUE, 0);
        snprintf(sub, sizeof sub, "%.1f%%", pct[i]);
        gtk_box_pack_end(GTK_BOX(row), ui_badge(sub, "badge-good"), FALSE, FALSE, 0);
        if (g >= 0) { snprintf(sub, sizeof sub, "SGPA %.2f", g); gtk_box_pack_end(GTK_BOX(row), ui_badge(sub, "badge-neutral"), FALSE, FALSE, 0); }
        gtk_box_pack_start(GTK_BOX(top_box), row, FALSE, FALSE, 0);
    }
    if (n == 0) gtk_box_pack_start(GTK_BOX(top_box), ui_label("No marks entered yet.", "hint"), FALSE, FALSE, 0);
    gtk_widget_show_all(top_box);

    gtk_list_store_clear(attn_store);
    for (i = 0; i < n_students; i++) {
        int lowatt = att_is_low(students[i].roll), failed = 0, lowm = 0;
        x = student_exam_pct(students[i].roll, EXAM_ALL, NULL, NULL);
        y = att_pct(students[i].roll, NULL);
        g = student_sgpa(students[i].roll, NULL, NULL);
        if (x >= 0) {
            for (j = 0; j < n_subjects; j++) if (subject_pct(students[i].roll, j) >= 0 && !subject_pass(students[i].roll, j)) failed++;
            lowm = x < 50;
        }
        if (!lowatt && !failed && !lowm) continue;
        if (failed || lowm) attn++;
        r[0] = '\0';
        if (failed) sb_add(r, sizeof r, "Failed %d subject%s", failed, failed == 1 ? "" : "s");
        if (lowm)   sb_add(r, sizeof r, "%sOverall below 50%%", r[0] ? "; " : "");
        if (lowatt) sb_add(r, sizeof r, "%sLow attendance", r[0] ? "; " : "");
        if (x >= 0) snprintf(xs, sizeof xs, "%.1f", x); else strcpy(xs, "-");
        if (y >= 0) snprintf(ys, sizeof ys, "%.1f", y); else strcpy(ys, "-");
        if (g >= 0) snprintf(gs, sizeof gs, "%.2f", g); else strcpy(gs, "-");
        ui_store_add(attn_store, 6, "#B91C1C", i, students[i].roll, students[i].name, xs, gs, ys, r);
    }
    snprintf(t, sizeof t, "%d", attn);
    set_card(6, t, "failed a subject or overall below 50%", attn == 0, 1, attn, 0, "");
    snprintf(t, sizeof t, "%d students", gtk_tree_model_iter_n_children(GTK_TREE_MODEL(attn_store), NULL));
    gtk_label_set_text(GTK_LABEL(l_attn_count), t);

    /* ---- chart data ---- */
    bar_n = att_n = trd_n = 0;
    for (i = 0; i < n_subjects; i++) {
        const char *nm = subjects[i].short_name[0] ? subjects[i].short_name : subjects[i].code;
        int c, a, tc2 = 0, ta = 0;
        v = subject_avg_pct(i, EXAM_ALL);
        if (v >= 0) { bar_lab[bar_n] = nm; bar_val[bar_n++] = v; }
        for (e = 0; e < n_students; e++) { att_stats(students[e].roll, subjects[i].code, &c, &a); tc2 += c; ta += a; }
        if (tc2 > 0) { att_lab[att_n] = nm; att_val[att_n++] = ta * 100.0 / tc2; }
    }
    for (e = 0; e < n_exams; e++) {
        v = class_avg_pct(e);
        if (v >= 0) { trd_lab[trd_n] = exams[e].name; trd_val[trd_n++] = v; }
    }

    clear_box(att_pills);
    if (cat[0] + cat[1] + cat[2] + cat[3] > 0) {
        snprintf(t, sizeof t, "Excellent %d", cat[0]);    gtk_box_pack_start(GTK_BOX(att_pills), ui_badge(t, "badge-good"), FALSE, FALSE, 0);
        snprintf(t, sizeof t, "Good %d", cat[1]);         gtk_box_pack_start(GTK_BOX(att_pills), ui_badge(t, "badge-neutral"), FALSE, FALSE, 0);
        snprintf(t, sizeof t, "Satisfactory %d", cat[2]); gtk_box_pack_start(GTK_BOX(att_pills), ui_badge(t, "badge-warn"), FALSE, FALSE, 0);
        snprintf(t, sizeof t, "Low %d", cat[3]);          gtk_box_pack_start(GTK_BOX(att_pills), ui_badge(t, "badge-bad"), FALSE, FALSE, 0);
        snprintf(t, sizeof t, "Excellent 90+  |  Good 80-89  |  Satisfactory %.0f-79  |  Low below %.0f", settings.att_threshold, settings.att_threshold);
    } else snprintf(t, sizeof t, "No attendance recorded yet.");
    gtk_label_set_text(GTK_LABEL(l_att_summary), t);
    gtk_widget_show_all(att_pills);
    gtk_widget_queue_draw(ring);
}
