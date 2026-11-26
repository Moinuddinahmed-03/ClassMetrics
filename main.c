/*
 * main.c : program entry point and the main window (sidebar + pages).
 *
 * Flow:  main() -> load CSV data -> login window -> main window with 8 pages.
 * ClassMetrics 2.0: premium navy sidebar with vector icons, faculty card and logout,
 * crossfade between pages, optional automatic screenshots (development aid).
 */
#include "ui.h"
#ifdef _WIN32
#include <windows.h>
#endif

GtkWidget *g_window = NULL;
GtkWidget *g_shot_dialog = NULL;     /* set by dialogs that may be captured in screenshot mode */
int        g_teacher = 0;

static GtkWidget *login_win = NULL;
static GtkWidget *stack = NULL;
static GtkWidget *nav_btn[8], *nav_icon[8];
static GtkWidget *side_name = NULL, *side_sub = NULL, *side_avatar_box = NULL;
static int current_page = 0;
static GtkWidget *toast_rev = NULL, *toast_label = NULL, *toast_icon = NULL;
static guint toast_timer = 0;

static const char *PAGE_NAME[8]  = { "dashboard", "students", "subjects", "marks", "attendance", "math", "reports", "profile" };
static const char *PAGE_LABEL[8] = { "Dashboard", "Students", "Subjects", "Marks", "Attendance", "Math Analyzer", "Reports", "Profile" };
static const int   PAGE_ICON[8]  = { ICON_DASHBOARD, ICON_STUDENTS, ICON_SUBJECTS, ICON_MARKS, ICON_ATTENDANCE, ICON_MATH, ICON_REPORTS, ICON_PROFILE };

/* Refreshes the content of one page from the stored data. */
static void refresh_page(int i)
{
    switch (i) {
        case 0: dashboard_refresh(); break;
        case 1: students_refresh(); break;
        case 2: subjects_refresh(); break;
        case 3: marks_refresh(); break;
        case 4: attendance_refresh(); break;
        case 5: math_refresh(); break;
        case 6: reports_refresh(); break;
        case 7: profile_refresh(); break;
    }
}

/* Faculty card at the bottom of the sidebar (initials avatar, name, subject code and class). */
static void update_side_profile(void)
{
    Teacher *t = &teachers[g_teacher];
    GdkPixbuf *pb;
    GtkWidget *img;
    char s[120];
    gtk_label_set_text(GTK_LABEL(side_name), t->name);
    snprintf(s, sizeof s, "%s  |  %s", t->subject_code[0] ? t->subject_code : "-", t->cls);
    gtk_label_set_text(GTK_LABEL(side_sub), s);
    gtk_container_foreach(GTK_CONTAINER(side_avatar_box), (GtkCallback)gtk_widget_destroy, NULL);
    pb = ui_avatar_pixbuf(t->name, 40, "#6842F5", "#FFFFFF");
    img = gtk_image_new_from_pixbuf(pb);
    g_object_unref(pb);
    gtk_box_pack_start(GTK_BOX(side_avatar_box), img, FALSE, FALSE, 0);
    gtk_widget_show_all(side_avatar_box);
}

void ui_refresh_all(void)
{
    int i;
    if (g_window == NULL) return;
    for (i = 0; i < 8; i++) refresh_page(i);
    update_side_profile();
}

static gboolean toast_hide(gpointer d)
{
    (void)d;
    gtk_revealer_set_reveal_child(GTK_REVEALER(toast_rev), FALSE);
    toast_timer = 0;
    return G_SOURCE_REMOVE;
}

/* Small confirmation at the bottom of the window with an animated check mark (about 1.8 s). */
void ui_toast(const char *msg)
{
    if (!toast_rev) return;
    gtk_label_set_text(GTK_LABEL(toast_label), msg);
    gtk_revealer_set_reveal_child(GTK_REVEALER(toast_rev), TRUE);
    ui_icon_animate(toast_icon);
    if (toast_timer) g_source_remove(toast_timer);
    toast_timer = g_timeout_add(1800, toast_hide, NULL);
}

void ui_save(void)
{
    if (storage_save_all() != 0) { ui_error("Could not save data files. Check that the data folder is writable."); return; }
    ui_toast("Saved successfully");
}

static void select_page(int i)
{
    int k;
    current_page = i;
    gtk_stack_set_visible_child_name(GTK_STACK(stack), PAGE_NAME[i]);
    for (k = 0; k < 8; k++) {
        GtkStyleContext *sc = gtk_widget_get_style_context(nav_btn[k]);
        if (k == i) gtk_style_context_add_class(sc, "active"); else gtk_style_context_remove_class(sc, "active");
        ui_icon_set_color(nav_icon[k], k == i ? "#FFFFFF" : "#C7D0E6");
    }
    ui_icon_animate(nav_icon[i]);                      /* small motion on the selected icon */
    refresh_page(i);
}

/* crossfade between pages only while animations are on */
void app_apply_motion(void)
{
    if (!stack) return;
    gtk_stack_set_transition_type(GTK_STACK(stack), ui_anim_on() ? GTK_STACK_TRANSITION_TYPE_CROSSFADE : GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 180);
}

static void on_nav(GtkButton *b, gpointer data) { (void)b; select_page(GPOINTER_TO_INT(data)); }

static gboolean on_main_delete(GtkWidget *w, GdkEvent *e, gpointer d)
{
    (void)w; (void)e; (void)d;
    ui_save();
    gtk_main_quit();
    return FALSE;
}

void app_logout(void)
{
    if (!ui_confirm("Log out of ClassMetrics?")) return;
    ui_save();
    gtk_widget_hide(g_window);
    login_window_reset();
    gtk_widget_show_all(login_win);
}

static void build_main_window(void)
{
    GtkWidget *root, *side, *content, *logo_box, *brand, *spacer, *out, *prof, *row, *outbox;
    GdkScreen *scr = gdk_screen_get_default();
    int i, w = 1280, h = 800;
    const char *sz = getenv("CLASSMETRICS_SIZE");

    g_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_window), "ClassMetrics 2.0 - Student Marks & Attendance Analyzer");
    if (scr) {                                         /* fits 1366 x 768 as well as larger screens */
        if (gdk_screen_get_width(scr) - 40 < w) w = gdk_screen_get_width(scr) - 40;
        if (gdk_screen_get_height(scr) - 90 < h) h = gdk_screen_get_height(scr) - 90;
    }
    if (sz) sscanf(sz, "%dx%d", &w, &h);               /* test hook: CLASSMETRICS_SIZE=1366x768 */
    gtk_window_set_default_size(GTK_WINDOW(g_window), w, h);
    gtk_window_set_position(GTK_WINDOW(g_window), GTK_WIN_POS_CENTER);
    g_signal_connect(g_window, "delete-event", G_CALLBACK(on_main_delete), NULL);

    root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    {   /* overlay: the content plus the toast message */
        GtkWidget *ov = gtk_overlay_new(), *tb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_container_add(GTK_CONTAINER(g_window), ov);
        gtk_container_add(GTK_CONTAINER(ov), root);
        toast_icon = ui_icon_new(ICON_CHECK_CIRCLE, 22, "#4ADE80", NULL);
        toast_label = gtk_label_new("");
        gtk_style_context_add_class(gtk_widget_get_style_context(tb), "toast");
        gtk_box_pack_start(GTK_BOX(tb), toast_icon, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(tb), toast_label, FALSE, FALSE, 0);
        toast_rev = gtk_revealer_new();
        gtk_revealer_set_transition_type(GTK_REVEALER(toast_rev), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
        gtk_revealer_set_transition_duration(GTK_REVEALER(toast_rev), ui_anim_on() ? 200 : 0);
        gtk_container_add(GTK_CONTAINER(toast_rev), tb);
        gtk_widget_set_halign(toast_rev, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(toast_rev, GTK_ALIGN_END);
        gtk_widget_set_margin_bottom(toast_rev, 26);
        gtk_overlay_add_overlay(GTK_OVERLAY(ov), toast_rev);
    }

    /* ---- navy sidebar ---- */
    side = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(side), "sidebar");
    gtk_widget_set_size_request(side, 224, -1);
    gtk_container_set_border_width(GTK_CONTAINER(side), 14);
    gtk_box_pack_start(GTK_BOX(root), side, FALSE, FALSE, 0);

    logo_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(logo_box), ui_logo(42), FALSE, FALSE, 0);
    brand = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(brand), ui_label("ClassMetrics", "brand"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(brand), ui_label("Version 2.0", "brand-sub"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(logo_box), brand, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(side), logo_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(side), gtk_label_new(""), FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(side), ui_label("MENU", "side-sub"), FALSE, FALSE, 4);

    for (i = 0; i < 8; i++) {
        GtkWidget *b = gtk_button_new(), *bx = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12), *l = gtk_label_new(PAGE_LABEL[i]);
        nav_icon[i] = ui_icon_new(PAGE_ICON[i], 20, "#C7D0E6", NULL);
        gtk_label_set_xalign(GTK_LABEL(l), 0);
        gtk_box_pack_start(GTK_BOX(bx), nav_icon[i], FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(bx), l, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(b), bx);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "nav-item");
        g_signal_connect(b, "clicked", G_CALLBACK(on_nav), GINT_TO_POINTER(i));
        nav_btn[i] = b;
        gtk_box_pack_start(GTK_BOX(side), b, FALSE, FALSE, 1);
    }
    spacer = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(side), spacer, TRUE, TRUE, 0);

    prof = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);                 /* faculty card */
    gtk_style_context_add_class(gtk_widget_get_style_context(prof), "side-profile");
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    side_avatar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(row), side_avatar_box, FALSE, FALSE, 0);
    outbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    side_name = ui_label("", "side-user");
    gtk_label_set_ellipsize(GTK_LABEL(side_name), PANGO_ELLIPSIZE_END);
    side_sub = ui_label("", "side-sub");
    gtk_label_set_ellipsize(GTK_LABEL(side_sub), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(outbox), side_name, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outbox), side_sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), outbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(prof), row, FALSE, FALSE, 0);
    out = gtk_button_new();
    {
        GtkWidget *bx = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_box_pack_start(GTK_BOX(bx), ui_icon_new(ICON_LOGOUT, 18, "#FFFFFF", NULL), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(bx), gtk_label_new("Logout"), FALSE, FALSE, 0);
        gtk_widget_set_halign(bx, GTK_ALIGN_CENTER);
        gtk_container_add(GTK_CONTAINER(out), bx);
    }
    gtk_style_context_add_class(gtk_widget_get_style_context(out), "logout-btn");
    g_signal_connect(out, "clicked", G_CALLBACK(app_logout), NULL);
    gtk_box_pack_start(GTK_BOX(prof), out, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(side), prof, FALSE, FALSE, 0);

    /* ---- pages (crossfade, 180 ms) ---- */
    content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(root), content, TRUE, TRUE, 0);
    stack = gtk_stack_new();
    if (ui_anim_on()) {
        gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
        gtk_stack_set_transition_duration(GTK_STACK(stack), 180);
    }
    gtk_box_pack_start(GTK_BOX(content), stack, TRUE, TRUE, 0);
    gtk_stack_add_named(GTK_STACK(stack), ui_page_wrap(dashboard_build()), PAGE_NAME[0]);
    gtk_stack_add_named(GTK_STACK(stack), ui_page_wrap(students_build()), PAGE_NAME[1]);
    gtk_stack_add_named(GTK_STACK(stack), ui_page_wrap(subjects_build()), PAGE_NAME[2]);
    gtk_stack_add_named(GTK_STACK(stack), ui_page_wrap(marks_build()), PAGE_NAME[3]);
    gtk_stack_add_named(GTK_STACK(stack), ui_page_wrap(attendance_build()), PAGE_NAME[4]);
    gtk_stack_add_named(GTK_STACK(stack), ui_page_wrap(math_build()), PAGE_NAME[5]);
    gtk_stack_add_named(GTK_STACK(stack), ui_page_wrap(reports_build()), PAGE_NAME[6]);
    gtk_stack_add_named(GTK_STACK(stack), ui_page_wrap(profile_build()), PAGE_NAME[7]);
}

void app_login_ok(int teacher_index)
{
    g_teacher = teacher_index;
    report_set_teacher(teacher_index);              /* teacher named in report headers */
    if (g_window == NULL) build_main_window();
    gtk_widget_hide(login_win);
    gtk_widget_set_opacity(login_win, 1.0);
    gtk_widget_show_all(g_window);
    ui_refresh_all();
    select_page(getenv("CLASSMETRICS_PAGE") ? atoi(getenv("CLASSMETRICS_PAGE")) : 0);   /* optional start page (testing) */
}

/* ---------- screenshot mode (development aid): CLASSMETRICS_SHOT_DIR=folder ----------
 * Logs in by itself (no input simulation), saves one PNG per page and quits. */
static int shot_step = 0;

void ui_shot_save(GtkWidget *w, const char *name)
{
    const char *dir = getenv("CLASSMETRICS_SHOT_DIR");
    GdkWindow *gw = w ? gtk_widget_get_window(w) : NULL;
    GdkPixbuf *pb;
    char path[400];
    if (!dir || !gw) return;
    pb = gdk_pixbuf_get_from_window(gw, 0, 0, gdk_window_get_width(gw), gdk_window_get_height(gw));
    if (!pb) return;
    snprintf(path, sizeof path, "%s/%s.png", dir, name);
    gdk_pixbuf_save(pb, path, "png", NULL, NULL);
    g_object_unref(pb);
}

static gboolean shot_tick(gpointer d)
{
    static const char *names[8] = { "01_dashboard", "02_students", "03_subjects", "04_marks", "05_attendance", "06_math", "07_reports", "08_profile" };
    (void)d;
    if (shot_step == 0) {                      /* the first page needs time to build; entrance animations then finish before the shot */
        ui_shot_save(login_win, "00_login"); app_login_ok(0); select_page(0); shot_step = 1;
        g_timeout_add(2800, shot_tick, NULL);
        return G_SOURCE_REMOVE;
    }
    if (shot_step >= 1 && shot_step <= 8) {
        ui_shot_save(g_window, names[shot_step - 1]);
        if (shot_step < 8) select_page(shot_step);
        else if (getenv("CLASSMETRICS_SHOT_VIEW")) marks_open_view_for_shot(0);          /* modal window, closes itself */
        shot_step++;
        return TRUE;
    }
    gtk_main_quit();
    return FALSE;
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    /* Run from the folder that holds the .exe so the data/ folder is always found. */
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        char *slash = strrchr(path, '\\');
        if (slash) { *slash = '\0'; SetCurrentDirectoryA(path); }
    }
#endif
    gtk_init(&argc, &argv);
    ui_load_css();
    storage_set_dir("data");
    {
        int rc = storage_load_all();
        if (getenv("CLASSMETRICS_NOANIM")) settings.animations = 0;         /* test aid: reduced-motion path */
        if (rc != 0) {
            GtkWidget *d = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, rc == 2 ? GTK_MESSAGE_ERROR : GTK_MESSAGE_WARNING,
                                                  GTK_BUTTONS_OK, "%s",
                                                  rc == 2 ? "The data upgrade could not make a backup of your data files,\nso nothing was changed.\n"
                                                            "Check that the data folder is writable, then start the application again."
                                                          : "Could not write the data folder.\nChanges may not be saved.");
            gtk_dialog_run(GTK_DIALOG(d));
            gtk_widget_destroy(d);
            if (rc == 2) return 1;
        }
        if (migration_note[0] != '\0' && !getenv("CLASSMETRICS_SHOT_DIR")) {   /* data was upgraded: tell the user where the backup is */
            GtkWidget *d = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", migration_note);
            gtk_dialog_run(GTK_DIALOG(d));
            gtk_widget_destroy(d);
        }
        {                                              /* old attendance: same date and period recorded for two subjects */
            char rep[900], t[1100];
            if (att_find_conflicts(rep, sizeof rep) > 0) {
                GtkWidget *d;
                snprintf(t, sizeof t, "Conflicting attendance sessions were found (same date and period for different subjects).\n"
                                      "Nothing was changed:\n\n%s\nUse Attendance > History > Delete session to keep only the correct subject.", rep);
                d = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "%s", t);
                gtk_dialog_run(GTK_DIALOG(d));
                gtk_widget_destroy(d);
            }
        }
    }
    login_win = login_window_new();
    gtk_widget_show_all(login_win);
    if (getenv("CLASSMETRICS_SHOT_DIR")) g_timeout_add(1400, shot_tick, NULL);
    gtk_main();
    return 0;
}
