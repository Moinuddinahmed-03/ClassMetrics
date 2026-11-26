/*
 * ui_students.c : Students page - table, search, sort, add / edit / delete,
 * and the individual student profile.
 */
#include "ui.h"

static GtkWidget *e_search, *c_sort, *l_count;
static GtkListStore *store;
static GtkTreeView *tree;

/* ---------- table ---------- */
void students_refresh(void)
{
    int view[MAX_STUDENTS], n, i, key, asc = 1;
    char sem[8], mk[16], at[16], rk[8], sg[16];
    const char *cells[10], *fgc[10], *bgc[10];
    double p, a, g;
    const char *q = gtk_entry_get_text(GTK_ENTRY(e_search));

    n = student_search(q, view);
    switch (gtk_combo_box_get_active(GTK_COMBO_BOX(c_sort))) {
        case 1:  key = 0; asc = 0; break;      /* roll Z-A  */
        case 2:  key = 1; break;               /* name A-Z  */
        case 3:  key = 1; asc = 0; break;      /* name Z-A  */
        case 4:  key = 2; asc = 0; break;      /* overall % high -> low */
        case 5:  key = 2; break;               /* marks low -> high */
        default: key = 0; break;               /* roll A-Z  */
    }
    student_sort(view, n, key, asc);

    gtk_list_store_clear(store);
    for (i = 0; i < n; i++) {
        Student *s = &students[view[i]];
        GdkPixbuf *av;
        int j, failed = 0;
        p = student_exam_pct(s->roll, EXAM_ALL, NULL, NULL);          /* overall % (CIE 1 + CIE 2 + SEE) */
        g = student_sgpa(s->roll, NULL, NULL);
        a = att_pct(s->roll, NULL);
        snprintf(sem, sizeof sem, "%d", s->semester);
        if (p >= 0) snprintf(mk, sizeof mk, "%.1f", p); else strcpy(mk, "-");
        if (a >= 0) snprintf(at, sizeof at, "%.1f", a); else strcpy(at, "-");
        if (p >= 0) snprintf(rk, sizeof rk, "%d", student_rank(s->roll, EXAM_ALL)); else strcpy(rk, "-");
        if (g >= 0) snprintf(sg, sizeof sg, "%.2f", g); else strcpy(sg, "-");
        for (j = 0; j < n_subjects; j++) if (subject_pct(s->roll, j) >= 0 && !subject_pass(s->roll, j)) failed++;
        cells[0] = s->roll; cells[1] = s->name; cells[2] = s->branch; cells[3] = s->section; cells[4] = sem;
        cells[5] = mk; cells[6] = sg; cells[7] = at; cells[8] = rk;
        for (j = 0; j < 10; j++) { fgc[j] = "#17213D"; bgc[j] = (i % 2) ? "#F5F6FB" : ""; }
        if (att_is_low(s->roll)) fgc[7] = "#DC2626";                       /* only the cell that needs attention is coloured */
        if (p < 0) { cells[9] = "-"; }
        else if (failed > 0) { cells[9] = "Needs attention"; fgc[9] = "#DC2626"; }
        else if (att_is_low(s->roll)) { cells[9] = "Low attendance"; fgc[9] = "#B45309"; }
        else { cells[9] = "Good standing"; fgc[9] = "#16A34A"; }
        av = ui_avatar_pixbuf(s->name, 28, "#EDE9FE", "#5B34D6");
        ui_avatar_store_add(store, 10, cells, fgc, bgc, av, view[i]);
        g_object_unref(av);
    }
    {
        char t[80];
        snprintf(t, sizeof t, "Showing %d of %d students", n, n_students);
        gtk_label_set_text(GTK_LABEL(l_count), t);
    }
}

static void on_changed(GtkWidget *w, gpointer d) { (void)w; (void)d; students_refresh(); }

/* ---------- add / edit dialog ---------- */
static void student_dialog(int idx)
{
    static const char *lab[5] = { "Student name", "Roll number", "Branch", "Section", "Semester" };
    GtkWidget *dlg = gtk_dialog_new_with_buttons(idx < 0 ? "Add student" : "Edit student", GTK_WINDOW(g_window),
                                                 GTK_DIALOG_MODAL, "Cancel", GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_OK, NULL);
    GtkWidget *grid = gtk_grid_new(), *en[5], *c_blood;
    char sem[8];
    int i, k;
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 14);
    for (i = 0; i < 5; i++) {
        en[i] = gtk_entry_new();
        gtk_widget_set_hexpand(en[i], TRUE);
        gtk_grid_attach(GTK_GRID(grid), ui_label(lab[i], "hint"), 0, i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), en[i], 1, i, 1, 1);
    }
    if (idx >= 0) {
        snprintf(sem, sizeof sem, "%d", students[idx].semester);
        gtk_entry_set_text(GTK_ENTRY(en[0]), students[idx].name);
        gtk_entry_set_text(GTK_ENTRY(en[1]), students[idx].roll);
        gtk_entry_set_text(GTK_ENTRY(en[2]), students[idx].branch);
        gtk_entry_set_text(GTK_ENTRY(en[3]), students[idx].section);
        gtk_entry_set_text(GTK_ENTRY(en[4]), sem);
    } else {
        gtk_entry_set_text(GTK_ENTRY(en[2]), teachers[g_teacher].cls[0] ? "CSM" : "");
        gtk_entry_set_text(GTK_ENTRY(en[3]), "B");
        snprintf(sem, sizeof sem, "%d", teachers[g_teacher].semester);
        gtk_entry_set_text(GTK_ENTRY(en[4]), sem);
    }
    gtk_entry_set_placeholder_text(GTK_ENTRY(en[1]), "e.g. 990000000056 (kept as text)");
    /* optional blood group (never used in any calculation) */
    c_blood = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c_blood), "Not specified");
    for (k = 0; k < 8; k++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c_blood), BLOOD_GROUPS[k]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(c_blood), 0);
    if (idx >= 0)
        for (k = 0; k < 8; k++) if (strcmp(students[idx].blood, BLOOD_GROUPS[k]) == 0) gtk_combo_box_set_active(GTK_COMBO_BOX(c_blood), k + 1);
    gtk_grid_attach(GTK_GRID(grid), ui_label("Blood group (optional)", "hint"), 0, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), c_blood, 1, 5, 1, 1);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), grid, TRUE, TRUE, 0);
    gtk_widget_show_all(dlg);

    while (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
        Student s;
        char err[160] = "";
        int ok, semv;
        memset(&s, 0, sizeof s);
        strncpy(s.name, gtk_entry_get_text(GTK_ENTRY(en[0])), sizeof s.name - 1);
        strncpy(s.roll, gtk_entry_get_text(GTK_ENTRY(en[1])), sizeof s.roll - 1);
        strncpy(s.branch, gtk_entry_get_text(GTK_ENTRY(en[2])), sizeof s.branch - 1);
        strncpy(s.section, gtk_entry_get_text(GTK_ENTRY(en[3])), sizeof s.section - 1);
        if (!parse_int(gtk_entry_get_text(GTK_ENTRY(en[4])), &semv)) { ui_error("Semester must be a whole number."); continue; }
        s.semester = semv;
        k = gtk_combo_box_get_active(GTK_COMBO_BOX(c_blood));
        if (k >= 1 && k <= 8) strcpy(s.blood, BLOOD_GROUPS[k - 1]);
        ok = idx < 0 ? student_add(&s, err) : student_update(idx, &s, err);
        if (!ok) { ui_error(err); continue; }       /* keep the dialog open to fix the input */
        ui_save();
        ui_refresh_all();
        break;
    }
    gtk_widget_destroy(dlg);
}

static void on_add(GtkWidget *w, gpointer d) { (void)w; (void)d; student_dialog(-1); }

static void on_edit(GtkWidget *w, gpointer d)
{
    int i = ui_tree_selected(tree);
    (void)w; (void)d;
    if (i < 0) { ui_info("Select a student in the table first."); return; }
    student_dialog(i);
}

static void on_delete(GtkWidget *w, gpointer d)
{
    int i = ui_tree_selected(tree);
    char t[200];
    (void)w; (void)d;
    if (i < 0) { ui_info("Select a student in the table first."); return; }
    snprintf(t, sizeof t, "Delete %s (%s)?\nTheir marks and attendance records will also be deleted.", students[i].name, students[i].roll);
    if (!ui_confirm(t)) return;
    student_delete(i);
    ui_save();
    ui_refresh_all();
}

static void on_import(GtkWidget *w, gpointer d)
{
    char *path, msg[400], prob[160];
    int added, skipped;
    (void)w; (void)d;
    path = ui_choose_file(0, "Import students from CSV", NULL);   /* native Windows Open dialog */
    if (path != NULL) {
        if (student_import_csv(path, &added, &skipped, prob) == 0) {
            snprintf(msg, sizeof msg, "Imported %d student(s), skipped %d.%s%s", added, skipped, skipped ? "\nFirst problem: " : "", skipped ? prob : "");
            if (added > 0) { ui_save(); ui_refresh_all(); }
            ui_info(msg);
        } else ui_error("Could not read that file.");
        g_free(path);
    }
}

static void on_profile(GtkWidget *w, gpointer d)
{
    int i = ui_tree_selected(tree);
    (void)w; (void)d;
    if (i < 0) { ui_info("Select a student in the table first."); return; }
    student_profile_dialog(i);
}

static void on_row_activated(GtkTreeView *tv, GtkTreePath *p, GtkTreeViewColumn *c, gpointer d)
{
    (void)p; (void)c; (void)d;
    on_profile(NULL, tv);
}

/* ---------- page ---------- */
GtkWidget *students_build(void)
{
    static const char *heads[10] = { "Roll No", "Name", "Branch", "Section", "Sem", "Overall %", "SGPA", "Attendance %", "Rank", "Status" };
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12), *bar, *card, *inner, *sw;
    const char *sorts[6] = { "Roll number (A-Z)", "Roll number (Z-A)", "Name (A-Z)", "Name (Z-A)",
                             "Overall % (high to low)", "Overall % (low to high)" };
    int i;

    gtk_box_pack_start(GTK_BOX(page), ui_title("Students", "Add, search, sort and review student records"), FALSE, FALSE, 0);
    card = ui_card(&inner);
    bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    e_search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_search), "Search by name or roll number");
    gtk_widget_set_size_request(e_search, 210, -1);
    g_signal_connect(e_search, "search-changed", G_CALLBACK(on_changed), NULL);
    gtk_box_pack_start(GTK_BOX(bar), e_search, FALSE, FALSE, 0);
    c_sort = gtk_combo_box_text_new();
    for (i = 0; i < 6; i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c_sort), sorts[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(c_sort), 0);
    g_signal_connect(c_sort, "changed", G_CALLBACK(on_changed), NULL);
    gtk_box_pack_start(GTK_BOX(bar), ui_labeled("Sort:", c_sort), FALSE, FALSE, 8);
    gtk_box_pack_end(GTK_BOX(bar), ui_icon_button(ICON_DELETE, "Delete", "danger", G_CALLBACK(on_delete), NULL), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(bar), ui_icon_button(ICON_EDIT, "Edit", NULL, G_CALLBACK(on_edit), NULL), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(bar), ui_icon_button(ICON_USER_CHECK, "Profile", NULL, G_CALLBACK(on_profile), NULL), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(bar), ui_icon_button(ICON_IMPORT, "Import", NULL, G_CALLBACK(on_import), NULL), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(bar), ui_icon_button(ICON_PLUS, "Add student", "accent", G_CALLBACK(on_add), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), bar, FALSE, FALSE, 0);

    store = ui_avatar_store_new(10);
    {
        static const float xal[10] = { 0, 0, 0, 0, 0.5f, 1, 1, 1, 0.5f, 0 };
        GtkWidget *tv = ui_avatar_tree_new(store, heads, 10, xal, 56);
        tree = GTK_TREE_VIEW(tv);
        sw = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(sw), 380);
        gtk_container_add(GTK_CONTAINER(sw), tv);
    }
    g_signal_connect(tree, "row-activated", G_CALLBACK(on_row_activated), NULL);
    gtk_box_pack_start(GTK_BOX(inner), sw, TRUE, TRUE, 0);
    l_count = ui_label("", "hint");
    gtk_box_pack_start(GTK_BOX(inner), l_count, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Double-click a row to open the student profile. Red figures need attention: a failed subject or attendance below the threshold.", "hint"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), card, TRUE, TRUE, 0);
    return page;
}
