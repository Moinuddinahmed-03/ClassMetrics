/*
 * ui_marks.c : Marks page.
 *   Tab 1  Marks sheet: ONE ROW PER STUDENT with the six subject columns, total,
 *          percentage, SGPA and overall result.  An examination filter switches the
 *          subject columns between the overall totals and CIE 1 / CIE 2 / SEE marks.
 *          "View Student" (or double-click) opens a read-only detailed marks window;
 *          "Edit Marks" opens a separate editing window for all 18 marks of a student.
 *   Tab 2  Ranking & summary (overall or per examination) with SGPA.
 * Totals, grades, SGPA and rankings are always recalculated from the stored marks.
 */
#include "ui.h"

static GtkWidget *e_search, *c_stu, *c_sub, *c_exam, *c_grade, *c_stat, *c_rank;
static GtkWidget *l_count, *l_hint, *l_rank_stats, *sheet_box, *sheet_holder = NULL;
static GtkListStore *st_sheet, *st_rank;
static GtkTreeView *tv_sheet;
static int busy = 0, sel_i = -1;

#define RESP_EDIT 10

static const char *GRADE_ITEMS[8] = { "All Grades", "S", "A", "B", "C", "D", "E", "F" };

/* ======================= helpers ======================= */

static int contains_nocase(const char *text, const char *pat)
{
    size_t i, j, lt = strlen(text), lp = strlen(pat);
    for (i = 0; i + lp <= lt; i++) {
        for (j = 0; j < lp; j++)
            if (tolower((unsigned char)text[i + j]) != tolower((unsigned char)pat[j])) break;
        if (j == lp) return 1;
    }
    return lp == 0;
}

static void fill_students_all(void)
{
    char t[100];
    int i, act = gtk_combo_box_get_active(GTK_COMBO_BOX(c_stu));
    if (act < 0 || act > n_students) act = 0;
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(c_stu));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c_stu), "All students");
    for (i = 0; i < n_students; i++) {
        snprintf(t, sizeof t, "%s - %s", students[i].roll, students[i].name);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c_stu), t);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(c_stu), act);
}

/* "Overall Marks (100 per subject)", "CIE 1 (20 per subject)", ... */
static void fill_exam_combo(void)
{
    char t[80];
    int i, act = gtk_combo_box_get_active(GTK_COMBO_BOX(c_exam));
    if (act < 0 || act > n_exams) act = 0;                          /* default: Overall Marks */
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(c_exam));
    snprintf(t, sizeof t, "Overall Marks (%.0f per subject)", n_subjects > 0 ? subjects[0].max_marks : 100.0);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c_exam), t);
    for (i = 0; i < n_exams; i++) {
        snprintf(t, sizeof t, "%s (%.0f per subject)", exams[i].name, exams[i].max_marks);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c_exam), t);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(c_exam), act);
}

/* Adds a row from an array of strings (the number of columns depends on the subjects). */

/* ======================= main marks sheet ======================= */

static void on_view(GtkWidget *w, gpointer d);

static void on_sheet_selected(GtkTreeView *tv, gpointer d)
{
    int key = ui_tree_selected(tv);
    (void)d;
    if (key >= 0 && !busy) sel_i = key;
}

static gboolean open_view_idle(gpointer d)
{
    (void)d;
    on_view(NULL, NULL);
    return G_SOURCE_REMOVE;
}

static void on_sheet_activated(GtkTreeView *tv, GtkTreePath *p, GtkTreeViewColumn *c, gpointer d)
{
    (void)p; (void)c; (void)d;
    on_sheet_selected(tv, NULL);
    g_idle_add(open_view_idle, NULL);        /* double-click opens the READ-ONLY details (after the click is handled) */
}

static void update_hint(int subj)
{
    if (subj >= 0)
        gtk_label_set_text(GTK_LABEL(l_hint), "A subject is selected: the Grade and Result filters apply to that subject. "
                                              "SGPA and Overall Result columns always show the OVERALL result.");
    else
        gtk_label_set_text(GTK_LABEL(l_hint), "All subjects: the Grade filter finds students with at least one subject of that grade; "
                                              "the Result filter uses the OVERALL result (any failed subject = Fail).");
}

/* Hover text of the "Fail (n)" cell: the failed subjects with their complete totals. */
static int sheet_ncols = 0;

static gboolean on_sheet_tip(GtkWidget *w, gint x, gint y, gboolean kb, GtkTooltip *tip, gpointer d)
{
    GtkTreeView *tv = GTK_TREE_VIEW(w);
    GtkTreeModel *m = gtk_tree_view_get_model(tv);
    GtkTreePath *p = NULL;
    GtkTreeViewColumn *col = NULL;
    GtkTreeIter it;
    gchar *text = NULL;
    int bx, by;
    (void)kb; (void)d;
    gtk_tree_view_convert_widget_to_bin_window_coords(tv, x, y, &bx, &by);
    if (!gtk_tree_view_get_path_at_pos(tv, bx, by, &p, &col, NULL, NULL)) return FALSE;
    if (col != gtk_tree_view_get_column(tv, sheet_ncols - 1) || !gtk_tree_model_get_iter(m, &it, p)) { gtk_tree_path_free(p); return FALSE; }
    gtk_tree_model_get(m, &it, 3 * sheet_ncols, &text, -1);
    if (text && text[0]) {
        gtk_tooltip_set_text(tip, text);
        gtk_tree_view_set_tooltip_cell(tv, tip, p, col, NULL);
        g_free(text); gtk_tree_path_free(p);
        return TRUE;
    }
    g_free(text); gtk_tree_path_free(p);
    return FALSE;
}

/* Rebuilds the table (columns depend on the subjects and on the chosen examination).
 * Only the FAILED SUBJECT cells and the overall Fail status are coloured, never the whole row. */
static void fill_sheet(void)
{
    const char *heads[MAX_SUBJECTS + 6], *cells[MAX_SUBJECTS + 6], *fg[MAX_SUBJECTS + 6], *bg[MAX_SUBJECTS + 6];
    char htext[MAX_SUBJECTS + 6][40], ctext[MAX_SUBJECTS + 6][24], t[120], tip[700], list[600];
    float xal[MAX_SUBJECTS + 6];
    int ncols = n_subjects + 6, i, j, shown = 0, key;
    int fs = gtk_combo_box_get_active(GTK_COMBO_BOX(c_stu)) - 1;      /* -1 = all students */
    int fj = gtk_combo_box_get_active(GTK_COMBO_BOX(c_sub)) - 1;      /* -1 = all subjects */
    int e = ui_exam_sel(c_exam, 1);                                    /* EXAM_ALL (-1) = overall */
    int fg_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(c_grade)) - 1; /* -1 = all grades, 0..6 = S..F */
    int ft = gtk_combo_box_get_active(GTK_COMBO_BOX(c_stat));          /* 0 all, 1 pass, 2 fail */
    const char *q = gtk_entry_get_text(GTK_ENTRY(e_search));
    double maxt = 0;
    SheetRow row;
    GtkWidget *sw, *tv;

    if (e < -1) e = -1;
    for (j = 0; j < n_subjects; j++) maxt += (e < 0) ? subjects[j].max_marks : (e < n_exams ? exams[e].max_marks : 0);

    if (sheet_holder) gtk_widget_destroy(sheet_holder);              /* new table: new column headings */
    snprintf(htext[0], sizeof htext[0], "Roll No");
    snprintf(htext[1], sizeof htext[1], "Student Name");
    for (j = 0; j < n_subjects; j++)
        snprintf(htext[2 + j], sizeof htext[0], "%s", subjects[j].short_name[0] ? subjects[j].short_name : subjects[j].code);
    snprintf(htext[2 + n_subjects], sizeof htext[0], "Total (/%.0f)", maxt);
    snprintf(htext[3 + n_subjects], sizeof htext[0], "Percentage");
    snprintf(htext[4 + n_subjects], sizeof htext[0], "SGPA");
    snprintf(htext[5 + n_subjects], sizeof htext[0], "Overall Result");
    for (i = 0; i < ncols; i++) { heads[i] = htext[i]; xal[i] = (i < 2) ? 0.0f : 0.5f; }
    sheet_ncols = ncols;
    st_sheet = ui_cell_store_new(ncols);
    tv = ui_cell_tree_new(st_sheet, heads, ncols, xal, 52);
    tv_sheet = GTK_TREE_VIEW(tv);
    g_object_unref(st_sheet);
    for (j = 0; j < n_subjects; j++) {                                /* full subject names as header tooltips */
        GtkTreeViewColumn *col = gtk_tree_view_get_column(tv_sheet, 2 + j);
        GtkWidget *l = gtk_label_new(htext[2 + j]);
        snprintf(t, sizeof t, "%s (%s)", subjects[j].name, subjects[j].code);
        gtk_widget_set_tooltip_text(l, t);
        gtk_widget_show(l);
        gtk_tree_view_column_set_widget(col, l);
        gtk_tree_view_column_set_alignment(col, 0.5f);
    }
    gtk_tree_view_column_set_expand(gtk_tree_view_get_column(tv_sheet, 1), TRUE);
    gtk_tree_view_column_set_min_width(gtk_tree_view_get_column(tv_sheet, 1), 160);
    g_signal_connect(tv, "cursor-changed", G_CALLBACK(on_sheet_selected), NULL);
    g_signal_connect(tv, "row-activated", G_CALLBACK(on_sheet_activated), NULL);
    g_signal_connect(tv, "query-tooltip", G_CALLBACK(on_sheet_tip), NULL);
    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(sw), 360);
    gtk_container_add(GTK_CONTAINER(sw), tv);
    gtk_box_pack_start(GTK_BOX(sheet_box), sw, TRUE, TRUE, 0);
    gtk_widget_show_all(sw);
    sheet_holder = sw;

    for (i = 0; i < n_students; i++) {
        if (fs >= 0 && i != fs) continue;
        if (q[0] && !contains_nocase(students[i].name, q) && !contains_nocase(students[i].roll, q)) continue;
        if (!marks_filter_match(students[i].roll, fj, fg_idx, ft)) continue;
        marks_sheet_row(students[i].roll, e, &row);
        snprintf(ctext[0], sizeof ctext[0], "%s", students[i].roll);
        cells[0] = ctext[0];
        cells[1] = students[i].name;
        for (j = 0; j < ncols; j++) { fg[j] = "#1F2937"; bg[j] = (shown % 2) ? "#F5F6FB" : ""; }      /* alternating rows */
        tip[0] = '\0'; list[0] = '\0';
        for (j = 0; j < n_subjects; j++) {
            if (row.has[j]) snprintf(ctext[2 + j], sizeof ctext[0], "%.1f", row.cell[j]); else snprintf(ctext[2 + j], sizeof ctext[0], "-");
            cells[2 + j] = ctext[2 + j];
            if (row.fail[j]) {                                        /* failed SUBJECT (judged on the complete total) */
                fg[2 + j] = "#D93636"; bg[2 + j] = "#FDECEC";
                sb_add(list, sizeof list, "%s%s \xE2\x80\x94 %.1f/%.0f", list[0] ? "\n" : "", subjects[j].name, row.subtotal[j], subjects[j].max_marks);
            }
        }
        if (row.any) snprintf(ctext[2 + n_subjects], sizeof ctext[0], "%.1f", row.total); else strcpy(ctext[2 + n_subjects], "-");
        if (row.pct >= 0) snprintf(ctext[3 + n_subjects], sizeof ctext[0], "%.2f%%", row.pct); else strcpy(ctext[3 + n_subjects], "-");
        if (row.sgpa >= 0) snprintf(ctext[4 + n_subjects], sizeof ctext[0], "%.2f", row.sgpa); else strcpy(ctext[4 + n_subjects], "-");
        if (!row.any) strcpy(ctext[5 + n_subjects], "-");
        else if (row.failed == 0) { strcpy(ctext[5 + n_subjects], "Pass"); fg[5 + n_subjects] = "#1E9E5A"; }
        else {
            snprintf(ctext[5 + n_subjects], sizeof ctext[0], "Fail (%d)", row.failed);
            fg[5 + n_subjects] = "#D93636";
            snprintf(tip, sizeof tip, "%s\n%s", row.failed > 1 ? "Failed subjects:" : "Failed subject:", list);
        }
        for (j = 2 + n_subjects; j < ncols; j++) cells[j] = ctext[j];
        ui_cell_store_add(st_sheet, ncols, cells, fg, bg, row.failed > 0 ? tip : "", i);
        shown++;
    }
    snprintf(t, sizeof t, "Showing %d of %d students", shown, n_students);
    gtk_label_set_text(GTK_LABEL(l_count), t);
    update_hint(fj);

    /* keep the previously selected student selected */
    if (sel_i >= 0) {
        GtkTreeModel *m = GTK_TREE_MODEL(st_sheet);
        GtkTreeIter it;
        if (gtk_tree_model_get_iter_first(m, &it)) do {
            gtk_tree_model_get(m, &it, 3 * ncols + 1, &key, -1);
            if (key == sel_i) {
                GtkTreePath *p = gtk_tree_model_get_path(m, &it);
                gtk_tree_selection_select_iter(gtk_tree_view_get_selection(tv_sheet), &it);
                gtk_tree_view_scroll_to_cell(tv_sheet, p, NULL, FALSE, 0, 0);
                gtk_tree_path_free(p);
                break;
            }
        } while (gtk_tree_model_iter_next(m, &it));
    }
}


/* ======================= ranking tab (unchanged behaviour) ======================= */

static void fill_rank(void)
{
    int e = ui_exam_sel(c_rank, 1), order[MAX_STUDENTS], n, i, rank = 0, earned, cred;
    double pct[MAX_STUDENTS], tot, mx, g;
    char a[16], b[16], c[16], d[16], f[16], h[16], t[260];

    gtk_list_store_clear(st_rank);
    if (e < -1) { gtk_label_set_text(GTK_LABEL(l_rank_stats), "No examinations."); return; }
    n = exam_ranking(e, order, pct);
    for (i = 0; i < n; i++) {
        int s = order[i], pass = student_exam_pass(students[s].roll, e);
        if (i == 0 || pct[i] < pct[i - 1]) rank = i + 1;
        student_exam_pct(students[s].roll, e, &tot, &mx);
        g = (e < 0) ? student_sgpa(students[s].roll, &earned, &cred) : -1;
        snprintf(a, sizeof a, "%d", rank);
        snprintf(b, sizeof b, "%.1f", tot);
        snprintf(c, sizeof c, "%.0f", mx);
        snprintf(d, sizeof d, "%.2f", pct[i]);
        if (g >= 0) { snprintf(f, sizeof f, "%.2f", g); snprintf(h, sizeof h, "%d / %d", earned, cred); } else { strcpy(f, "-"); strcpy(h, "-"); }
        ui_store_add(st_rank, 10, pass ? NULL : "#D93636", s, a, students[s].roll, students[s].name, b, c, d,
                     grade_of(pct[i]), f, h, pass ? "Pass" : "Fail");
    }
    if (n > 0) {
        snprintf(t, sizeof t, "Class average %.2f%%   |   Highest %.2f%% (%s)   |   Lowest %.2f%%   |   Pass %.1f%%   |   Average SGPA %.2f",
                 class_avg_pct(e), pct[0], students[order[0]].name, pct[n - 1], class_pass_pct(e), class_avg_sgpa());
        if (e >= 0) { char *p = strstr(t, "   |   Average SGPA"); if (p) *p = '\0'; }
    } else snprintf(t, sizeof t, "No marks entered yet.");
    gtk_label_set_text(GTK_LABEL(l_rank_stats), t);
}

void marks_refresh(void)
{
    busy = 1;
    fill_students_all();
    ui_combo_subjects(GTK_COMBO_BOX_TEXT(c_sub), 1);
    fill_exam_combo();
    ui_combo_exams_ex(GTK_COMBO_BOX_TEXT(c_rank), 1);
    busy = 0;
    if (sel_i >= n_students) sel_i = -1;
    fill_sheet();
    fill_rank();
}

static void on_filter(GtkWidget *w, gpointer d) { (void)w; (void)d; if (!busy) fill_sheet(); }
static void on_rank_changed(GtkWidget *w, gpointer d) { (void)w; (void)d; if (!busy) fill_rank(); }

/* ======================= Edit Marks window ======================= */

typedef struct {
    int idx;
    GtkWidget *en[MAX_SUBJECTS][3];
    GtkWidget *l_total[MAX_SUBJECTS], *l_grade[MAX_SUBJECTS], *l_gp[MAX_SUBJECTS], *l_stat[MAX_SUBJECTS];
    GtkWidget *l_sum;
} EditCtx;

static void set_cls(GtkWidget *l, const char *cls)
{
    GtkStyleContext *sc = gtk_widget_get_style_context(l);
    gtk_style_context_remove_class(sc, "good");
    gtk_style_context_remove_class(sc, "warn");
    if (cls) gtk_style_context_add_class(sc, cls);
}

/* Reads one entry: 0 = blank, 1 = valid number, -1 = invalid (also shows the error colour). */
static int read_entry(GtkWidget *en, double max, double *v)
{
    const char *s = gtk_entry_get_text(GTK_ENTRY(en));
    GtkStyleContext *sc = gtk_widget_get_style_context(en);
    int bad = 0, blank = (s[0] == '\0');
    if (!blank) bad = !parse_double(s, v) || *v < 0 || *v > max;
    if (bad) gtk_style_context_add_class(sc, "entry-error"); else gtk_style_context_remove_class(sc, "entry-error");
    return blank ? 0 : bad ? -1 : 1;
}

/* Live update of totals, grades, grade points, SGPA and result while typing. */
static void edit_recalc(GtkWidget *w, gpointer data)
{
    EditCtx *c = data;
    double tot = 0, maxe = 0, sg_num = 0, sg_den = 0;
    int j, e, anybad = 0, overall_pass = 1, any = 0;
    char t[200];
    (void)w;
    for (j = 0; j < n_subjects; j++) {
        double st = 0, sm = 0, v;
        int cnt = 0, bad = 0;
        for (e = 0; e < 3 && e < n_exams; e++) {
            int r = read_entry(c->en[j][e], exams[e].max_marks, &v);
            if (r < 0) bad = 1;
            else if (r > 0) { st += v; sm += exams[e].max_marks; cnt++; }
        }
        if (bad) {
            anybad = 1;
            gtk_label_set_text(GTK_LABEL(c->l_total[j]), "-"); gtk_label_set_text(GTK_LABEL(c->l_grade[j]), "-");
            gtk_label_set_text(GTK_LABEL(c->l_gp[j]), "-"); gtk_label_set_text(GTK_LABEL(c->l_stat[j]), "Invalid");
            set_cls(c->l_stat[j], "warn");
        } else if (cnt == 0 || sm <= 0) {
            gtk_label_set_text(GTK_LABEL(c->l_total[j]), "-"); gtk_label_set_text(GTK_LABEL(c->l_grade[j]), "-");
            gtk_label_set_text(GTK_LABEL(c->l_gp[j]), "-"); gtk_label_set_text(GTK_LABEL(c->l_stat[j]), "-");
            set_cls(c->l_stat[j], NULL);
        } else {
            double p = st * 100.0 / sm;
            int pass = p + 1e-9 >= subjects[j].pass_marks * 100.0 / subjects[j].max_marks;
            double gp = pass ? grade_points_of(p) : 0.0;
            snprintf(t, sizeof t, "%.1f / %.0f", st, subjects[j].max_marks);
            gtk_label_set_text(GTK_LABEL(c->l_total[j]), t);
            gtk_label_set_text(GTK_LABEL(c->l_grade[j]), grade_of(p));
            snprintf(t, sizeof t, "%.0f", gp);
            gtk_label_set_text(GTK_LABEL(c->l_gp[j]), t);
            gtk_label_set_text(GTK_LABEL(c->l_stat[j]), pass ? "Pass" : "Fail");
            set_cls(c->l_stat[j], pass ? "good" : "warn");
            tot += st; maxe += sm; any = 1;
            if (!pass) overall_pass = 0;
            if (subjects[j].credits > 0) { sg_den += subjects[j].credits; sg_num += subjects[j].credits * gp; }
        }
    }
    if (anybad) gtk_label_set_text(GTK_LABEL(c->l_sum), "Some entries are invalid (negative, not a number, or above the maximum).");
    else if (!any) gtk_label_set_text(GTK_LABEL(c->l_sum), "No marks entered.");
    else {
        snprintf(t, sizeof t, "Overall marks %.1f / %.0f     Percentage %.2f%%     SGPA %s     Overall result: %s", tot, maxe,
                 tot * 100.0 / maxe, "", overall_pass ? "PASS" : "FAIL");
        if (sg_den > 0) snprintf(t, sizeof t, "Overall marks %.1f / %.0f     Percentage %.2f%%     SGPA %.2f     Overall result: %s", tot, maxe,
                                 tot * 100.0 / maxe, sg_num / sg_den, overall_pass ? "PASS" : "FAIL");
        gtk_label_set_text(GTK_LABEL(c->l_sum), t);
    }
}

/* Opens the editing window.  Returns 1 when the marks were saved. */
static int edit_student(int idx)
{
    EditCtx *c = g_malloc0(sizeof *c);
    GtkWidget *dlg, *grid, *box, *sw;
    static const char *hd[8] = { "Subject", "", "", "", "Total", "Grade", "Grade points", "Pass/Fail" };
    char t[420], leg[300];
    int j, e, saved = 0;

    c->idx = idx;
    snprintf(t, sizeof t, "Edit Marks - %s", students[idx].name);
    dlg = gtk_dialog_new_with_buttons(t, GTK_WINDOW(g_window), GTK_DIALOG_MODAL, "Cancel", GTK_RESPONSE_CANCEL,
                                      "Save All Marks", GTK_RESPONSE_OK, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 900, 460);
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(box), 16);
    snprintf(t, sizeof t, "%s    (Roll No: %s)", students[idx].name, students[idx].roll);
    gtk_box_pack_start(GTK_BOX(box), ui_label(t, "page-title"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), ui_label("Edit any of the marks below. Totals, grades, grade points, SGPA and result update as you type. "
                                              "A blank box means no mark entered. Nothing is saved until you press Save All Marks.", "hint"), FALSE, FALSE, 0);

    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 14);
    for (e = 0; e < 8; e++) {
        GtkWidget *h;
        if (e >= 1 && e <= 3) {
            if (e - 1 < n_exams) snprintf(t, sizeof t, "%s (max %.0f)", exams[e - 1].name, exams[e - 1].max_marks); else strcpy(t, "-");
            h = ui_label(t, "card-head");
        } else h = ui_label(hd[e], "card-head");
        gtk_grid_attach(GTK_GRID(grid), h, e, 0, 1, 1);
    }
    for (j = 0; j < n_subjects; j++) {
        GtkWidget *nm;
        snprintf(t, sizeof t, "%s - %s", subjects[j].short_name, subjects[j].name);
        nm = ui_label(t, NULL);
        gtk_label_set_ellipsize(GTK_LABEL(nm), PANGO_ELLIPSIZE_END);
        gtk_widget_set_size_request(nm, 260, -1);
        gtk_widget_set_tooltip_text(nm, t);
        gtk_grid_attach(GTK_GRID(grid), nm, 0, j + 1, 1, 1);
        for (e = 0; e < 3; e++) {
            int k = e < n_exams ? marks_find(students[idx].roll, subjects[j].code, e) : -1;
            c->en[j][e] = gtk_entry_new();
            gtk_entry_set_width_chars(GTK_ENTRY(c->en[j][e]), 8);
            gtk_entry_set_alignment(GTK_ENTRY(c->en[j][e]), 0.5f);
            if (k >= 0) { snprintf(t, sizeof t, "%g", marks[k].marks); gtk_entry_set_text(GTK_ENTRY(c->en[j][e]), t); }
            gtk_widget_set_sensitive(c->en[j][e], e < n_exams);
            gtk_grid_attach(GTK_GRID(grid), c->en[j][e], 1 + e, j + 1, 1, 1);
        }
        c->l_total[j] = ui_label("-", NULL); c->l_grade[j] = ui_label("-", NULL);
        c->l_gp[j] = ui_label("-", NULL);    c->l_stat[j] = ui_label("-", NULL);
        gtk_grid_attach(GTK_GRID(grid), c->l_total[j], 4, j + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), c->l_grade[j], 5, j + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), c->l_gp[j], 6, j + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), c->l_stat[j], 7, j + 1, 1, 1);
    }
    for (j = 0; j < n_subjects; j++)                                  /* connect after all widgets exist */
        for (e = 0; e < 3; e++) g_signal_connect(c->en[j][e], "changed", G_CALLBACK(edit_recalc), c);
    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(sw), 240);
    gtk_container_add(GTK_CONTAINER(sw), grid);
    gtk_box_pack_start(GTK_BOX(box), sw, TRUE, TRUE, 0);
    c->l_sum = ui_label("", "formula");
    gtk_box_pack_start(GTK_BOX(box), c->l_sum, FALSE, FALSE, 0);
    grade_legend(leg, sizeof leg);
    snprintf(t, sizeof t, "Grades: %s", leg);
    {
        GtkWidget *l = ui_label(t, "hint");
        gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
        gtk_box_pack_start(GTK_BOX(box), l, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), box, TRUE, TRUE, 0);
    gtk_widget_show_all(dlg);
    edit_recalc(NULL, c);

    while (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
        double vals[MAX_SUBJECTS * 3] = { 0 };
        int given[MAX_SUBJECTS * 3] = { 0 }, bad = 0;
        char err[200] = "";
        for (j = 0; j < n_subjects && !bad; j++)
            for (e = 0; e < 3 && e < n_exams; e++) {
                int r = read_entry(c->en[j][e], exams[e].max_marks, &vals[j * 3 + e]);
                if (r < 0) {
                    const char *s = gtk_entry_get_text(GTK_ENTRY(c->en[j][e]));
                    double v;
                    if (!parse_double(s, &v)) snprintf(err, sizeof err, "%s, %s: enter a number.", subjects[j].short_name, exams[e].name);
                    else if (v < 0) snprintf(err, sizeof err, "%s, %s: marks cannot be negative.", subjects[j].short_name, exams[e].name);
                    else snprintf(err, sizeof err, "%s, %s: marks cannot exceed %.0f.", subjects[j].short_name, exams[e].name, exams[e].max_marks);
                    bad = 1;
                    break;
                }
                given[j * 3 + e] = (r > 0);
            }
        if (bad) { ui_error(err); continue; }                         /* nothing is saved */
        if (!marks_save_student(students[idx].roll, vals, given, err)) { ui_error(err); continue; }
        ui_save();
        ui_refresh_all();
        saved = 1;
        break;
    }
    gtk_widget_destroy(dlg);
    g_free(c);
    return saved;
}

/* ======================= View Student window (read-only) =======================
 * Fixed header (student information) + ONE vertically scrolling area with the nine
 * summary cards and the examination marks table + fixed footer buttons (dialog buttons).
 * The scroll area never scrolls sideways: labels are ellipsized and the table columns share the width. */

static void clear_children(GtkWidget *w, gpointer d) { (void)d; gtk_widget_destroy(w); }

/* One summary card (equal size cards come from the homogeneous grid). */
static void card_in(GtkWidget *grid, int idx, int icon, const char *cap, const char *val, const char *note, int good)
{
    GtkWidget *inner, *card = ui_card(&inner), *top, *l, *v;
    top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    l = ui_label(cap, "card-label");
    gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(l, TRUE);
    gtk_box_pack_start(GTK_BOX(top), l, TRUE, TRUE, 0);
    l = ui_icon_new(icon, 28, "#6842F5", "#EDE9FE");                    /* soft icon square, top right */
    gtk_box_pack_end(GTK_BOX(top), l, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), top, FALSE, FALSE, 0);
    v = ui_label(val, "card-value");
    gtk_label_set_ellipsize(GTK_LABEL(v), PANGO_ELLIPSIZE_END);
    if (good >= 0) gtk_style_context_add_class(gtk_widget_get_style_context(v), good ? "good" : "warn");
    gtk_box_pack_start(GTK_BOX(inner), v, FALSE, FALSE, 0);
    l = ui_label(note, "hint");
    gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(inner), l, FALSE, FALSE, 0);
    gtk_widget_set_hexpand(card, TRUE);
    gtk_widget_set_size_request(card, -1, 104);
    gtk_grid_attach(GTK_GRID(grid), card, idx % 3, idx / 3, 1, 1);
}

/* Fixed header: name, roll, branch, section, semester and the overall result on the right. */
static void build_view_header(GtkWidget *holder, int idx)
{
    Student *s = &students[idx];
    SheetRow sr;
    GtkWidget *card, *inner, *row, *l;
    char t[200];
    gtk_container_foreach(GTK_CONTAINER(holder), clear_children, NULL);
    marks_sheet_row(s->roll, EXAM_ALL, &sr);
    card = ui_card(&inner);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    l = ui_label(s->name, "page-title");
    gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(row), l, TRUE, TRUE, 0);
    snprintf(t, sizeof t, "Overall result: %s", !sr.any ? "-" : sr.pass ? "Pass" : "Fail");
    l = ui_label(t, "card-head");
    set_cls(l, !sr.any ? NULL : sr.pass ? "good" : "warn");
    gtk_box_pack_end(GTK_BOX(row), l, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);
    snprintf(t, sizeof t, "Roll No: %s    |    Branch: %s    |    Section: %s    |    Semester: %d", s->roll, s->branch, s->section, s->semester);
    l = ui_label(t, "page-sub");
    gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(inner), l, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(holder), card, FALSE, FALSE, 0);
    gtk_widget_show_all(holder);
}

/* Tooltip of the examination table: full subject name and code. */
static gboolean on_view_tip(GtkWidget *w, gint x, gint y, gboolean kb, GtkTooltip *tip, gpointer d)
{
    GtkTreeView *tv = GTK_TREE_VIEW(w);
    GtkTreeModel *m = gtk_tree_view_get_model(tv);
    GtkTreePath *p = NULL;
    GtkTreeViewColumn *col = NULL;
    GtkTreeIter it;
    gchar *text = NULL;
    int bx, by;
    (void)kb; (void)d;
    gtk_tree_view_convert_widget_to_bin_window_coords(tv, x, y, &bx, &by);
    if (!gtk_tree_view_get_path_at_pos(tv, bx, by, &p, &col, NULL, NULL)) return FALSE;
    if (!gtk_tree_model_get_iter(m, &it, p)) { gtk_tree_path_free(p); return FALSE; }
    gtk_tree_model_get(m, &it, 27, &text, -1);                      /* 3 x 9 columns -> tooltip column */
    if (text && text[0]) {
        gtk_tooltip_set_text(tip, text);
        gtk_tree_view_set_tooltip_cell(tv, tip, p, col, NULL);
        g_free(text); gtk_tree_path_free(p);
        return TRUE;
    }
    g_free(text); gtk_tree_path_free(p);
    return FALSE;
}

/* Scrolling content: nine cards (3 x 3) and the examination marks table. */
static void build_view_body(GtkWidget *holder, int idx)
{
    Student *s = &students[idx];
    GtkWidget *card, *inner, *grid, *row, *l, *tv;
    GtkListStore *st;
    SheetRow sr;
    const char *cells[9], *fg[9], *bg[9];
    char heads_t[9][24], c[9][28], t[200], sub[120], leg[300], tip[160];
    const char *heads[9];
    float xal[9] = { 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 1.0f, 1.0f, 0.5f };
    double sum_e[3] = { 0, 0, 0 }, max_e[3] = { 0, 0, 0 }, tot, mx;
    int j, e, rank, ne = n_exams < 3 ? n_exams : 3, conducted = 0, attended = 0, shown = 0;
    GList *cols, *cl;

    gtk_container_foreach(GTK_CONTAINER(holder), clear_children, NULL);
    marks_sheet_row(s->roll, EXAM_ALL, &sr);
    student_exam_pct(s->roll, EXAM_ALL, &tot, &mx);
    rank = sr.any ? student_rank(s->roll, EXAM_ALL) : 0;
    att_stats(s->roll, NULL, &conducted, &attended);                   /* all subjects, Environmental Science included */

    /* ---- nine summary cards: 3 columns x 3 rows, equal size ---- */
    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 14);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 14);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);

    if (sr.any) snprintf(t, sizeof t, "%.1f / %.0f", tot, sr.max); else strcpy(t, "-");
    if (n_subjects == 6) strcpy(sub, "All six subjects"); else snprintf(sub, sizeof sub, "All %d subjects", n_subjects);
    card_in(grid, 0, ICON_MARKS, "Total Marks", t, sub, -1);
    if (sr.pct >= 0) snprintf(t, sizeof t, "%.2f%%", sr.pct); else strcpy(t, "-");
    card_in(grid, 1, ICON_PERCENT, "Overall Percentage", t, sr.pct >= 0 ? grade_performance(grade_of(sr.pct)) : "No marks", -1);
    if (sr.sgpa >= 0) snprintf(t, sizeof t, "%.2f", sr.sgpa); else strcpy(t, "-");
    card_in(grid, 2, ICON_GRAD, "SGPA", t, "Out of 10", -1);
    if (rank > 0) snprintf(t, sizeof t, "%d / %d", rank, n_students); else strcpy(t, "-");
    card_in(grid, 3, ICON_TROPHY, "Class Rank", t, "Based on overall marks", -1);
    snprintf(t, sizeof t, "%d / %d", sr.earned, total_credits());
    card_in(grid, 4, ICON_CREDITS, "Earned Credits", t, "Credits earned", sr.any ? (sr.earned == total_credits()) : -1);
    if (sr.any) snprintf(t, sizeof t, "%d / %d", sr.passed, n_subjects); else strcpy(t, "-");
    card_in(grid, 5, ICON_CHECK_CIRCLE, "Subjects Passed", t, "Successfully passed", sr.any ? 1 : -1);
    if (sr.any) snprintf(t, sizeof t, "%d / %d", sr.failed, n_subjects); else strcpy(t, "-");
    card_in(grid, 6, ICON_CROSS_CIRCLE, "Subjects Failed", t, sr.failed > 0 ? "Below passing marks" : "No failed subjects", sr.any ? (sr.failed == 0) : -1);
    if (conducted > 0) {
        snprintf(t, sizeof t, "%d / %d", attended, conducted);
        snprintf(sub, sizeof sub, "%.2f%% attendance", attended * 100.0 / conducted);
    } else { strcpy(t, "-"); strcpy(sub, "No classes recorded"); }
    card_in(grid, 7, ICON_CALENDAR, "Classes Attended", t, sub,
            conducted > 0 ? (attended * 100.0 / conducted >= settings.att_threshold) : -1);
    card_in(grid, 8, ICON_BLOOD, "Blood Group", s->blood[0] ? s->blood : "Not specified", "Student information", -1);
    gtk_box_pack_start(GTK_BOX(holder), grid, FALSE, FALSE, 0);

    /* ---- examination marks table ---- */
    card = ui_card(&inner);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(row), ui_label("Examination Marks", "card-head"), FALSE, FALSE, 0);
    snprintf(t, sizeof t, "Semester %d", s->semester);
    gtk_box_pack_end(GTK_BOX(row), ui_label(t, "hint"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);

    snprintf(heads_t[0], sizeof heads_t[0], "Subject");
    for (e = 0; e < 3; e++) {
        if (e >= ne) snprintf(heads_t[1 + e], sizeof heads_t[0], "-");
        else snprintf(heads_t[1 + e], sizeof heads_t[0], "%.8s (%.0f)", exams[e].name, exams[e].max_marks);
    }
    snprintf(heads_t[4], sizeof heads_t[0], "Total (%.0f)", n_subjects > 0 ? subjects[0].max_marks : 100.0);
    snprintf(heads_t[5], sizeof heads_t[0], "Grade");
    snprintf(heads_t[6], sizeof heads_t[0], "GP");
    snprintf(heads_t[7], sizeof heads_t[0], "Cr.");
    snprintf(heads_t[8], sizeof heads_t[0], "Result");
    for (j = 0; j < 9; j++) heads[j] = heads_t[j];

    st = ui_cell_store_new(9);
    tv = ui_cell_tree_new(st, heads, 9, xal, 40);
    gtk_widget_set_can_focus(tv, FALSE);                              /* read-only: focus must not scroll the window */
    g_object_unref(st);
    for (j = 0; j < n_subjects; j++) {
        double stt = 0, sm = 0, sp = subject_pct(s->roll, j);
        int has = sp >= 0, ok = subject_pass(s->roll, j);
        const char *gname = has ? grade_of(sp) : "-";
        subject_total(s->roll, j, &stt, &sm);
        snprintf(c[0], sizeof c[0], "%s", subjects[j].short_name[0] ? subjects[j].short_name : subjects[j].code);
        for (e = 0; e < 3; e++) {
            int k = e < ne ? marks_find(s->roll, subjects[j].code, e) : -1;
            if (k >= 0) { snprintf(c[1 + e], sizeof c[0], "%.1f", marks[k].marks); sum_e[e] += marks[k].marks; max_e[e] += exams[e].max_marks; }
            else snprintf(c[1 + e], sizeof c[0], "-");
        }
        if (has) { snprintf(c[4], sizeof c[0], "%.1f", stt); snprintf(c[6], sizeof c[0], "%.0f", ok ? grade_points_of(sp) : 0.0); }
        else { strcpy(c[4], "-"); strcpy(c[6], "-"); }
        snprintf(c[5], sizeof c[0], "%s", gname);
        snprintf(c[7], sizeof c[0], "%d", subjects[j].credits);
        snprintf(c[8], sizeof c[0], "%s", !has ? "-" : ok ? "Pass" : "Fail");
        for (e = 0; e < 9; e++) { cells[e] = c[e]; fg[e] = "#14213D"; bg[e] = (shown % 2) ? "#F5F6FB" : ""; }
        if (has && !ok) { fg[4] = "#D93636"; fg[5] = "#D93636"; fg[8] = "#D93636"; }     /* only total, grade and result */
        if (has && ok) fg[8] = "#1E9E5A";
        snprintf(tip, sizeof tip, "%s (%s)", subjects[j].name, subjects[j].code);
        ui_cell_store_add(st, 9, cells, fg, bg, tip, j);
        shown++;
    }
    for (e = 0; e < 3; e++) snprintf(c[1 + e], sizeof c[0], "%.1f / %.0f", sum_e[e], e < ne ? n_subjects * exams[e].max_marks : 0.0);
    snprintf(c[0], sizeof c[0], "Total");
    snprintf(c[4], sizeof c[0], "%.1f / %.0f", tot, sr.max);
    snprintf(c[5], sizeof c[0], "\xE2\x80\x94");
    snprintf(c[6], sizeof c[0], "\xE2\x80\x94");
    snprintf(c[7], sizeof c[0], "%d / %d", sr.earned, total_credits());
    snprintf(c[8], sizeof c[0], "%s", !sr.any ? "-" : sr.pass ? "Pass" : "Fail");
    for (e = 0; e < 9; e++) { cells[e] = c[e]; fg[e] = "#14213D"; bg[e] = "#EDE7FF"; }   /* total row: light purple */
    if (sr.any) fg[8] = sr.pass ? "#1E9E5A" : "#D93636";
    ui_cell_store_add(st, 9, cells, fg, bg, "", n_subjects);
    (void)max_e;

    cols = gtk_tree_view_get_columns(GTK_TREE_VIEW(tv));              /* bold: subject and total columns */
    for (cl = cols, j = 0; cl; cl = cl->next, j++) {
        GList *rl = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(cl->data));
        if (rl && (j == 0 || j == 4)) g_object_set(rl->data, "weight", PANGO_WEIGHT_BOLD, NULL);
        g_list_free(rl);
    }
    g_list_free(cols);
    gtk_tree_view_column_set_min_width(gtk_tree_view_get_column(GTK_TREE_VIEW(tv), 0), 60);
    g_signal_connect(tv, "query-tooltip", G_CALLBACK(on_view_tip), NULL);
    gtk_box_pack_start(GTK_BOX(inner), tv, FALSE, FALSE, 0);

    grade_legend(leg, sizeof leg);
    snprintf(t, sizeof t, "Grades: %s.   Hover a subject for its full name.", leg);
    l = ui_label(t, "hint");
    gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_label_set_max_width_chars(GTK_LABEL(l), 60);
    gtk_box_pack_start(GTK_BOX(inner), l, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(holder), card, FALSE, FALSE, 0);
    gtk_widget_show_all(holder);
}

/* screenshot mode only: capture the window, then close it */
static gboolean shot_close_view(gpointer d)
{
    ui_shot_save(GTK_WIDGET(d), "09_view_student");
    gtk_dialog_response(GTK_DIALOG(d), GTK_RESPONSE_CLOSE);
    return G_SOURCE_REMOVE;
}

static void show_student_window(int idx)
{
    GtkWidget *dlg, *area, *hdr, *sw, *body, *edit;
    GdkScreen *scr = gdk_screen_get_default();
    char t[120];
    int h = 700;
    snprintf(t, sizeof t, "Student Marks - %s", students[idx].name);
    dlg = gtk_dialog_new_with_buttons(t, GTK_WINDOW(g_window), GTK_DIALOG_MODAL, "Edit Marks", RESP_EDIT,
                                      "Close", GTK_RESPONSE_CLOSE, NULL);
    if (scr && gdk_screen_get_height(scr) - 120 < h) h = gdk_screen_get_height(scr) - 120;     /* fits 1366 x 768 */
    gtk_window_set_default_size(GTK_WINDOW(dlg), 920, h);
    gtk_window_set_resizable(GTK_WINDOW(dlg), TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(gtk_dialog_get_action_area(GTK_DIALOG(dlg))), 10);

    edit = gtk_dialog_get_widget_for_response(GTK_DIALOG(dlg), RESP_EDIT);            /* purple primary button */
    {   /* pencil icon + text on the primary button */
        GtkWidget *old = gtk_bin_get_child(GTK_BIN(edit)), *bx = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);
        gtk_container_remove(GTK_CONTAINER(edit), old);
        gtk_box_pack_start(GTK_BOX(bx), ui_icon_new(ICON_EDIT, 17, "#FFFFFF", NULL), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(bx), gtk_label_new("Edit Marks"), FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(edit), bx);
    }
    gtk_style_context_add_class(gtk_widget_get_style_context(edit), "accent");

    area = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    hdr = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);                                    /* FIXED header */
    gtk_container_set_border_width(GTK_CONTAINER(hdr), 14);
    gtk_box_pack_start(GTK_BOX(area), hdr, FALSE, FALSE, 0);

    sw = gtk_scrolled_window_new(NULL, NULL);                                          /* the ONE scrolling area */
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(sw), FALSE);         /* scrollbar always visible */
    body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_container_set_border_width(GTK_CONTAINER(body), 14);
    gtk_container_add(GTK_CONTAINER(sw), body);
    gtk_box_pack_start(GTK_BOX(area), sw, TRUE, TRUE, 0);

    build_view_header(hdr, idx);
    build_view_body(body, idx);
    gtk_widget_show_all(dlg);
    gtk_widget_grab_focus(edit);
    if (getenv("CLASSMETRICS_SHOT_DIR")) g_timeout_add(1500, shot_close_view, dlg);
    while (gtk_dialog_run(GTK_DIALOG(dlg)) == RESP_EDIT)
        if (edit_student(idx)) { build_view_header(hdr, idx); build_view_body(body, idx); }   /* refresh after saving */
    gtk_widget_destroy(dlg);
}

void marks_open_view_for_shot(int idx) { if (idx < n_students) show_student_window(idx); }

static void on_view(GtkWidget *w, gpointer d)
{
    (void)w; (void)d;
    if (sel_i < 0 || sel_i >= n_students) { ui_info("Select a student row in the table first."); return; }
    show_student_window(sel_i);
}

static void on_edit(GtkWidget *w, gpointer d)
{
    (void)w; (void)d;
    if (sel_i < 0 || sel_i >= n_students) { ui_info("Select a student row in the table first."); return; }
    edit_student(sel_i);
}

/* Export CSV: asks which examination, then writes a spreadsheet (one row per student, one column per subject). */
static void on_export(GtkWidget *w, gpointer d)
{
    static const char *names[4] = { "marks_CIE1.csv", "marks_CIE2.csv", "marks_SEE.csv", "marks_CIE1_CIE2_SEE.csv" };
    GtkWidget *dlg, *combo, *box, *l;
    char *path, msg[400];
    int choice, rows, resp;
    (void)w; (void)d;
    dlg = gtk_dialog_new_with_buttons("Export marks", GTK_WINDOW(g_window), GTK_DIALOG_MODAL,
                                      "Cancel", GTK_RESPONSE_CANCEL, "Export", GTK_RESPONSE_OK, NULL);
    box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_container_set_border_width(GTK_CONTAINER(box), 16);
    gtk_box_set_spacing(GTK_BOX(box), 10);
    l = ui_label("Which examination do you want to export?", NULL);
    gtk_box_pack_start(GTK_BOX(box), l, FALSE, FALSE, 0);
    combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "CIE 1 (max 20)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "CIE 2 (max 20)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "SEE (max 60)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "All three (CIE 1, CIE 2 and SEE side by side)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    gtk_box_pack_start(GTK_BOX(box), combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), ui_label("One row per student, one column per subject. Edit the marks in Excel, save as CSV and use Import CSV.", "hint"), FALSE, FALSE, 0);
    gtk_widget_show_all(dlg);
    resp = gtk_dialog_run(GTK_DIALOG(dlg));
    choice = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    gtk_widget_destroy(dlg);
    if (resp != GTK_RESPONSE_OK || choice < 0) return;
    path = ui_choose_file(1, "Export marks as CSV", names[choice]);
    if (path == NULL) return;
    if (marks_export_csv(path, choice, &rows) == 0) {
        snprintf(msg, sizeof msg, "Exported %d students.\n\nEdit the marks in Excel (keep the Roll No column and the column titles), "
                 "save as CSV, then press Import CSV to update the marks. Blank cells keep the existing mark.", rows);
        ui_info(msg);
    } else ui_error("Could not write the file. Choose another folder or file name.");
    g_free(path);
}

static void on_import(GtkWidget *w, gpointer d)
{
    char *path, msg[400], prob[160];
    int added, skipped;
    (void)w; (void)d;
    path = ui_choose_file(0, "Import marks from CSV (Roll No, Student Name, then one column per subject)", NULL);
    if (path == NULL) return;
    if (marks_import_csv(path, &added, &skipped, prob) == 0) {
        snprintf(msg, sizeof msg, "Imported marks for %d row(s), skipped %d.%s%s", added, skipped, skipped ? "\nFirst problem: " : "", skipped ? prob : "");
        if (added > 0) { ui_save(); ui_refresh_all(); }
        ui_info(msg);
    } else ui_error("Could not read that file.");
    g_free(path);
}

/* ======================= page ======================= */

GtkWidget *marks_build(void)
{
    static const char *h2[10] = { "Rank", "Roll No", "Name", "Total", "Max", "Percent", "Grade", "SGPA", "Credits earned", "Result" };
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12), *nb, *card, *inner, *row, *top;
    char leg[300], t[400];
    int i;

    gtk_box_pack_start(GTK_BOX(page), ui_title("Marks", "One row per student: subject totals out of 100 (CIE 1 + CIE 2 + SEE), overall out of 600"), FALSE, FALSE, 0);
    nb = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(page), nb, TRUE, TRUE, 0);

    /* ---- tab 1: marks sheet ---- */
    card = ui_card(&inner);
    top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    e_search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_search), "Search name or roll no");
    gtk_widget_set_size_request(e_search, 190, -1);
    g_signal_connect(e_search, "search-changed", G_CALLBACK(on_filter), NULL);
    c_stu = gtk_combo_box_text_new();
    c_sub = gtk_combo_box_text_new();
    c_exam = gtk_combo_box_text_new();
    g_signal_connect(c_stu, "changed", G_CALLBACK(on_filter), NULL);
    g_signal_connect(c_sub, "changed", G_CALLBACK(on_filter), NULL);
    g_signal_connect(c_exam, "changed", G_CALLBACK(on_filter), NULL);
    gtk_box_pack_start(GTK_BOX(top), e_search, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(top), ui_labeled("Student:", c_stu), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(top), ui_labeled("Subject:", c_sub), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), top, FALSE, FALSE, 0);
    top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    c_grade = gtk_combo_box_text_new();
    c_stat = gtk_combo_box_text_new();
    for (i = 0; i < 8; i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c_grade), GRADE_ITEMS[i]);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c_stat), "All Results");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c_stat), "Pass");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c_stat), "Fail");
    gtk_combo_box_set_active(GTK_COMBO_BOX(c_grade), 0);
    gtk_combo_box_set_active(GTK_COMBO_BOX(c_stat), 0);
    g_signal_connect(c_grade, "changed", G_CALLBACK(on_filter), NULL);
    g_signal_connect(c_stat, "changed", G_CALLBACK(on_filter), NULL);
    gtk_box_pack_start(GTK_BOX(top), ui_labeled("Examination:", c_exam), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(top), ui_labeled("Grade:", c_grade), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(top), ui_labeled("Result:", c_stat), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), top, FALSE, FALSE, 0);
    l_hint = ui_label("", "hint");
    gtk_label_set_line_wrap(GTK_LABEL(l_hint), TRUE);
    gtk_box_pack_start(GTK_BOX(inner), l_hint, FALSE, FALSE, 0);

    top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(top), ui_icon_button(ICON_USER_CHECK, "View Student", "accent", G_CALLBACK(on_view), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(top), ui_icon_button(ICON_EDIT, "Edit Marks", NULL, G_CALLBACK(on_edit), NULL), FALSE, FALSE, 0);
    l_count = ui_label("", "hint");
    gtk_box_pack_start(GTK_BOX(top), l_count, FALSE, FALSE, 12);
    gtk_box_pack_end(GTK_BOX(top), ui_icon_button(ICON_IMPORT, "Import CSV", NULL, G_CALLBACK(on_import), NULL), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(top), ui_icon_button(ICON_EXPORT, "Export CSV", NULL, G_CALLBACK(on_export), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), top, FALSE, FALSE, 0);

    sheet_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);            /* the table is rebuilt inside this box */
    gtk_box_pack_start(GTK_BOX(inner), sheet_box, TRUE, TRUE, 0);

    grade_legend(leg, sizeof leg);
    snprintf(t, sizeof t, "Grades: %s.   S Outstanding, A Excellent, B Very Good, C Good, D Average, E Pass, F Fail.", leg);
    row = ui_label(t, "hint");
    gtk_label_set_line_wrap(GTK_LABEL(row), TRUE);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Hover a subject heading for its full name. Select a student and press View Student (or double-click) for details; "
                                                "Edit Marks changes the marks. Failed subject marks are shown in red; hover Fail (n) to see which subjects failed.", "hint"), FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), card, gtk_label_new("Marks sheet"));

    /* ---- tab 2: ranking ---- */
    card = ui_card(&inner);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    c_rank = gtk_combo_box_text_new();
    g_signal_connect(c_rank, "changed", G_CALLBACK(on_rank_changed), NULL);
    gtk_box_pack_start(GTK_BOX(row), ui_labeled("Ranking based on:", c_rank), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);
    st_rank = ui_store_new(10);
    gtk_box_pack_start(GTK_BOX(inner), ui_tree_new(st_rank, h2, 10, NULL, 360), TRUE, TRUE, 0);
    l_rank_stats = ui_label("", "hint");
    gtk_box_pack_start(GTK_BOX(inner), l_rank_stats, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Percent = total marks / total maximum x 100. SGPA = sum(credits x grade points) / sum(credits); Environmental Science (0 credits) is excluded from SGPA. Equal percentages share a rank.", "hint"), FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), card, gtk_label_new("Ranking & summary"));
    ui_shot_tab(nb);                                        /* screenshot aid */
    return page;
}
