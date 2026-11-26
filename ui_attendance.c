/*
 * ui_attendance.c : ClassMetrics 2.0 Attendance page.
 *   Tab 1  Take / edit attendance for one class session (live counters, lock + save animations)
 *   Tab 2  History: date-range and subject filters, summary cards, View / Edit / Delete, CSV export
 *   Tab 3  Summary & warnings: summary cards, progress bars per student, threshold
 *
 * A session is identified by DATE + PERIOD for the class.  Its subject is stored with
 * the session: once attendance is saved, that period of that date belongs to that subject
 * (the rule is enforced in attendance.c, the screen only shows it).  Changing the date
 * or period loads the saved session automatically and locks the subject.  A wrongly
 * assigned subject is corrected by deleting the session in History.
 * Saving an existing session EDITS it - duplicates are never created.
 */
#include <math.h>
#include <string.h>
#include "ui.h"

static GtkWidget *nb, *e_date, *c_sub, *sp_period, *l_session, *lock_icon, *save_ok, *l_conflict, *sp_thr, *c_sumsub;
static GtkWidget *cnt_val[4];                          /* total / present / absent / not marked */
static GtkWidget *e_from, *e_to, *c_hsub, *hist_val[3];
static GtkWidget *sum_val[3];
static GtkListStore *st_take, *st_hist, *st_sum;
static GtkTreeView *tv_take, *tv_hist;
static int pres[MAX_STUDENTS];      /* per student: 1 present, 0 absent, -1 not marked */
static int dirty = 0, loaded_n = -1, busy = 0;
static char cur_date[12] = "";      /* the session that is currently shown */
static int cur_period = 0, locked = 0;
static guint ok_timer = 0;

/* distinct sessions found in the attendance records */
typedef struct { char code[16]; char date[12]; int session; int present, total; } Session;
static Session sess[2000];
static int n_sess = 0;

#define MSG_EXISTING "Existing attendance loaded. Subject locked for this period."
#define MSG_NEW      "New session. Select a subject and record attendance."

static int sel_subject(void) { return gtk_combo_box_get_active(GTK_COMBO_BOX(c_sub)); }
/* The period is a plain integer 1..8 changed by two ordinary buttons: a GtkButton emits "clicked" once per click
 * (a GtkSpinButton auto-repeats while the arrow is held and could run up to 8 from one slow click). */
#define PERIOD_MIN 1
#define PERIOD_MAX 8
static int period_val = 1;
static GtkWidget *l_period, *b_per_dn, *b_per_up;
static int sel_period(void)  { return period_val; }
static void set_period(int v)
{
    char t[8];
    if (v < PERIOD_MIN) v = PERIOD_MIN;
    if (v > PERIOD_MAX) v = PERIOD_MAX;
    period_val = v;
    snprintf(t, sizeof t, "%d", v);
    gtk_label_set_text(GTK_LABEL(l_period), t);
    gtk_widget_set_sensitive(b_per_dn, v > PERIOD_MIN);
    gtk_widget_set_sensitive(b_per_up, v < PERIOD_MAX);
}

/* ---------- small helpers ---------- */
/* Counter box: big number + caption.  kind: 0 neutral, 1 green, 2 red. */
static GtkWidget *counter(const char *caption, int kind, GtkWidget **value)
{
    GtkWidget *b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkStyleContext *sc = gtk_widget_get_style_context(b);
    gtk_style_context_add_class(sc, "counter");
    if (kind == 1) gtk_style_context_add_class(sc, "counter-good");
    if (kind == 2) gtk_style_context_add_class(sc, "counter-bad");
    *value = ui_label("0", "counter-value");
    if (kind == 1) gtk_style_context_add_class(gtk_widget_get_style_context(*value), "good");
    if (kind == 2) gtk_style_context_add_class(gtk_widget_get_style_context(*value), "warn");
    gtk_box_pack_start(GTK_BOX(b), *value, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(b), ui_label(caption, "field-label"), FALSE, FALSE, 0);
    gtk_widget_set_hexpand(b, TRUE);
    return b;
}

/* Card with a soft icon: big value, caption (used by History and Summary). */
static GtkWidget *mini_card(int icon, const char *fg, const char *bg, const char *caption, GtkWidget **value)
{
    GtkWidget *inner, *card = ui_card(&inner), *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12), *col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "stat-card");
    gtk_box_pack_start(GTK_BOX(row), ui_icon_new(icon, 40, fg, bg), FALSE, FALSE, 0);
    *value = ui_label("-", "card-value");
    gtk_box_pack_start(GTK_BOX(col), *value, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(col), ui_label(caption, "card-label"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), col, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);
    gtk_widget_set_hexpand(card, TRUE);
    return card;
}

static void set_num(GtkWidget *l, double v, int dec, const char *suf)
{
    char t[40];
    snprintf(t, sizeof t, "%.*f%s", dec, v, suf);
    gtk_label_set_text(GTK_LABEL(l), t);
}

/* ---------- take attendance tab ---------- */
static void show_take_list(void)
{
    int i, np = 0, na = 0, nn = 0;
    char ov[16];
    double p;
    gtk_list_store_clear(st_take);
    for (i = 0; i < n_students; i++) {
        p = att_pct(students[i].roll, NULL);
        if (p >= 0) snprintf(ov, sizeof ov, "%.1f", p); else strcpy(ov, "-");
        if (pres[i] == 1) np++; else if (pres[i] == 0) na++; else nn++;
        ui_store_add(st_take, 4, pres[i] == 1 ? "#16A34A" : pres[i] == 0 ? "#DC2626" : "#64748B", i,
                     students[i].roll, students[i].name,
                     pres[i] == 1 ? "Present" : pres[i] == 0 ? "Absent" : "Not marked", ov);
    }
    set_num(cnt_val[0], n_students, 0, "");                 /* live counters */
    set_num(cnt_val[1], np, 0, "");
    set_num(cnt_val[2], na, 0, "");
    set_num(cnt_val[3], nn, 0, "");
}

static void set_locked(int on)
{
    locked = on;
    gtk_widget_set_sensitive(c_sub, !on);          /* the subject cannot be changed for a saved period */
    gtk_widget_set_visible(lock_icon, on);
}

/*
 * Shows the session of the date and period chosen on the screen:
 *  - saved session: its subject is selected and locked, saved statuses are loaded
 *  - no session   : the subject can be chosen, every student is "Not marked"
 */
static void sync_session(void)
{
    const char *date = gtk_entry_get_text(GTK_ENTRY(e_date));
    int i, j, per = sel_period(), k;
    char code[16];

    for (i = 0; i < n_students; i++) pres[i] = -1;
    dirty = 0;
    loaded_n = n_students;
    if (!valid_date(date)) {
        set_locked(0);
        gtk_label_set_text(GTK_LABEL(l_session), "Enter the date as YYYY-MM-DD.");
        show_take_list();
        return;
    }
    snprintf(cur_date, sizeof cur_date, "%s", date);
    cur_period = per;
    if (att_period_subject(date, per, code) && (j = subject_find(code)) >= 0) {
        busy = 1;
        gtk_combo_box_set_active(GTK_COMBO_BOX(c_sub), j);      /* the saved subject is selected automatically */
        busy = 0;
        for (i = 0; i < n_students; i++)
            if ((k = att_find(students[i].roll, code, date, per)) >= 0) pres[i] = atts[k].present;
        set_locked(1);
        gtk_label_set_text(GTK_LABEL(l_session), MSG_EXISTING);
    } else {
        set_locked(0);
        gtk_label_set_text(GTK_LABEL(l_session), n_subjects > 0 ? MSG_NEW : "Add a subject first.");
    }
    show_take_list();
}

/* Saves the session that is currently shown.  Returns 1 on success (errors are reported). */
static int save_current(void)
{
    int j = sel_subject();
    char err[200] = "";
    if (j < 0) { ui_error("Select a subject."); return 0; }
    if (!att_save_session(subjects[j].code, cur_date, cur_period, pres, err)) { ui_error(err); return 0; }
    dirty = 0;
    ui_save();
    return 1;
}

/* Asks what to do with unsaved changes: 1 save, 2 discard, 0 cancel. */
static int ask_unsaved(void)
{
    char t[200];
    GtkWidget *d;
    int r;
    snprintf(t, sizeof t, "The attendance of %s, period %d has unsaved changes.\nWhat do you want to do?", cur_date, cur_period);
    d = gtk_message_dialog_new(GTK_WINDOW(g_window), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s", t);
    gtk_dialog_add_button(GTK_DIALOG(d), "Save and continue", 1);
    gtk_dialog_add_button(GTK_DIALOG(d), "Discard changes", 2);
    gtk_dialog_add_button(GTK_DIALOG(d), "Cancel", GTK_RESPONSE_CANCEL);
    r = gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    return (r == 1 || r == 2) ? r : 0;
}

/* Puts the date and period widgets back to the session that is still shown. */
static void restore_widgets(void)
{
    busy = 1;
    gtk_entry_set_text(GTK_ENTRY(e_date), cur_date);
    set_period(cur_period);
    busy = 0;
}

/* Date or period changed (or Load session pressed): load that session, never lose unsaved marks silently. */
static void request_sync(int force)
{
    const char *d = gtk_entry_get_text(GTK_ENTRY(e_date));
    int p = sel_period();
    if (!force && cur_date[0] != '\0' && strcmp(d, cur_date) == 0 && p == cur_period) return;
    if (!valid_date(d)) {
        if (force) ui_error("Enter a valid date as YYYY-MM-DD.");
        return;                                    /* still typing the date */
    }
    if (dirty && cur_date[0] != '\0') {
        int r = ask_unsaved();
        if (r == 0) { restore_widgets(); return; }
        if (r == 1) {
            if (!save_current()) { restore_widgets(); return; }
            ui_refresh_all();                      /* other pages show the saved attendance */
        }
    }
    sync_session();
}

static void on_load(GtkWidget *w, gpointer d) { (void)w; (void)d; if (!busy) request_sync(1); }
static void on_date_changed(GtkWidget *w, gpointer d) { (void)w; (void)d; if (!busy) request_sync(0); }
/* "-" / "+": exactly one period per click, limited to 1..8 */
static void on_period_step(GtkWidget *w, gpointer step)
{
    int v = period_val + GPOINTER_TO_INT(step);
    (void)w;
    if (v < PERIOD_MIN || v > PERIOD_MAX || busy) return;
    set_period(v);
    request_sync(0);                               /* loads the saved session + subject of that period */
}

static void on_today(GtkWidget *w, gpointer d)
{
    char t[12];
    (void)w; (void)d;
    today_str(t);
    gtk_entry_set_text(GTK_ENTRY(e_date), t);      /* the "changed" signal loads the session */
}

static void on_pick(GtkWidget *w, gpointer d)
{
    (void)w; (void)d;
    ui_pick_date(GTK_WINDOW(g_window), GTK_ENTRY(e_date));
}

static void set_selected(int v)
{
    int i = ui_tree_selected(tv_take);
    if (i < 0) { ui_info("Select a student in the list first."); return; }
    pres[i] = v;
    dirty = 1;
    show_take_list();
}
static void on_present(GtkWidget *w, gpointer d) { (void)w; (void)d; set_selected(1); }
static void on_absent(GtkWidget *w, gpointer d)  { (void)w; (void)d; set_selected(0); }

static void on_row_activated(GtkTreeView *tv, GtkTreePath *p, GtkTreeViewColumn *c, gpointer d)
{
    int i = ui_tree_selected(tv);
    (void)p; (void)c; (void)d;
    if (i >= 0) { pres[i] = (pres[i] == 1) ? 0 : 1; dirty = 1; show_take_list(); }   /* double-click toggles */
}

static void mark_all(int v)
{
    int i;
    for (i = 0; i < n_students; i++) pres[i] = v;
    dirty = 1;
    show_take_list();
}
static void on_all_present(GtkWidget *w, gpointer d) { (void)w; (void)d; mark_all(1); }
static void on_all_absent(GtkWidget *w, gpointer d)  { (void)w; (void)d; mark_all(0); }

static gboolean hide_save_ok(gpointer d)
{
    (void)d;
    gtk_widget_hide(save_ok);
    ok_timer = 0;
    return G_SOURCE_REMOVE;
}

static void on_save(GtkWidget *w, gpointer d)
{
    int i, unmarked = 0;
    (void)w; (void)d;
    if (cur_date[0] == '\0' || strcmp(gtk_entry_get_text(GTK_ENTRY(e_date)), cur_date) != 0 || sel_period() != cur_period) {
        ui_error("The date or period was changed. Press Load session first.");
        return;
    }
    for (i = 0; i < n_students; i++) if (pres[i] < 0) unmarked++;
    if (unmarked > 0) {
        char t[160];
        snprintf(t, sizeof t, "%d student(s) are still 'Not marked'.\nMark them Present or Absent (or use Mark all present).", unmarked);
        ui_error(t);
        return;
    }
    if (!save_current()) return;
    ui_refresh_all();                              /* reloads the session: the subject is now locked */
    gtk_label_set_text(GTK_LABEL(l_session), "Attendance saved. Subject locked for this period.");
    gtk_widget_show(save_ok);                      /* animated success check next to the button */
    ui_icon_animate(save_ok);
    ui_icon_animate(lock_icon);                    /* the lock closes */
    ui_toast("Attendance saved");
    if (ok_timer) g_source_remove(ok_timer);
    ok_timer = g_timeout_add(1800, hide_save_ok, NULL);
}

/* ---------- history tab ---------- */
static void build_sessions(void)
{
    int i, k, j;
    n_sess = 0;
    for (i = 0; i < n_atts; i++) {
        for (k = 0; k < n_sess; k++)
            if (sess[k].session == atts[i].session && strcmp(sess[k].code, atts[i].subject) == 0 &&
                strcmp(sess[k].date, atts[i].date) == 0) break;
        if (k == n_sess) {
            if (n_sess >= 2000) continue;
            strcpy(sess[k].code, atts[i].subject);
            strcpy(sess[k].date, atts[i].date);
            sess[k].session = atts[i].session;
            sess[k].present = sess[k].total = 0;
            n_sess++;
        }
        sess[k].total++;
        if (atts[i].present) sess[k].present++;
    }
    /* insertion sort: newest date first */
    for (i = 1; i < n_sess; i++) {
        Session cur = sess[i];
        j = i - 1;
        while (j >= 0 && (strcmp(sess[j].date, cur.date) < 0 ||
               (strcmp(sess[j].date, cur.date) == 0 && sess[j].session < cur.session))) { sess[j + 1] = sess[j]; j--; }
        sess[j + 1] = cur;
    }
}

/* Does session s pass the date range and subject filters of the History tab? */
static int session_matches(const Session *s)
{
    const char *from = gtk_entry_get_text(GTK_ENTRY(e_from)), *to = gtk_entry_get_text(GTK_ENTRY(e_to));
    int j = gtk_combo_box_get_active(GTK_COMBO_BOX(c_hsub)) - 1;
    if (valid_date(from) && strcmp(s->date, from) < 0) return 0;
    if (valid_date(to) && strcmp(s->date, to) > 0) return 0;
    if (j >= 0 && strcmp(s->code, subjects[j].code) != 0) return 0;
    return 1;
}

/* History table: one progress-bar column for the present percentage.
 * Store: 7 strings, colour string, present % (int), key (session index). */
static void show_history(void)
{
    int i, j, shown = 0, tp = 0, tt = 0, distinct = 0, seen[MAX_SUBJECTS] = { 0 };
    char per[8], pr[8], ab[8], tot[8], pc[16];
    GtkTreeIter it;
    build_sessions();
    gtk_list_store_clear(st_hist);
    for (i = 0; i < n_sess; i++) {
        double pcv;
        if (!session_matches(&sess[i])) continue;
        shown++; tp += sess[i].present; tt += sess[i].total;
        j = subject_find(sess[i].code);
        if (j >= 0 && !seen[j]) { seen[j] = 1; distinct++; }
        pcv = sess[i].present * 100.0 / sess[i].total;
        snprintf(per, sizeof per, "%d", sess[i].session);
        snprintf(pr, sizeof pr, "%d", sess[i].present);
        snprintf(ab, sizeof ab, "%d", sess[i].total - sess[i].present);
        snprintf(tot, sizeof tot, "%d", sess[i].total);
        snprintf(pc, sizeof pc, "%.1f", pcv);
        gtk_list_store_append(st_hist, &it);
        gtk_list_store_set(st_hist, &it, 0, sess[i].date, 1, per, 2, j >= 0 && subjects[j].short_name[0] ? subjects[j].short_name : sess[i].code,
                           3, pr, 4, ab, 5, tot, 6, pc, 7, pcv < settings.att_threshold ? "#B91C1C" : "#17213D", 8, (int)(pcv + 0.5), 9, i, -1);
    }
    set_num(hist_val[0], shown, 0, "");                                  /* summary cards follow the filters */
    if (tt > 0) set_num(hist_val[1], tp * 100.0 / tt, 1, "%"); else gtk_label_set_text(GTK_LABEL(hist_val[1]), "-");
    set_num(hist_val[2], distinct, 0, "");
}

static int hist_selected(void)
{
    GtkTreeModel *m;
    GtkTreeIter it;
    int k = -1;
    if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection(tv_hist), &m, &it)) return -1;
    gtk_tree_model_get(m, &it, 9, &k, -1);
    return k;
}

static void on_hist_filter(GtkWidget *w, gpointer d) { (void)w; (void)d; if (!busy) show_history(); }

static void on_edit_session(GtkWidget *w, gpointer d)
{
    int i = hist_selected();
    (void)w; (void)d;
    if (i < 0) { ui_info("Select a session in the history table first."); return; }
    busy = 1;
    gtk_entry_set_text(GTK_ENTRY(e_date), sess[i].date);
    set_period(sess[i].session);
    busy = 0;
    request_sync(1);                               /* selects and locks the saved subject */
    if (strcmp(gtk_entry_get_text(GTK_ENTRY(e_date)), sess[i].date) == 0 && sel_period() == sess[i].session)
        gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), 0);
}

static void on_delete_session(GtkWidget *w, gpointer d)
{
    int i = hist_selected(), j;
    char t[420];
    (void)w; (void)d;
    if (i < 0) { ui_info("Select a session in the history table first."); return; }
    j = subject_find(sess[i].code);
    snprintf(t, sizeof t, "WARNING: delete the whole attendance session of %s, period %d (%s)?\n\n"
                          "All its Present/Absent records are removed from the attendance calculations and the period is unlocked, "
                          "so another subject can be recorded. This cannot be undone.",
             sess[i].date, sess[i].session, j >= 0 ? subjects[j].name : sess[i].code);
    if (!ui_confirm(t)) return;
    att_delete_session(sess[i].code, sess[i].date, sess[i].session);
    ui_save();
    ui_refresh_all();
}

/* Read-only list of who was present / absent in one class. */
static void on_view_session(GtkWidget *w, gpointer d)
{
    static const char *heads[3] = { "Roll No", "Name", "Status" };
    int i = hist_selected(), k, j, np = 0, na = 0;
    GtkWidget *dlg, *box, *row, *inner, *card, *c1, *c2, *v1, *v2;
    GtkListStore *st;
    char t[200];
    (void)w; (void)d;
    if (i < 0) { ui_info("Select a session in the history table first."); return; }
    j = subject_find(sess[i].code);
    snprintf(t, sizeof t, "Class on %s, period %d", sess[i].date, sess[i].session);
    dlg = gtk_dialog_new_with_buttons(t, GTK_WINDOW(g_window), GTK_DIALOG_MODAL, "Close", GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 620, 640);
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(box), 16);
    card = ui_card(&inner);
    gtk_box_pack_start(GTK_BOX(inner), ui_label(j >= 0 ? subjects[j].name : sess[i].code, "page-title"), FALSE, FALSE, 0);
    snprintf(t, sizeof t, "%s  |  Period %d  |  %s  |  read-only", sess[i].date, sess[i].session, j >= 0 ? subjects[j].faculty : "");
    gtk_box_pack_start(GTK_BOX(inner), ui_label(t, "page-sub"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), card, FALSE, FALSE, 0);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_set_homogeneous(GTK_BOX(row), TRUE);
    st = ui_store_new(3);
    for (k = 0; k < n_atts; k++) {
        int si;
        if (atts[k].session != sess[i].session || strcmp(atts[k].subject, sess[i].code) != 0 || strcmp(atts[k].date, sess[i].date) != 0) continue;
        si = student_find(atts[k].roll);
        if (atts[k].present) np++; else na++;
        ui_store_add(st, 3, atts[k].present ? "#16A34A" : "#DC2626", k, atts[k].roll, si >= 0 ? students[si].name : "(removed student)",
                     atts[k].present ? "Present" : "Absent");
    }
    c1 = counter("Present", 1, &v1); c2 = counter("Absent", 2, &v2);
    set_num(v1, np, 0, ""); set_num(v2, na, 0, "");
    gtk_box_pack_start(GTK_BOX(row), c1, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), c2, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), ui_tree_new(st, heads, 3, NULL, 360), TRUE, TRUE, 0);
    g_object_unref(st);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), box, TRUE, TRUE, 0);
    gtk_widget_show_all(dlg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

/* CSV export of the sessions that match the filters (existing export mechanism + native file dialog). */
static void on_export_history(GtkWidget *w, gpointer d)
{
    static Report r;
    static const char *heads[7] = { "Date", "Period", "Subject", "Present", "Absent", "Total", "Present %" };
    GtkTreeModel *m = GTK_TREE_MODEL(st_hist);
    GtkTreeIter it;
    char *path, name[60], today[12];
    int j, n = 0;
    (void)w; (void)d;
    memset(&r, 0, sizeof r);
    snprintf(r.title, sizeof r.title, "Attendance History (%d sessions)", gtk_tree_model_iter_n_children(m, NULL));
    r.ncols = 7;
    for (j = 0; j < 7; j++) snprintf(r.head[j], sizeof r.head[j], "%s", heads[j]);
    if (gtk_tree_model_get_iter_first(m, &it)) do {
        for (j = 0; j < 7 && n < REP_ROWS; j++) {
            gchar *s = NULL;
            gtk_tree_model_get(m, &it, j, &s, -1);
            snprintf(r.cell[n][j], sizeof r.cell[n][j], "%s", s ? s : "");
            g_free(s);
        }
        n++;
    } while (n < REP_ROWS && gtk_tree_model_iter_next(m, &it));
    r.nrows = n;
    today_str(today);
    snprintf(name, sizeof name, "attendance_history_%s.csv", today);
    path = ui_choose_file(1, "Export attendance history as CSV", name);
    if (!path) return;
    if (report_export_csv(&r, path) == 0) ui_toast("History exported"); else ui_error("Could not write the file. Choose another folder or file name.");
    g_free(path);
}

/* ---------- summary tab ---------- */
static void show_summary(void)
{
    int i, j = gtk_combo_box_get_active(GTK_COMBO_BOX(c_sumsub)) - 1, c, a, ok = 0, low = 0, tc = 0, ta = 0;
    char cs[8], as[8], ps[16];
    GtkTreeIter it;
    gtk_list_store_clear(st_sum);
    for (i = 0; i < n_students; i++) {
        int is_low;
        double p;
        att_stats(students[i].roll, j >= 0 ? subjects[j].code : NULL, &c, &a);
        snprintf(cs, sizeof cs, "%d", c);
        snprintf(as, sizeof as, "%d", a);
        p = c > 0 ? a * 100.0 / c : 0;
        if (c > 0) snprintf(ps, sizeof ps, "%.1f", p); else strcpy(ps, "-");
        is_low = c > 0 && p < settings.att_threshold;
        if (c > 0) { tc += c; ta += a; if (is_low) low++; else ok++; }
        gtk_list_store_append(st_sum, &it);
        gtk_list_store_set(st_sum, &it, 0, students[i].roll, 1, students[i].name, 2, cs, 3, as, 4, ps,
                           5, c == 0 ? "No records" : is_low ? "LOW ATTENDANCE" : p >= 90 ? "Excellent" : p >= 80 ? "Good" : "Satisfactory",
                           6, is_low ? "#B91C1C" : "#17213D", 7, (int)(p + 0.5), 8, i, -1);
    }
    if (tc > 0) set_num(sum_val[0], ta * 100.0 / tc, 1, "%"); else gtk_label_set_text(GTK_LABEL(sum_val[0]), "-");
    set_num(sum_val[1], ok, 0, "");
    set_num(sum_val[2], low, 0, "");
}

static void on_sumsub(GtkWidget *w, gpointer d) { (void)w; (void)d; if (!busy) show_summary(); }

static void on_apply_thr(GtkWidget *w, gpointer d)
{
    (void)w; (void)d;
    settings.att_threshold = gtk_spin_button_get_value(GTK_SPIN_BUTTON(sp_thr));
    ui_save();
    ui_refresh_all();
}

/* ---------- refresh / build ---------- */
void attendance_refresh(void)
{
    char rep[900], t[1000];
    int nc;
    busy = 1;
    ui_combo_subjects(GTK_COMBO_BOX_TEXT(c_sub), 0);
    ui_combo_subjects(GTK_COMBO_BOX_TEXT(c_sumsub), 1);
    ui_combo_subjects(GTK_COMBO_BOX_TEXT(c_hsub), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(sp_thr), settings.att_threshold);
    busy = 0;
    if (!dirty || loaded_n != n_students || cur_date[0] == '\0') sync_session(); else show_take_list();
    show_history();
    show_summary();

    nc = att_find_conflicts(rep, sizeof rep);      /* old data: the same date and period recorded for several subjects */
    if (nc > 0) {
        snprintf(t, sizeof t, "Conflicting sessions found (same date and period, different subjects) - nothing was changed:\n%s"
                              "Use History > Delete session to keep only the correct subject.", rep);
        gtk_label_set_text(GTK_LABEL(l_conflict), t);
    }
    gtk_widget_set_visible(l_conflict, nc > 0);
}

/* Rounded progress bar drawn with Cairo (the stock GTK3 progress cell ignores our CSS colours). */
static void bar_cell_func(GtkTreeViewColumn *col, GtkCellRenderer *cell, GtkTreeModel *m, GtkTreeIter *it, gpointer ncols_p)
{
    int nc = GPOINTER_TO_INT(ncols_p), pct = 0, W = 230, H = 14;
    char *colour = NULL;
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t *cr = cairo_create(s);
    GdkRGBA c;
    double w, r = H / 2.0;
    gtk_tree_model_get(m, it, nc, &colour, nc + 1, &pct, -1);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    gdk_rgba_parse(&c, (colour && !strcmp(colour, "#B91C1C")) ? "#DC2626" : "#16A34A");
    cairo_set_source_rgb(cr, 0.886, 0.910, 0.941);                       /* #E2E8F0 trough */
    cairo_arc(cr, r, r, r, M_PI / 2, 3 * M_PI / 2); cairo_arc(cr, W - r, r, r, -M_PI / 2, M_PI / 2); cairo_close_path(cr); cairo_fill(cr);
    w = (W - 0) * pct / 100.0;
    if (w > 0) {
        if (w < H) w = H;
        gdk_cairo_set_source_rgba(cr, &c);
        cairo_arc(cr, r, r, r, M_PI / 2, 3 * M_PI / 2); cairo_arc(cr, w - r, r, r, -M_PI / 2, M_PI / 2); cairo_close_path(cr); cairo_fill(cr);
    }
    {
        GdkPixbuf *pb = gdk_pixbuf_get_from_surface(s, 0, 0, W, H);
        g_object_set(cell, "pixbuf", pb, NULL);
        if (pb) g_object_unref(pb);
    }
    cairo_destroy(cr); cairo_surface_destroy(s); g_free(colour);
}

/* Tree with a progress-bar column.  Store: ncols strings, colour string, percent int, key int. */
static GtkWidget *progress_tree(GtkListStore *st, const char **titles, int ncols, int min_h, GtkTreeView **tv_out)
{
    GtkWidget *tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(st)), *sw = gtk_scrolled_window_new(NULL, NULL);
    GtkCellRenderer *pr = gtk_cell_renderer_pixbuf_new();
    g_object_set(pr, "xalign", 0.0, "xpad", 6, NULL);
    GtkTreeViewColumn *pc;
    int i;
    for (i = 0; i < ncols; i++) {
        GtkCellRenderer *r = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(titles[i], r, "text", i, "foreground", ncols, NULL);
        gtk_tree_view_column_set_resizable(c, TRUE);
        gtk_tree_view_column_set_min_width(c, 60);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tv), c);
    }
    pc = gtk_tree_view_column_new_with_attributes("Progress", pr, NULL);
    gtk_tree_view_column_set_cell_data_func(pc, pr, bar_cell_func, GINT_TO_POINTER(ncols), NULL);
    gtk_tree_view_column_set_min_width(pc, 150);
    gtk_tree_view_column_set_expand(pc, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tv), pc);
    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(tv), GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(sw), min_h);
    gtk_container_add(GTK_CONTAINER(sw), tv);
    if (tv_out) *tv_out = GTK_TREE_VIEW(tv);
    return sw;
}

GtkWidget *attendance_build(void)
{
    static const char *h1[4] = { "Roll No", "Name", "Status", "Overall att. %" };
    static const char *h2[7] = { "Date", "Period", "Subject", "Present", "Absent", "Total", "Present %" };
    static const char *h3[6] = { "Roll No", "Name", "Classes held", "Attended", "Attendance %", "Status" };
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12), *card, *inner, *row, *sb, *g, *b;
    char today[12];
    int i;

    gtk_box_pack_start(GTK_BOX(page), ui_title("Attendance", "Take attendance, review history and track low attendance"), FALSE, FALSE, 0);
    nb = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(page), nb, TRUE, TRUE, 0);

    /* =============== tab 1: take attendance =============== */
    card = ui_card(&inner);
    l_conflict = ui_label("", "warn");
    gtk_label_set_line_wrap(GTK_LABEL(l_conflict), TRUE);
    gtk_widget_set_no_show_all(l_conflict, TRUE);
    gtk_box_pack_start(GTK_BOX(inner), l_conflict, FALSE, FALSE, 0);

    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);                 /* session selection */
    e_date = gtk_entry_new();
    today_str(today);
    gtk_entry_set_text(GTK_ENTRY(e_date), today);
    gtk_entry_set_width_chars(GTK_ENTRY(e_date), 11);
    g_signal_connect(e_date, "changed", G_CALLBACK(on_date_changed), NULL);
    c_sub = gtk_combo_box_text_new();
    lock_icon = ui_icon_new(ICON_LOCK, 30, "#6842F5", "#EDE9FE");     /* professional lock icon (animated after saving) */
    gtk_widget_set_no_show_all(lock_icon, TRUE);
    gtk_widget_set_tooltip_text(lock_icon, "Subject locked for this period. Delete the session in History to change it.");
    l_period = ui_label("1", "period-val");
    b_per_dn = ui_button("−", NULL, G_CALLBACK(on_period_step), GINT_TO_POINTER(-1));
    b_per_up = ui_button("+", NULL, G_CALLBACK(on_period_step), GINT_TO_POINTER(1));
    sp_period = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(sp_period), b_per_dn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sp_period), l_period, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sp_period), b_per_up, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(sp_period, "Period 1-8: each click changes the period by one");
    set_period(1);
    gtk_box_pack_start(GTK_BOX(row), ui_icon_new(ICON_CALENDAR, 30, "#6842F5", "#EDE9FE"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), ui_labeled("Date", e_date), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), ui_button("Pick", NULL, G_CALLBACK(on_pick), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), ui_button("Today", NULL, G_CALLBACK(on_today), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), ui_labeled("Period", sp_period), FALSE, FALSE, 10);
    sb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(sb), c_sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sb), lock_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), ui_labeled("Subject", sb), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), ui_icon_button(ICON_SEARCH, "Load", NULL, G_CALLBACK(on_load), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);

    l_session = ui_label("", "hint");
    gtk_box_pack_start(GTK_BOX(inner), l_session, FALSE, FALSE, 0);

    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);                 /* live counters */
    gtk_box_set_homogeneous(GTK_BOX(row), TRUE);
    gtk_box_pack_start(GTK_BOX(row), counter("Total students", 0, &cnt_val[0]), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), counter("Present", 1, &cnt_val[1]), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), counter("Absent", 2, &cnt_val[2]), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), counter("Not marked", 0, &cnt_val[3]), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);

    st_take = ui_store_new(4);
    gtk_box_pack_start(GTK_BOX(inner), ui_tree_new(st_take, h1, 4, &tv_take, 300), TRUE, TRUE, 0);
    g_signal_connect(tv_take, "row-activated", G_CALLBACK(on_row_activated), NULL);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(row), ui_icon_button(ICON_CHECK_CIRCLE, "Present", NULL, G_CALLBACK(on_present), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), ui_icon_button(ICON_CROSS_CIRCLE, "Absent", NULL, G_CALLBACK(on_absent), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), ui_button("Mark all present", NULL, G_CALLBACK(on_all_present), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), ui_button("Mark all absent", NULL, G_CALLBACK(on_all_absent), NULL), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(row), ui_icon_button(ICON_CHECK_CIRCLE, "Save attendance", "accent", G_CALLBACK(on_save), NULL), FALSE, FALSE, 0);
    save_ok = ui_icon_new(ICON_CHECK_CIRCLE, 28, "#16A34A", NULL);     /* animated success check after saving */
    gtk_widget_set_no_show_all(save_ok, TRUE);
    gtk_box_pack_end(GTK_BOX(row), save_ok, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Each period of a date has ONE subject. Changing the date or period loads the saved session automatically. "
                                               "Select a student and press Present/Absent, or double-click a row to toggle; saving again edits the session without duplicates.", "hint"), FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), card, gtk_label_new("Take attendance"));

    /* =============== tab 2: history =============== */
    card = ui_card(&inner);
    g = gtk_grid_new();                                                /* three summary cards that follow the filters */
    gtk_grid_set_column_spacing(GTK_GRID(g), 12);
    gtk_grid_set_column_homogeneous(GTK_GRID(g), TRUE);
    gtk_grid_attach(GTK_GRID(g), mini_card(ICON_ATTENDANCE, "#6842F5", "#EDE9FE", "Classes conducted", &hist_val[0]), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), mini_card(ICON_PERCENT, "#16A34A", "#DCFCE7", "Average attendance", &hist_val[1]), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), mini_card(ICON_SUBJECTS, "#F59E0B", "#FEF3C7", "Subjects conducted", &hist_val[2]), 2, 0, 1, 1);
    gtk_box_pack_start(GTK_BOX(inner), g, FALSE, FALSE, 0);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);                  /* filters */
    e_from = gtk_entry_new(); e_to = gtk_entry_new(); c_hsub = gtk_combo_box_text_new();
    gtk_entry_set_width_chars(GTK_ENTRY(e_from), 11); gtk_entry_set_width_chars(GTK_ENTRY(e_to), 11);
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_from), "YYYY-MM-DD"); gtk_entry_set_placeholder_text(GTK_ENTRY(e_to), "YYYY-MM-DD");
    g_signal_connect(e_from, "changed", G_CALLBACK(on_hist_filter), NULL);
    g_signal_connect(e_to, "changed", G_CALLBACK(on_hist_filter), NULL);
    g_signal_connect(c_hsub, "changed", G_CALLBACK(on_hist_filter), NULL);
    gtk_box_pack_start(GTK_BOX(row), ui_labeled("From", e_from), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), ui_labeled("To", e_to), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), ui_labeled("Subject", c_hsub), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(row), ui_icon_button(ICON_EXPORT, "Export CSV", NULL, G_CALLBACK(on_export_history), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);
    st_hist = gtk_list_store_new(10, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);
    gtk_box_pack_start(GTK_BOX(inner), progress_tree(st_hist, h2, 7, 300, &tv_hist), TRUE, TRUE, 0);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(row), ui_icon_button(ICON_USER_CHECK, "View Session", "accent", G_CALLBACK(on_view_session), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), ui_icon_button(ICON_EDIT, "Edit Session", NULL, G_CALLBACK(on_edit_session), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), ui_icon_button(ICON_DELETE, "Delete Session", "danger", G_CALLBACK(on_delete_session), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Recorded the wrong subject for a period? Delete that session here, then record the correct subject in Take attendance. "
                                               "Dates are YYYY-MM-DD; leave a date empty for no limit.", "hint"), FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), card, gtk_label_new("History"));

    /* =============== tab 3: summary & warnings =============== */
    card = ui_card(&inner);
    g = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(g), 12);
    gtk_grid_set_column_homogeneous(GTK_GRID(g), TRUE);
    gtk_grid_attach(GTK_GRID(g), mini_card(ICON_ATTENDANCE, "#6842F5", "#EDE9FE", "Overall attendance", &sum_val[0]), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), mini_card(ICON_CHECK_CIRCLE, "#16A34A", "#DCFCE7", "Satisfactory or better", &sum_val[1]), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), mini_card(ICON_WARNING, "#DC2626", "#FEE2E2", "Low attendance", &sum_val[2]), 2, 0, 1, 1);
    gtk_box_pack_start(GTK_BOX(inner), g, FALSE, FALSE, 0);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    c_sumsub = gtk_combo_box_text_new();
    g_signal_connect(c_sumsub, "changed", G_CALLBACK(on_sumsub), NULL);
    sp_thr = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(sp_thr), settings.att_threshold);
    gtk_box_pack_start(GTK_BOX(row), ui_labeled("Subject", c_sumsub), FALSE, FALSE, 0);
    b = ui_button("Apply", "accent", G_CALLBACK(on_apply_thr), NULL);
    gtk_box_pack_end(GTK_BOX(row), b, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(row), ui_labeled("Low-attendance threshold (%)", sp_thr), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);
    st_sum = gtk_list_store_new(9, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);
    gtk_box_pack_start(GTK_BOX(inner), progress_tree(st_sum, h3, 6, 300, NULL), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Attendance % = classes attended / classes held x 100. Students below the threshold are marked LOW ATTENDANCE in red.", "hint"), FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), card, gtk_label_new("Summary & warnings"));
    (void)i;
    ui_shot_tab(nb);                                        /* screenshot aid */
    return page;
}
