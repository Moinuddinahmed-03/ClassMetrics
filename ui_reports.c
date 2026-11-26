/*
 * ui_reports.c : Reports page - nine reports shown in a table, with CSV export.
 */
#include "ui.h"

static GtkWidget *c_type, *c_stu, *c_sub, *c_exam, *l_title, *l_info, *box_table;
static GtkWidget *tree_holder = NULL;
static Report rep;                 /* the report currently displayed */
static int busy = 0;

#define NTYPES 9
static const char *TYPES[NTYPES] = { "Individual student performance", "Class performance", "Subject-wise marks",
    "Examination-wise marks", "Attendance report", "Low-attendance students", "Grade distribution",
    "SGPA ranking", "Mathematical analysis" };

/* Builds the report chosen in the combo boxes. */
static void generate(void)
{
    int t = gtk_combo_box_get_active(GTK_COMBO_BOX(c_type));
    int s = gtk_combo_box_get_active(GTK_COMBO_BOX(c_stu));
    int j = gtk_combo_box_get_active(GTK_COMBO_BOX(c_sub));
    int e = ui_exam_sel(c_exam, 1);                    /* EXAM_ALL (-1) = all examinations / totals */
    switch (t) {
        case 0: report_student(&rep, (s >= 0 && s < n_students) ? students[s].roll : ""); break;
        case 1: report_class(&rep, e); break;
        case 2: report_subject(&rep, j, e); break;
        case 3: report_exam(&rep, e); break;
        case 4: report_attendance(&rep); break;
        case 5: report_low_attendance(&rep); break;
        case 6: report_grades(&rep); break;
        case 7: report_sgpa(&rep); break;
        default: report_math(&rep, e); break;
    }
}

/* Rebuilds the table widget for the current report (columns differ per report). */
static void show_report(void)
{
    GtkListStore *st;
    GtkWidget *tree;
    const char *heads[REP_COLS];
    int i, j;
    char info[900], leg[300];

    generate();
    if (tree_holder != NULL) gtk_widget_destroy(tree_holder);
    st = ui_store_new(rep.ncols);
    for (j = 0; j < rep.ncols; j++) heads[j] = rep.head[j];
    for (i = 0; i < rep.nrows; i++) {
        GtkTreeIter it;
        gtk_list_store_append(st, &it);
        for (j = 0; j < rep.ncols; j++) gtk_list_store_set(st, &it, j, rep.cell[i][j], -1);
        gtk_list_store_set(st, &it, rep.ncols, rep.flag[i] ? "#D93636" : "#1F2937", rep.ncols + 1, i, -1);
    }
    tree = ui_tree_new(st, heads, rep.ncols, NULL, 380);
    g_object_unref(st);
    gtk_box_pack_start(GTK_BOX(box_table), tree, TRUE, TRUE, 0);
    gtk_widget_show_all(tree);
    tree_holder = tree;
    gtk_label_set_text(GTK_LABEL(l_title), rep.title);
    grade_legend(leg, sizeof leg);
    snprintf(info, sizeof info, "%d row(s). Red rows are warnings (failed subject, low attendance).\nGrades: %s%s%s",
             rep.nrows, leg, rep.prepared[0] ? "\n" : "", rep.prepared);
    gtk_label_set_text(GTK_LABEL(l_info), info);
}

/* Enables only the selectors that the chosen report needs. */
static void update_selectors(void)
{
    int t = gtk_combo_box_get_active(GTK_COMBO_BOX(c_type));
    gtk_widget_set_sensitive(c_stu, t == 0);
    gtk_widget_set_sensitive(c_sub, t == 2);
    gtk_widget_set_sensitive(c_exam, t == 1 || t == 2 || t == 3 || t == 8);
}

static void on_change(GtkWidget *w, gpointer d)
{
    (void)w; (void)d;
    if (busy) return;
    update_selectors();
    show_report();
}

void reports_refresh(void)
{
    busy = 1;
    ui_combo_students(GTK_COMBO_BOX_TEXT(c_stu));
    ui_combo_subjects(GTK_COMBO_BOX_TEXT(c_sub), 0);
    ui_combo_exams_ex(GTK_COMBO_BOX_TEXT(c_exam), 1);
    busy = 0;
    update_selectors();
    show_report();
}

static void on_export(GtkWidget *w, gpointer d)
{
    char name[80], today[12], *path;
    (void)w; (void)d;
    today_str(today);
    snprintf(name, sizeof name, "report_%d_%s.csv", gtk_combo_box_get_active(GTK_COMBO_BOX(c_type)) + 1, today);
    path = ui_choose_file(1, "Export report as CSV", name);     /* native Windows Save dialog */
    if (path == NULL) return;                                    /* cancelled */
    if (report_export_csv(&rep, path) == 0) {
        char t[600];
        snprintf(t, sizeof t, "Report exported to:\n%s", path);
        ui_info(t);
    } else ui_error("Could not write the file. Choose another folder or file name.");
    g_free(path);
}

GtkWidget *reports_build(void)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12), *card, *inner, *row;
    int i;
    gtk_box_pack_start(GTK_BOX(page), ui_title("Reports", "Generate reports and export them as CSV files"), FALSE, FALSE, 0);
    card = ui_card(&inner);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    c_type = gtk_combo_box_text_new();
    for (i = 0; i < NTYPES; i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c_type), TYPES[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(c_type), 1);
    c_stu = gtk_combo_box_text_new();
    c_sub = gtk_combo_box_text_new();
    c_exam = gtk_combo_box_text_new();
    g_signal_connect(c_type, "changed", G_CALLBACK(on_change), NULL);
    g_signal_connect(c_stu, "changed", G_CALLBACK(on_change), NULL);
    g_signal_connect(c_sub, "changed", G_CALLBACK(on_change), NULL);
    g_signal_connect(c_exam, "changed", G_CALLBACK(on_change), NULL);
    gtk_box_pack_start(GTK_BOX(row), ui_labeled("Report:", c_type), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), ui_labeled("Student:", c_stu), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(row), ui_labeled("Subject:", c_sub), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), ui_labeled("Exam:", c_exam), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);
    l_title = ui_label("", "card-head");
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(row), l_title, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(row), ui_icon_button(ICON_EXPORT, "Export CSV", "accent", G_CALLBACK(on_export), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);
    box_table = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(inner), box_table, TRUE, TRUE, 0);
    l_info = ui_label("", "hint");
    gtk_box_pack_start(GTK_BOX(inner), l_info, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), card, TRUE, TRUE, 0);
    return page;
}
