/*
 * ui_profile.c : graphical student performance profile (opened from the
 * Students page by "View profile" or by double-clicking a row).
 *
 * Sections: header with initials avatar, 8 summary cards, examination trend
 * graph, subject-wise marks table (CIE 1, CIE 2, SEE, total, grade, grade
 * points, credits, status) and subject-wise attendance bars.
 * Everything is calculated from the stored records.
 */
#include "ui.h"

/* state of the profile that is currently open (the dialog is modal) */
static char initials[4];
static const char *tr_lab[MAX_EXAMS];
static double tr_val[MAX_EXAMS];
static int tr_n;

/* ---------- avatar: purple circle with initials ---------- */
static gboolean draw_avatar(GtkWidget *w, cairo_t *cr, gpointer d)
{
    cairo_text_extents_t e;
    (void)w; (void)d;
    cairo_set_source_rgb(cr, 0.427, 0.290, 1.0);
    cairo_arc(cr, 36, 36, 34, 0, 6.2832);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_select_font_face(cr, "Segoe UI", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 26);
    cairo_text_extents(cr, initials, &e);
    cairo_move_to(cr, 36 - e.width / 2 - e.x_bearing, 36 - e.height / 2 - e.y_bearing);
    cairo_show_text(cr, initials);
    return FALSE;
}

static gboolean draw_trend(GtkWidget *w, cairo_t *cr, gpointer d)
{
    (void)d;
    ui_draw_line(cr, gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w),
                 tr_lab, tr_val, tr_n, "Examination performance (%)");
    return FALSE;
}

/* ---------- small helpers ---------- */
/* good: 1 green, 0 red, -1 neutral */
static void summary_card(GtkWidget *grid, int idx, const char *caption, const char *value, const char *note, int good)
{
    GtkWidget *inner, *card = ui_card(&inner), *v = ui_label(value, "card-value");
    gtk_box_pack_start(GTK_BOX(inner), ui_label(caption, "card-label"), FALSE, FALSE, 0);
    if (strlen(value) > 11) gtk_style_context_add_class(gtk_widget_get_style_context(v), "card-value-sm");
    if (good >= 0) gtk_style_context_add_class(gtk_widget_get_style_context(v), good ? "good" : "warn");
    gtk_label_set_ellipsize(GTK_LABEL(v), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(inner), v, FALSE, FALSE, 0);
    if (note && note[0]) gtk_box_pack_start(GTK_BOX(inner), ui_label(note, "hint"), FALSE, FALSE, 0);
    gtk_widget_set_hexpand(card, TRUE);
    gtk_grid_attach(GTK_GRID(grid), card, idx % 4, idx / 4, 1, 1);
}

/* "CIE 1 (20)" style column heading for examination e. */
static void exam_head(int e, char *out, size_t cap)
{
    if (e >= n_exams) { snprintf(out, cap, "-"); return; }
    else snprintf(out, cap, "%.12s (%.0f)", exams[e].name, exams[e].max_marks);
}

/* One attendance row: label, progress bar, "attended / total  percent  category". */
static void attendance_row(GtkWidget *box, const char *label, int att, int tot, int bold)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10), *bar = gtk_progress_bar_new(), *l, *v;
    char t[100];
    double p = tot > 0 ? att * 100.0 / tot : 0;
    l = ui_label(label, bold ? "card-head" : NULL);
    gtk_widget_set_size_request(l, 260, -1);
    gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    gtk_widget_set_tooltip_text(l, label);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(bar), p / 100.0);
    gtk_widget_set_hexpand(bar, TRUE);
    gtk_widget_set_valign(bar, GTK_ALIGN_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(bar), p < settings.att_threshold ? "att-low" : "att-good");
    if (tot > 0) snprintf(t, sizeof t, "%d / %d classes   %.1f%%   %s", att, tot, p, att_category_name(att_category(p)));
    else snprintf(t, sizeof t, "No records");
    v = ui_label(t, tot > 0 ? (p < settings.att_threshold ? "warn" : "good") : "hint");
    gtk_widget_set_size_request(v, 270, -1);
    gtk_box_pack_start(GTK_BOX(row), l, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), bar, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), v, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 3);
}

/* ---------- the window ---------- */
void student_profile_dialog(int idx)
{
    Student *s;
    GtkWidget *dlg, *sw, *page, *card, *inner, *row, *av, *info, *grid, *da, *tree;
    GtkListStore *st;
    const char *heads[9];
    char hb[3][24], t[200], title[120], *w;
    double pct[MAX_EXAMS], op, tot, mx, sg, ap;
    int ex[MAX_EXAMS], n, i, j, c, a, rank, earned = 0, cred = 0, failed = 0, pass;

    if (idx < 0 || idx >= n_students) return;
    s = &students[idx];

    /* initials: first letter of the first and the last word of the name */
    memset(initials, 0, sizeof initials);
    initials[0] = (char)toupper((unsigned char)s->name[0]);
    w = strrchr(s->name, ' ');
    if (w != NULL && w[1] != '\0') initials[1] = (char)toupper((unsigned char)w[1]);

    n = student_trend(s->roll, pct, ex);
    tr_n = n;
    for (i = 0; i < n; i++) { tr_lab[i] = exams[ex[i]].name; tr_val[i] = pct[i]; }

    op = student_exam_pct(s->roll, EXAM_ALL, &tot, &mx);
    sg = student_sgpa(s->roll, &earned, &cred);
    rank = op >= 0 ? student_rank(s->roll, EXAM_ALL) : 0;
    ap = att_pct(s->roll, NULL);
    for (j = 0; j < n_subjects; j++) if (subject_pct(s->roll, j) >= 0 && !subject_pass(s->roll, j)) failed++;
    pass = op >= 0 && failed == 0;

    snprintf(title, sizeof title, "Student Profile - %s", s->name);
    dlg = gtk_dialog_new_with_buttons(title, GTK_WINDOW(g_window), GTK_DIALOG_MODAL, "Close", GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 900, 720);
    gtk_window_set_resizable(GTK_WINDOW(dlg), TRUE);

    page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_container_set_border_width(GTK_CONTAINER(page), 16);

    /* 1. header */
    card = ui_card(&inner);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    av = gtk_drawing_area_new();
    gtk_widget_set_size_request(av, 72, 72);
    gtk_widget_set_valign(av, GTK_ALIGN_CENTER);
    g_signal_connect(av, "draw", G_CALLBACK(draw_avatar), NULL);
    info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_valign(info, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(info), ui_label(s->name, "page-title"), FALSE, FALSE, 0);
    snprintf(t, sizeof t, "Roll No: %s     Branch: %s     Section: %s     Semester: %d     Blood Group: %s", s->roll, s->branch, s->section, s->semester,
             s->blood[0] ? s->blood : "Not specified");
    gtk_box_pack_start(GTK_BOX(info), ui_label(t, "page-sub"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), av, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), info, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), card, FALSE, FALSE, 0);

    /* 2. academic summary cards */
    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    if (op >= 0) snprintf(t, sizeof t, "%.1f / %.0f", tot, mx); else strcpy(t, "-");
    summary_card(grid, 0, "Total marks", t, "CIE 1 + CIE 2 + SEE, all subjects", -1);
    if (op >= 0) snprintf(t, sizeof t, "%.2f%%", op); else strcpy(t, "-");
    summary_card(grid, 1, "Overall percentage", t, op >= 0 ? grade_of(op) : "", op >= 0 ? op >= 50 : -1);
    if (sg >= 0) snprintf(t, sizeof t, "%.2f", sg); else strcpy(t, "-");
    summary_card(grid, 2, "SGPA", t, "credit weighted, out of 10", sg >= 0 ? sg >= 5 : -1);
    if (rank > 0) snprintf(t, sizeof t, "%d of %d", rank, n_students); else strcpy(t, "-");
    summary_card(grid, 3, "Class rank", t, "by overall percentage", -1);
    if (ap >= 0) snprintf(t, sizeof t, "%.1f%%", ap); else strcpy(t, "-");
    summary_card(grid, 4, "Overall attendance", t, ap >= 0 ? att_category_name(att_category(ap)) : "", ap >= 0 ? ap >= settings.att_threshold : -1);
    snprintf(t, sizeof t, "%d / %d", earned, total_credits());
    summary_card(grid, 5, "Earned credits", t, "passed credit-bearing subjects", op >= 0 ? earned == total_credits() : -1);
    if (op < 0) strcpy(t, "-");
    else if (pass) strcpy(t, "Pass");
    else snprintf(t, sizeof t, "Backlog (%d)", failed);
    summary_card(grid, 6, "Academic status", t, op >= 0 ? (pass ? "all subjects cleared" : "subject(s) below passing marks") : "", op >= 0 ? pass : -1);
    summary_card(grid, 7, "Overall grade", op >= 0 ? grade_of(op) : "-", "from overall percentage", op >= 0 ? op >= 40 : -1);
    gtk_box_pack_start(GTK_BOX(page), grid, FALSE, FALSE, 0);

    /* 3. trend graph */
    card = ui_card(&inner);
    da = ui_chart_area(G_CALLBACK(draw_trend), 400, 230);
    gtk_box_pack_start(GTK_BOX(inner), da, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(page), card, FALSE, FALSE, 0);

    /* 4. subject-wise marks */
    card = ui_card(&inner);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Subject-wise marks", "card-head"), FALSE, FALSE, 0);
    for (i = 0; i < 3; i++) exam_head(i, hb[i], sizeof hb[0]);
    heads[0] = "Subject"; heads[1] = hb[0]; heads[2] = hb[1]; heads[3] = hb[2]; heads[4] = "Total (100)";
    heads[5] = "Grade"; heads[6] = "Grade pts"; heads[7] = "Credits"; heads[8] = "Status";
    st = ui_store_new(9);
    for (j = 0; j < n_subjects; j++) {
        char m[3][16], tt[16], cr[8], gp[8];
        double st_, sm, sp = subject_pct(s->roll, j);
        int ok = subject_pass(s->roll, j);
        for (i = 0; i < 3; i++) {
            int k = i < n_exams ? marks_find(s->roll, subjects[j].code, i) : -1;
            if (k >= 0) snprintf(m[i], sizeof m[i], "%.1f", marks[k].marks); else strcpy(m[i], "-");
        }
        subject_total(s->roll, j, &st_, &sm);
        snprintf(cr, sizeof cr, "%d", subjects[j].credits);
        if (sp >= 0) { snprintf(tt, sizeof tt, "%.1f", st_); snprintf(gp, sizeof gp, "%.0f", ok ? grade_points_of(sp) : 0.0); }
        else { strcpy(tt, "-"); strcpy(gp, "-"); }
        snprintf(t, sizeof t, "%s (%s)", subjects[j].name, subjects[j].short_name);
        ui_store_add(st, 9, (sp >= 0 && !ok) ? "#D93636" : NULL, j, t, m[0], m[1], m[2], tt,
                     sp >= 0 ? grade_of(sp) : "-", gp, cr, sp < 0 ? "-" : ok ? "Pass" : "Fail");
    }
    tree = ui_tree_new(st, heads, 9, NULL, 36 + 30 * (n_subjects > 0 ? n_subjects : 1));
    g_object_unref(st);
    gtk_box_pack_start(GTK_BOX(inner), tree, FALSE, FALSE, 0);
    if (op >= 0) {
        snprintf(t, sizeof t, "Total: %.1f / %.0f  (%.2f%%)     SGPA: %.2f     Earned credits: %d / %d     Result: %s",
                 tot, mx, op, sg >= 0 ? sg : 0.0, earned, total_credits(), pass ? "PASS" : "BACKLOG");
        gtk_box_pack_start(GTK_BOX(inner), ui_label(t, "card-head"), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(inner), ui_label("Environmental Science carries 0 credits: it counts in marks, percentage, attendance and pass/fail, but not in SGPA.", "hint"), FALSE, FALSE, 0);
        {
            char leg[300], lt[340];
            GtkWidget *ll;
            grade_legend(leg, sizeof leg);
            snprintf(lt, sizeof lt, "Grades: %s", leg);
            ll = ui_label(lt, "hint");
            gtk_label_set_line_wrap(GTK_LABEL(ll), TRUE);
            gtk_box_pack_start(GTK_BOX(inner), ll, FALSE, FALSE, 0);
        }
    }
    gtk_box_pack_start(GTK_BOX(page), card, FALSE, FALSE, 0);

    /* 5. attendance */
    card = ui_card(&inner);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Attendance", "card-head"), FALSE, FALSE, 0);
    snprintf(t, sizeof t, "Green = at or above %.0f%%, red = below the threshold.", settings.att_threshold);
    gtk_box_pack_start(GTK_BOX(inner), ui_label(t, "hint"), FALSE, FALSE, 0);
    for (i = 0; i < n_subjects; i++) {
        att_stats(s->roll, subjects[i].code, &c, &a);
        snprintf(t, sizeof t, "%s - %s", subjects[i].short_name, subjects[i].name);
        attendance_row(inner, t, a, c, 0);
    }
    att_stats(s->roll, NULL, &c, &a);
    attendance_row(inner, "Overall attendance", a, c, 1);
    if (c > 0 && att_is_low(s->roll))
        gtk_box_pack_start(GTK_BOX(inner), ui_label("Warning: overall attendance is below the required threshold.", "warn"), FALSE, FALSE, 4);
    for (i = 0; i < n_subjects; i++)
        if (att_subject_low(s->roll, subjects[i].code)) {
            snprintf(t, sizeof t, "Subject warning: attendance in %s is below the threshold.", subjects[i].name);
            gtk_box_pack_start(GTK_BOX(inner), ui_label(t, "warn"), FALSE, FALSE, 0);
        }
    gtk_box_pack_start(GTK_BOX(page), card, FALSE, FALSE, 0);

    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(sw), page);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), sw, TRUE, TRUE, 0);
    gtk_widget_show_all(dlg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}
