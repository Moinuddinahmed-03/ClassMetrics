/*
 * ui.h : declarations shared by the GTK (graphical) files.
 * The interface is created entirely in C code - no Glade/XML files are used.
 */
#ifndef UI_H
#define UI_H

#include <gtk/gtk.h>
#include "common.h"

/* ---- application state ---- */
extern GtkWidget *g_window;      /* main window                      */
extern int        g_teacher;     /* index of the logged-in teacher   */
extern GtkWidget *g_shot_dialog;          /* dialog to capture in screenshot mode */
void       ui_shot_save(GtkWidget *w, const char *name);
void       marks_open_view_for_shot(int idx);
void       ui_apply_motion_setting(void);
void       ui_shot_tab(GtkWidget *nb);
void       ui_toast(const char *msg);
void       app_apply_motion(void);

/* ---- main.c ---- */
void ui_refresh_all(void);       /* refresh every page after data changes */
void ui_save(void);              /* save all CSV files, report errors     */
void app_logout(void);
void app_login_ok(int teacher_index);

/* ---- login.c ---- */
GtkWidget *ui_logo(int size);
GtkWidget *login_window_new(void);
void       login_window_reset(void);
GtkWidget *profile_build(void);
void       profile_refresh(void);

/* ---- ui_common.c : helpers ---- */
typedef enum {
    ICON_DASHBOARD = 1, ICON_STUDENTS, ICON_SUBJECTS, ICON_MARKS, ICON_ATTENDANCE, ICON_MATH, ICON_REPORTS, ICON_PROFILE,
    ICON_LOGOUT, ICON_SEARCH, ICON_CALENDAR, ICON_SETTINGS, ICON_GRAD, ICON_PERFORMANCE, ICON_TROPHY, ICON_WARNING,
    ICON_BLOOD, ICON_CREDITS, ICON_EXPORT, ICON_IMPORT, ICON_EDIT, ICON_DELETE, ICON_LOCK, ICON_CHECK_CIRCLE,
    ICON_CROSS_CIRCLE, ICON_PERCENT, ICON_PLUS, ICON_EYE, ICON_EYE_OFF, ICON_USER_CHECK, ICON_MAIL, ICON_BUILDING,
    ICON_FLASK, ICON_CODE, ICON_CHAT, ICON_HEART, ICON_LEAF, ICON_BOOK_OPEN
} IconId;

typedef void (*UiAnimStep)(double progress, gpointer data);
int        ui_anim_on(void);                                     /* animations enabled? (Profile switch) */
void       ui_animate(GtkWidget *w, const char *key, int dur_ms, UiAnimStep step, gpointer data, GDestroyNotify free_data);
void       ui_countup(GtkWidget *label, double value, int decimals, const char *suffix);
GtkWidget *ui_icon_new(int id, int size, const char *fg, const char *bg);
void       ui_icon_set_color(GtkWidget *icon, const char *fg);
void       ui_icon_animate(GtkWidget *icon);
GdkPixbuf *ui_icon_pixbuf(int id, int size, const char *fg);
GdkPixbuf *ui_avatar_pixbuf(const char *name, int size, const char *bg, const char *fg);
GtkWidget *ui_icon_button(int icon, const char *text, const char *css_class, GCallback cb, gpointer data);
GtkWidget *ui_badge(const char *text, const char *css_class);
void       ui_load_css(void);
void       ui_info(const char *msg);
void       ui_error(const char *msg);
gboolean   ui_confirm(const char *msg);
GtkWidget *ui_title(const char *title, const char *subtitle);
GtkWidget *ui_label(const char *text, const char *css_class);
GtkWidget *ui_button(const char *text, const char *css_class, GCallback cb, gpointer data);
GtkWidget *ui_card(GtkWidget **inner_box);
GtkWidget *ui_labeled(const char *label, GtkWidget *w);
GtkWidget *ui_page_wrap(GtkWidget *content);           /* scrollable page */
GtkListStore *ui_store_new(int ncols);
GtkWidget *ui_tree_new(GtkListStore *st, const char **titles, int ncols, GtkTreeView **tv_out, int min_h);
void       ui_store_add(GtkListStore *st, int ncols, const char *color, int index, ...);
int        ui_tree_selected(GtkTreeView *tv);          /* stored index or -1 */
GtkWidget *ui_textview_new(GtkTextView **tv_out, int min_h);
void       ui_set_text(GtkTextView *tv, const char *text);
void       ui_text_dialog(const char *title, const char *text);
void       ui_combo_students(GtkComboBoxText *c);
void       ui_combo_subjects(GtkComboBoxText *c, int with_all);
void       ui_combo_exams(GtkComboBoxText *c, int default_latest);
void       ui_combo_exams_ex(GtkComboBoxText *c, int with_all);
int        ui_exam_sel(GtkWidget *combo, int with_all);
void       subject_profile_dialog(int idx);
void       ui_draw_bars(cairo_t *cr, int w, int h, const char **labels, const double *val, int n, double thr, const char *title);
void       ui_draw_line(cairo_t *cr, int w, int h, const char **labels, const double *val, int n, const char *title);
GtkWidget *ui_chart_area(GCallback draw_cb, int w, int h);
void       ui_chart_attach_anim(GtkWidget *da);
void       ui_pick_date(GtkWindow *parent, GtkEntry *entry);
GtkListStore *ui_cell_store_new(int ncols);
void       ui_cell_store_add(GtkListStore *st, int ncols, const char **cells, const char **fg, const char **bg, const char *tip, int key);
GtkWidget *ui_cell_tree_new(GtkListStore *st, const char **titles, int ncols, const float *xalign, int min_w);
GtkListStore *ui_avatar_store_new(int ncols);
void       ui_avatar_store_add(GtkListStore *st, int ncols, const char **cells, const char **fg, const char **bg, GdkPixbuf *av, int key);
GtkWidget *ui_avatar_tree_new(GtkListStore *st, const char **titles, int ncols, const float *xalign, int min_w);
char      *ui_choose_file(int save, const char *title, const char *default_name);

/* ---- pages ---- */
GtkWidget *dashboard_build(void);   void dashboard_refresh(void);
GtkWidget *students_build(void);    void students_refresh(void);
GtkWidget *subjects_build(void);    void subjects_refresh(void);
GtkWidget *marks_build(void);       void marks_refresh(void);
GtkWidget *attendance_build(void);  void attendance_refresh(void);
GtkWidget *math_build(void);        void math_refresh(void);
GtkWidget *reports_build(void);     void reports_refresh(void);
void       student_profile_dialog(int idx);

#endif
