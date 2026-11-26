/*
 * ui_subjects.c : Subjects page - subject table (code, name, faculty, credits,
 * maximum marks, type), add / edit / delete, and the subject profile window
 * (faculty, examination pattern and the five syllabus units).
 */
#include "ui.h"

static GtkWidget *flow = NULL, *l_summary = NULL;
static int sel_idx = -1;

/* icon and soft accent colours per kind of subject (decoration only) */
static int subject_icon(const Subject *s)
{
    if (strstr(s->name, "Math")) return ICON_MATH;
    if (strstr(s->name, "Chem")) return ICON_FLASK;
    if (strstr(s->name, "Program")) return ICON_CODE;
    if (strstr(s->name, "English")) return ICON_CHAT;
    if (strstr(s->name, "Human")) return ICON_HEART;
    if (strstr(s->name, "Environ")) return ICON_LEAF;
    return ICON_BOOK_OPEN;
}

static void subject_colors(int icon, const char **fg, const char **bg)
{
    switch (icon) {
        case ICON_MATH:  *fg = "#6842F5"; *bg = "#EDE9FE"; break;
        case ICON_FLASK: *fg = "#0284C7"; *bg = "#E0F2FE"; break;
        case ICON_CODE:  *fg = "#16A34A"; *bg = "#DCFCE7"; break;
        case ICON_CHAT:  *fg = "#D97706"; *bg = "#FEF3C7"; break;
        case ICON_HEART: *fg = "#DB2777"; *bg = "#FCE7F3"; break;
        case ICON_LEAF:  *fg = "#0D9488"; *bg = "#CCFBF1"; break;
        default:         *fg = "#6842F5"; *bg = "#EDE9FE"; break;
    }
}

/* One subject card: icon, abbreviation, code, name, faculty and credits. */
static GtkWidget *subject_card(int i)
{
    Subject *s = &subjects[i];
    GtkWidget *inner, *card = ui_card(&inner), *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10), *row, *l, *col;
    const char *fg, *bg;
    char t[100];
    int icon = subject_icon(s);
    subject_colors(icon, &fg, &bg);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "subject-card");

    gtk_box_pack_start(GTK_BOX(top), ui_icon_new(icon, 46, fg, bg), FALSE, FALSE, 0);
    col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    l = ui_label(s->short_name[0] ? s->short_name : s->code, "card-head");
    gtk_box_pack_start(GTK_BOX(col), l, FALSE, FALSE, 0);
    l = ui_label(s->code, "hint");
    gtk_box_pack_start(GTK_BOX(col), l, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(top), col, TRUE, TRUE, 0);
    snprintf(t, sizeof t, "%d credit%s", s->credits, s->credits == 1 ? "" : "s");
    gtk_box_pack_end(GTK_BOX(top), ui_badge(t, s->credits == 0 ? "badge-warn" : "badge-neutral"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), top, FALSE, FALSE, 0);

    l = ui_label(s->name, "info-card-value");
    gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_box_pack_start(GTK_BOX(inner), l, FALSE, FALSE, 4);

    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(row), ui_icon_new(ICON_PROFILE, 16, "#64748B", NULL), FALSE, FALSE, 0);
    l = ui_label(s->faculty[0] ? s->faculty : "Faculty not set", "hint");
    gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(row), l, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);

    snprintf(t, sizeof t, "%s  |  %.0f marks  |  pass %.0f", s->type, s->max_marks, s->pass_marks);
    gtk_box_pack_start(GTK_BOX(inner), ui_label(t, "hint"), FALSE, FALSE, 0);
    return card;
}

void subjects_refresh(void)
{
    int i;
    char t[100];
    gtk_container_foreach(GTK_CONTAINER(flow), (GtkCallback)gtk_widget_destroy, NULL);
    for (i = 0; i < n_subjects; i++) gtk_flow_box_insert(GTK_FLOW_BOX(flow), subject_card(i), -1);
    gtk_widget_show_all(flow);
    if (sel_idx >= n_subjects) sel_idx = -1;
    if (sel_idx >= 0) gtk_flow_box_select_child(GTK_FLOW_BOX(flow), gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(flow), sel_idx));
    snprintf(t, sizeof t, "%d subjects  |  %d credits in total", n_subjects, total_credits());
    gtk_label_set_text(GTK_LABEL(l_summary), t);
}

/* index of the selected subject card or -1 */
static int selected(void)
{
    GList *l = gtk_flow_box_get_selected_children(GTK_FLOW_BOX(flow));
    int idx = -1;
    if (l) idx = gtk_flow_box_child_get_index(GTK_FLOW_BOX_CHILD(l->data));
    g_list_free(l);
    return idx;
}

static void on_card_selected(GtkFlowBox *fb, gpointer d) { (void)fb; (void)d; sel_idx = selected(); }

static void subject_dialog(int idx)
{
    static const char *lab[9] = { "Subject code", "Short name", "Subject name", "Faculty name", "Faculty email",
                                  "Subject type", "Maximum marks", "Passing marks", "Credits (0 allowed)" };
    GtkWidget *dlg = gtk_dialog_new_with_buttons(idx < 0 ? "Add subject" : "Edit subject", GTK_WINDOW(g_window),
                                                 GTK_DIALOG_MODAL, "Cancel", GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_OK, NULL);
    GtkWidget *grid = gtk_grid_new(), *en[9];
    char t[9][72];
    int i;
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 14);
    if (idx >= 0) {
        snprintf(t[0], 72, "%s", subjects[idx].code);      snprintf(t[1], 72, "%s", subjects[idx].short_name);
        snprintf(t[2], 72, "%s", subjects[idx].name);      snprintf(t[3], 72, "%s", subjects[idx].faculty);
        snprintf(t[4], 72, "%s", subjects[idx].email);     snprintf(t[5], 72, "%s", subjects[idx].type);
        snprintf(t[6], 72, "%g", subjects[idx].max_marks); snprintf(t[7], 72, "%g", subjects[idx].pass_marks);
        snprintf(t[8], 72, "%d", subjects[idx].credits);
    } else {
        for (i = 0; i < 9; i++) t[i][0] = '\0';
        strcpy(t[5], "Theory"); strcpy(t[6], "100"); strcpy(t[7], "40"); strcpy(t[8], "3");
    }
    for (i = 0; i < 9; i++) {
        en[i] = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(en[i]), t[i]);
        gtk_widget_set_hexpand(en[i], TRUE);
        gtk_entry_set_width_chars(GTK_ENTRY(en[i]), 34);
        gtk_grid_attach(GTK_GRID(grid), ui_label(lab[i], "hint"), 0, i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), en[i], 1, i, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), grid, TRUE, TRUE, 0);
    gtk_widget_show_all(dlg);
    while (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
        Subject s;
        char err[160] = "";
        int ok;
        memset(&s, 0, sizeof s);
        strncpy(s.code, gtk_entry_get_text(GTK_ENTRY(en[0])), sizeof s.code - 1);
        strncpy(s.short_name, gtk_entry_get_text(GTK_ENTRY(en[1])), sizeof s.short_name - 1);
        strncpy(s.name, gtk_entry_get_text(GTK_ENTRY(en[2])), sizeof s.name - 1);
        strncpy(s.faculty, gtk_entry_get_text(GTK_ENTRY(en[3])), sizeof s.faculty - 1);
        strncpy(s.email, gtk_entry_get_text(GTK_ENTRY(en[4])), sizeof s.email - 1);
        strncpy(s.type, gtk_entry_get_text(GTK_ENTRY(en[5])), sizeof s.type - 1);
        if (!parse_double(gtk_entry_get_text(GTK_ENTRY(en[6])), &s.max_marks)) { ui_error("Maximum marks must be a number."); continue; }
        if (!parse_double(gtk_entry_get_text(GTK_ENTRY(en[7])), &s.pass_marks)) { ui_error("Passing marks must be a number."); continue; }
        if (!parse_int(gtk_entry_get_text(GTK_ENTRY(en[8])), &s.credits)) { ui_error("Credits must be a whole number."); continue; }
        ok = idx < 0 ? subject_add(&s, err) : subject_update(idx, &s, err);
        if (!ok) { ui_error(err); continue; }
        ui_save();
        ui_refresh_all();
        break;
    }
    gtk_widget_destroy(dlg);
}

static void on_add(GtkWidget *w, gpointer d) { (void)w; (void)d; subject_dialog(-1); }

static void on_edit(GtkWidget *w, gpointer d)
{
    int i = selected();
    (void)w; (void)d;
    if (i < 0) { ui_info("Select a subject card first."); return; }
    subject_dialog(i);
}

static void on_delete(GtkWidget *w, gpointer d)
{
    int i = selected();
    char t[260];
    (void)w; (void)d;
    if (i < 0) { ui_info("Select a subject card first."); return; }
    snprintf(t, sizeof t, "Delete subject %s - %s?\nAll marks and attendance for this subject will also be deleted.",
             subjects[i].code, subjects[i].name);
    if (!ui_confirm(t)) return;
    subject_delete(i);
    sel_idx = -1;
    ui_save();
    ui_refresh_all();
}

static void info_row(GtkWidget *grid, int row, int col, const char *label, const char *value)
{
    GtkWidget *l = ui_label(value[0] ? value : "-", NULL);
    gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
    gtk_label_set_selectable(GTK_LABEL(l), TRUE);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_widget_set_hexpand(l, TRUE);
    gtk_grid_attach(GTK_GRID(grid), ui_label(label, "hint"), col * 2, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), l, col * 2 + 1, row, 1, 1);
}

void subject_profile_dialog(int idx)
{
    static const char *heads[3] = { "Examination", "Maximum marks", "Weightage" };
    Subject *s;
    GtkWidget *dlg, *sw, *page, *card, *inner, *grid, *row, *badge;
    GtkListStore *st;
    char t[200], title[120], w[16], m[16];
    double sum = 0;
    int i, shown = 0;

    if (idx < 0 || idx >= n_subjects) return;
    s = &subjects[idx];
    snprintf(title, sizeof title, "Subject - %s", s->name);
    dlg = gtk_dialog_new_with_buttons(title, GTK_WINDOW(g_window), GTK_DIALOG_MODAL, "Close", GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 780, 680);
    page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_container_set_border_width(GTK_CONTAINER(page), 16);

    /* header */
    card = ui_card(&inner);
    {
        GtkWidget *nl = ui_label(s->name, "page-title");
        gtk_label_set_line_wrap(GTK_LABEL(nl), TRUE);
        gtk_box_pack_start(GTK_BOX(inner), nl, FALSE, FALSE, 0);
    }
    snprintf(t, sizeof t, "%s   |   %s   |   %d credit%s", s->code, s->short_name, s->credits, s->credits == 1 ? "" : "s");
    gtk_box_pack_start(GTK_BOX(inner), ui_label(t, "page-sub"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), card, FALSE, FALSE, 0);

    /* details */
    card = ui_card(&inner);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Subject details", "card-head"), FALSE, FALSE, 0);
    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 14);
    info_row(grid, 0, 0, "Subject name", s->name);
    info_row(grid, 0, 1, "Subject code", s->code);
    info_row(grid, 1, 0, "Short name", s->short_name);
    info_row(grid, 1, 1, "Subject type", s->type);
    info_row(grid, 2, 0, "Faculty", s->faculty);
    info_row(grid, 2, 1, "Faculty email", s->email);
    snprintf(t, sizeof t, "%d", s->credits);                        info_row(grid, 3, 0, "Credits", t);
    snprintf(t, sizeof t, "%d", teachers[g_teacher].semester);      info_row(grid, 3, 1, "Semester", t);
    snprintf(t, sizeof t, "%.0f", s->max_marks);                    info_row(grid, 4, 0, "Maximum marks", t);
    snprintf(t, sizeof t, "%.0f (demonstration value)", s->pass_marks); info_row(grid, 4, 1, "Passing marks", t);
    if (s->credits == 0)
        info_row(grid, 5, 0, "Note", "Zero credits: counted in marks and attendance, excluded from SGPA.");
    gtk_box_pack_start(GTK_BOX(inner), grid, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), card, FALSE, FALSE, 0);

    /* examination pattern */
    card = ui_card(&inner);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Examination pattern", "card-head"), FALSE, FALSE, 0);
    st = ui_store_new(3);
    for (i = 0; i < n_exams; i++) sum += exams[i].max_marks;
    for (i = 0; i < n_exams; i++) {
        snprintf(m, sizeof m, "%.0f", exams[i].max_marks);
        snprintf(w, sizeof w, "%.0f%%", sum > 0 ? exams[i].max_marks * 100.0 / sum : 0.0);
        ui_store_add(st, 3, NULL, i, exams[i].name, m, w);
    }
    snprintf(m, sizeof m, "%.0f", sum);
    ui_store_add(st, 3, "#6D4AFF", n_exams, "Total", m, "100%");
    gtk_box_pack_start(GTK_BOX(inner), ui_tree_new(st, heads, 3, NULL, 36 + 28 * (n_exams + 1)), FALSE, FALSE, 0);
    g_object_unref(st);
    gtk_box_pack_start(GTK_BOX(page), card, FALSE, FALSE, 0);

    /* syllabus */
    card = ui_card(&inner);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Syllabus units", "card-head"), FALSE, FALSE, 0);
    for (i = 0; i < 5; i++) {
        GtkWidget *txt;
        if (s->units[i][0] == '\0') continue;
        row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_style_context_add_class(gtk_widget_get_style_context(row), "unit-row");
        snprintf(t, sizeof t, "%d", i + 1);
        badge = ui_label(t, "unit-num");
        gtk_widget_set_valign(badge, GTK_ALIGN_CENTER);
        gtk_label_set_xalign(GTK_LABEL(badge), 0.5f);
        gtk_box_pack_start(GTK_BOX(row), badge, FALSE, FALSE, 0);
        snprintf(t, sizeof t, "Unit %d", i + 1);
        txt = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_box_pack_start(GTK_BOX(txt), ui_label(t, "hint"), FALSE, FALSE, 0);
        {
            GtkWidget *l = ui_label(s->units[i], NULL);
            gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
            gtk_box_pack_start(GTK_BOX(txt), l, FALSE, FALSE, 0);
        }
        gtk_box_pack_start(GTK_BOX(row), txt, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 3);
        shown++;
    }
    if (shown == 0) gtk_box_pack_start(GTK_BOX(inner), ui_label("No syllabus units stored for this subject.", "hint"), FALSE, FALSE, 0);
    else gtk_box_pack_start(GTK_BOX(inner), ui_label("Syllabus units are informational; marks are not entered per unit.", "hint"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), card, FALSE, FALSE, 0);

    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(sw), page);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), sw, TRUE, TRUE, 0);
    gtk_widget_show_all(dlg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

static void on_view(GtkWidget *w, gpointer d)
{
    int i = selected();
    (void)w; (void)d;
    if (i < 0) { ui_info("Select a subject card first."); return; }
    subject_profile_dialog(i);
}

static void on_child_activated(GtkFlowBox *fb, GtkFlowBoxChild *ch, gpointer d)       /* double-click */
{
    (void)fb; (void)d;
    sel_idx = gtk_flow_box_child_get_index(ch);
    subject_profile_dialog(sel_idx);
}

GtkWidget *subjects_build(void)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14), *bar;
    gtk_box_pack_start(GTK_BOX(page), ui_title("Subjects", "Semester subjects, faculty, credits and syllabus"), FALSE, FALSE, 0);
    bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(bar), ui_icon_button(ICON_PLUS, "Add subject", "accent", G_CALLBACK(on_add), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), ui_icon_button(ICON_BOOK_OPEN, "View Subject", NULL, G_CALLBACK(on_view), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), ui_icon_button(ICON_EDIT, "Edit", NULL, G_CALLBACK(on_edit), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), ui_icon_button(ICON_DELETE, "Delete", "danger", G_CALLBACK(on_delete), NULL), FALSE, FALSE, 0);
    l_summary = ui_label("", "hint");
    gtk_box_pack_end(GTK_BOX(bar), l_summary, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(page), bar, FALSE, FALSE, 0);

    flow = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_SINGLE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), TRUE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 1);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 3);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 6);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 6);
    gtk_flow_box_set_activate_on_single_click(GTK_FLOW_BOX(flow), FALSE);        /* double-click opens the subject */
    g_signal_connect(flow, "selected-children-changed", G_CALLBACK(on_card_selected), NULL);
    g_signal_connect(flow, "child-activated", G_CALLBACK(on_child_activated), NULL);
    gtk_box_pack_start(GTK_BOX(page), flow, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), ui_label("Click a subject to select it; double-click or press View Subject for faculty, examination pattern and syllabus. "
                                              "Credits weight the SGPA; zero-credit subjects are excluded from it.", "hint"), FALSE, FALSE, 0);
    return page;
}
