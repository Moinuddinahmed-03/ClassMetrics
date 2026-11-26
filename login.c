/*
 * login.c : ClassMetrics 2.0 login window (animated logo, vector decoration, icon fields)
 *           and the faculty profile page.
 * NOTE: this is a simple local DEMONSTRATION login (passwords are stored in a
 * plain CSV file).  It is not production-grade authentication.
 */
#include "ui.h"

void ui_apply_motion_setting(void);

/* ================= logo (rounded purple square with rising bars) ================= */
typedef struct { int size; double p; } LogoData;

static gboolean draw_logo(GtkWidget *w, cairo_t *cr, gpointer data)
{
    LogoData *ld = data;
    double s = ld->size, r = s * 0.26, u = s / 40.0, p = ld->p, sc = 0.7 + 0.3 * p;
    (void)w;
    cairo_translate(cr, s / 2, s / 2); cairo_scale(cr, sc, sc); cairo_translate(cr, -s / 2, -s / 2);   /* entrance: grows */
    cairo_new_sub_path(cr);
    cairo_arc(cr, s - r, r, r, -G_PI / 2, 0);
    cairo_arc(cr, s - r, s - r, r, 0, G_PI / 2);
    cairo_arc(cr, r, s - r, r, G_PI / 2, G_PI);
    cairo_arc(cr, r, r, r, G_PI, 1.5 * G_PI);
    cairo_close_path(cr);
    {
        cairo_pattern_t *g = cairo_pattern_create_linear(0, 0, s, s);
        cairo_pattern_add_color_stop_rgba(g, 0, 0.45, 0.30, 1.0, p);           /* #7350FF -> #5B34E6 */
        cairo_pattern_add_color_stop_rgba(g, 1, 0.36, 0.20, 0.90, p);
        cairo_set_source(cr, g);
        cairo_fill(cr);
        cairo_pattern_destroy(g);
    }
    cairo_set_source_rgba(cr, 1, 1, 1, p);
    {   /* three bars rise one after another */
        static const double bx[3] = { 9, 17, 25 }, bh[3] = { 10, 17, 24 };
        int i;
        for (i = 0; i < 3; i++) {
            double k = p * 3.0 - i * 0.6; if (k < 0) k = 0; if (k > 1) k = 1;
            cairo_rectangle(cr, bx[i] * u, (32 - bh[i] * k) * u, 6 * u, bh[i] * k * u);
            cairo_fill(cr);
        }
    }
    return FALSE;
}

static void logo_step(double p, gpointer d) { ((LogoData *)d)->p = p; }

static void logo_map(GtkWidget *w, gpointer d) { ui_animate(w, "logo", 480, logo_step, d, NULL); }

GtkWidget *ui_logo(int size)
{
    LogoData *ld = g_new0(LogoData, 1);
    GtkWidget *da = gtk_drawing_area_new();
    ld->size = size; ld->p = 1.0;
    gtk_widget_set_size_request(da, size, size);
    gtk_widget_set_halign(da, GTK_ALIGN_START);
    gtk_widget_set_valign(da, GTK_ALIGN_CENTER);
    g_object_set_data_full(G_OBJECT(da), "logo", ld, g_free);
    g_signal_connect(da, "draw", G_CALLBACK(draw_logo), ld);
    g_signal_connect(da, "map", G_CALLBACK(logo_map), ld);          /* animated entrance whenever it appears */
    return da;
}

/* ================= login window ================= */
static GtkWidget *win, *e_user, *e_pass, *l_error, *login_card;
static int pass_visible = 0;

static void opacity_step(double p, gpointer d) { gtk_widget_set_opacity(GTK_WIDGET(d), p); }
static void fade_out_step(double p, gpointer d) { gtk_widget_set_opacity(GTK_WIDGET(d), 1.0 - p); }

static gboolean finish_login(gpointer d)
{
    app_login_ok(GPOINTER_TO_INT(d));
    return G_SOURCE_REMOVE;
}

static void do_login(void)
{
    const char *u = gtk_entry_get_text(GTK_ENTRY(e_user));
    const char *p = gtk_entry_get_text(GTK_ENTRY(e_pass));
    int i;
    for (i = 0; i < n_teachers; i++)
        if (strcmp(teachers[i].username, u) == 0 && strcmp(teachers[i].password, p) == 0) {
            gtk_label_set_text(GTK_LABEL(l_error), "");
            if (ui_anim_on()) {                       /* smooth transition: the login window fades out (220 ms) */
                ui_animate(win, "fade", 220, fade_out_step, win, NULL);
                g_timeout_add(230, finish_login, GINT_TO_POINTER(i));
            } else app_login_ok(i);
            return;
        }
    gtk_label_set_text(GTK_LABEL(l_error), "Invalid username or password.");
}

static void on_login(GtkWidget *w, gpointer d) { (void)w; (void)d; do_login(); }

static void set_eye_icon(void)
{
    GdkPixbuf *pb = ui_icon_pixbuf(pass_visible ? ICON_EYE_OFF : ICON_EYE, 20, "#64748B");
    gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(e_pass), GTK_ENTRY_ICON_SECONDARY, pb);
    gtk_entry_set_icon_tooltip_text(GTK_ENTRY(e_pass), GTK_ENTRY_ICON_SECONDARY, pass_visible ? "Hide password" : "Show password");
    g_object_unref(pb);
}

static void on_eye(GtkEntry *e, GtkEntryIconPosition pos, GdkEvent *ev, gpointer d)
{
    (void)ev; (void)d;
    if (pos != GTK_ENTRY_ICON_SECONDARY) return;
    pass_visible = !pass_visible;
    gtk_entry_set_visibility(e, pass_visible);
    set_eye_icon();
}

static gboolean on_login_delete(GtkWidget *w, GdkEvent *e, gpointer d)
{
    (void)w; (void)e; (void)d;
    gtk_main_quit();
    return FALSE;
}

void login_window_reset(void)
{
    gtk_entry_set_text(GTK_ENTRY(e_pass), "");
    gtk_label_set_text(GTK_LABEL(l_error), "");
    gtk_widget_set_opacity(win, 1.0);
    gtk_widget_grab_focus(e_pass);
}

/* Decorative background: navy gradient, soft circles, dotted grid (vector only, no image files). */
static gboolean draw_login_bg(GtkWidget *w, cairo_t *cr, gpointer d)
{
    double W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w), x, y;
    cairo_pattern_t *g = cairo_pattern_create_linear(0, 0, W, H);
    (void)d;
    cairo_pattern_add_color_stop_rgb(g, 0, 0.078, 0.129, 0.239);       /* #14213D */
    cairo_pattern_add_color_stop_rgb(g, 1, 0.200, 0.140, 0.440);       /* deep purple */
    cairo_set_source(cr, g);
    cairo_paint(cr);
    cairo_pattern_destroy(g);
    cairo_set_source_rgba(cr, 0.41, 0.26, 0.96, 0.18);                 /* big soft circles */
    cairo_arc(cr, W * 0.12, H * 0.14, H * 0.28, 0, 2 * G_PI); cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.55, 0.36, 1.0, 0.12);
    cairo_arc(cr, W * 0.92, H * 0.88, H * 0.36, 0, 2 * G_PI); cairo_fill(cr);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.07);                          /* dotted grid */
    for (x = 24; x < W; x += 34) for (y = 24; y < H; y += 34) { cairo_arc(cr, x, y, 1.4, 0, 2 * G_PI); cairo_fill(cr); }
    return FALSE;
}

GtkWidget *login_window_new(void)
{
    GtkWidget *overlay, *bg, *deco, *card, *btn, *hint, *col, *brand_row, *u_lbl, *p_lbl;
    GdkPixbuf *pb;
    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "ClassMetrics 2.0 - Login");
    gtk_window_set_default_size(GTK_WINDOW(win), 760, 640);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "login-bg");
    g_signal_connect(win, "delete-event", G_CALLBACK(on_login_delete), NULL);

    overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(win), overlay);
    bg = gtk_drawing_area_new();
    g_signal_connect(bg, "draw", G_CALLBACK(draw_login_bg), NULL);
    gtk_container_add(GTK_CONTAINER(overlay), bg);

    deco = ui_icon_new(ICON_GRAD, 190, "#34406E", NULL);              /* academic-themed outlines */
    gtk_widget_set_halign(deco, GTK_ALIGN_END); gtk_widget_set_valign(deco, GTK_ALIGN_START);
    gtk_widget_set_margin_end(deco, 40); gtk_widget_set_margin_top(deco, 30);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), deco);
    deco = ui_icon_new(ICON_MATH, 150, "#2F3A68", NULL);
    gtk_widget_set_halign(deco, GTK_ALIGN_START); gtk_widget_set_valign(deco, GTK_ALIGN_END);
    gtk_widget_set_margin_start(deco, 40); gtk_widget_set_margin_bottom(deco, 30);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), deco);
    deco = ui_icon_new(ICON_DASHBOARD, 120, "#2F3A68", NULL);
    gtk_widget_set_halign(deco, GTK_ALIGN_END); gtk_widget_set_valign(deco, GTK_ALIGN_END);
    gtk_widget_set_margin_end(deco, 60); gtk_widget_set_margin_bottom(deco, 40);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), deco);

    card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    login_card = card;
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "login-card");
    gtk_widget_set_halign(card, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(card, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(card, 390, -1);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), card);

    brand_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_box_pack_start(GTK_BOX(brand_row), ui_logo(60), FALSE, FALSE, 0);
    col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(col), ui_label("ClassMetrics", "login-title"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(col), ui_label("Version 2.0", "hint"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(brand_row), col, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), brand_row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), ui_label("Student Marks & Attendance Analyzer", "page-sub"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), gtk_label_new(""), FALSE, FALSE, 6);

    u_lbl = ui_label("Username", "card-label");
    gtk_box_pack_start(GTK_BOX(card), u_lbl, FALSE, FALSE, 0);
    e_user = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_user), "Enter your username");
    pb = ui_icon_pixbuf(ICON_PROFILE, 20, "#64748B");
    gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(e_user), GTK_ENTRY_ICON_PRIMARY, pb);
    g_object_unref(pb);
    g_signal_connect(e_user, "activate", G_CALLBACK(on_login), NULL);
    gtk_box_pack_start(GTK_BOX(card), e_user, FALSE, FALSE, 0);

    p_lbl = ui_label("Password", "card-label");
    gtk_box_pack_start(GTK_BOX(card), p_lbl, FALSE, FALSE, 4);
    e_pass = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(e_pass), FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_pass), "Enter your password");
    pb = ui_icon_pixbuf(ICON_LOCK, 20, "#64748B");
    gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(e_pass), GTK_ENTRY_ICON_PRIMARY, pb);
    g_object_unref(pb);
    set_eye_icon();                                                      /* show / hide password icon */
    g_signal_connect(e_pass, "icon-press", G_CALLBACK(on_eye), NULL);
    g_signal_connect(e_pass, "activate", G_CALLBACK(on_login), NULL);
    gtk_box_pack_start(GTK_BOX(card), e_pass, FALSE, FALSE, 0);

    l_error = ui_label("", "error");
    gtk_box_pack_start(GTK_BOX(card), l_error, FALSE, FALSE, 0);
    btn = ui_button("Login", "accent", G_CALLBACK(on_login), NULL);
    gtk_box_pack_start(GTK_BOX(card), btn, FALSE, FALSE, 4);

    hint = ui_label("Demonstration account (local file login):\nusername  teacher     password  class123", "hint");
    gtk_box_pack_start(GTK_BOX(card), hint, FALSE, FALSE, 8);

    ui_animate(card, "fade-in", 380, opacity_step, card, NULL);           /* card fades in */
    return win;
}

/* ================= faculty profile page ================= */
#define NP 10
static GtkWidget *p_val[NP], *p_avatar_box, *p_subj_card;
static const char *P_LABEL[NP] = { "Teacher name", "Faculty ID", "Department", "Email", "Assigned class", "Semester", "Academic year",
                                   "Subject", "Subject code", "Institution" };

void profile_refresh(void)
{
    Teacher *t = &teachers[g_teacher];
    char sem[8];
    GdkPixbuf *pb;
    snprintf(sem, sizeof sem, "%d", t->semester);
    gtk_label_set_text(GTK_LABEL(p_val[0]), t->name);
    gtk_label_set_text(GTK_LABEL(p_val[1]), t->faculty_id[0] ? t->faculty_id : "-");
    gtk_label_set_text(GTK_LABEL(p_val[2]), t->dept[0] ? t->dept : "-");
    gtk_label_set_text(GTK_LABEL(p_val[3]), t->email[0] ? t->email : "-");
    gtk_label_set_text(GTK_LABEL(p_val[4]), t->cls[0] ? t->cls : "-");
    gtk_label_set_text(GTK_LABEL(p_val[5]), sem);
    gtk_label_set_text(GTK_LABEL(p_val[6]), t->year[0] ? t->year : "-");
    gtk_label_set_text(GTK_LABEL(p_val[7]), t->subject[0] ? t->subject : "-");
    gtk_label_set_text(GTK_LABEL(p_subj_card), t->subject[0] ? t->subject : "-");
    gtk_label_set_text(GTK_LABEL(p_val[8]), t->subject_code[0] ? t->subject_code : "-");
    gtk_label_set_text(GTK_LABEL(p_val[9]), t->institution[0] ? t->institution : "-");
    gtk_container_foreach(GTK_CONTAINER(p_avatar_box), (GtkCallback)gtk_widget_destroy, NULL);
    pb = ui_avatar_pixbuf(t->name, 76, "#FFFFFF", "#6842F5");
    gtk_box_pack_start(GTK_BOX(p_avatar_box), gtk_image_new_from_pixbuf(pb), FALSE, FALSE, 0);
    g_object_unref(pb);
    gtk_widget_show_all(p_avatar_box);
}

static void on_edit_profile(GtkWidget *w, gpointer d)
{
    Teacher *t = &teachers[g_teacher];
    GtkWidget *dlg = gtk_dialog_new_with_buttons("Edit profile", GTK_WINDOW(g_window), GTK_DIALOG_MODAL,
                                                 "Cancel", GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_OK, NULL);
    GtkWidget *grid = gtk_grid_new(), *en[NP];
    char sem[8];
    int i;
    (void)w; (void)d;
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 14);
    snprintf(sem, sizeof sem, "%d", t->semester);
    {
        const char *init[NP] = { t->name, t->faculty_id, t->dept, t->email, t->cls, sem, t->year, t->subject, t->subject_code, t->institution };
        for (i = 0; i < NP; i++) {
            en[i] = gtk_entry_new();
            gtk_entry_set_text(GTK_ENTRY(en[i]), init[i]);
            gtk_widget_set_hexpand(en[i], TRUE);
            gtk_entry_set_width_chars(GTK_ENTRY(en[i]), 36);
            gtk_grid_attach(GTK_GRID(grid), ui_label(P_LABEL[i], "card-label"), 0, i, 1, 1);
            gtk_grid_attach(GTK_GRID(grid), en[i], 1, i, 1, 1);
        }
    }
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), grid, TRUE, TRUE, 0);
    gtk_widget_show_all(dlg);
    while (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
        char v[NP][80];
        int semv;
        for (i = 0; i < NP; i++) {
            strncpy(v[i], gtk_entry_get_text(GTK_ENTRY(en[i])), 79); v[i][79] = '\0';
            clean_text(v[i]);
        }
        if (v[0][0] == '\0' || v[1][0] == '\0' || v[4][0] == '\0') { ui_error("Name, Faculty ID and class cannot be empty."); continue; }
        if (!parse_int(v[5], &semv) || semv < 1 || semv > 8) { ui_error("Semester must be a number from 1 to 8."); continue; }
        strncpy(t->name, v[0], sizeof t->name - 1);       strncpy(t->faculty_id, v[1], sizeof t->faculty_id - 1);
        strncpy(t->dept, v[2], sizeof t->dept - 1);       strncpy(t->email, v[3], sizeof t->email - 1);
        strncpy(t->cls, v[4], sizeof t->cls - 1);         t->semester = semv;
        strncpy(t->year, v[6], sizeof t->year - 1);
        strncpy(t->subject, v[7], sizeof t->subject - 1);
        strncpy(t->subject_code, v[8], sizeof t->subject_code - 1);
        strncpy(t->institution, v[9], sizeof t->institution - 1);
        ui_save();
        ui_refresh_all();
        break;
    }
    gtk_widget_destroy(dlg);
}

static void on_anim_toggle(GObject *sw, GParamSpec *ps, gpointer d)
{
    (void)ps; (void)d;
    settings.animations = gtk_switch_get_active(GTK_SWITCH(sw)) ? 1 : 0;
    ui_apply_motion_setting();
    ui_save();
}

/* One information card: soft icon square, small title, value. */
static GtkWidget *info_card(int icon, const char *title, GtkWidget *value)
{
    GtkWidget *inner, *card = ui_card(&inner), *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12), *col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(row), ui_icon_new(icon, 40, "#6842F5", "#EDE9FE"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(col), ui_label(title, "info-card-title"), FALSE, FALSE, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(value), "info-card-value");
    gtk_label_set_xalign(GTK_LABEL(value), 0);
    gtk_label_set_line_wrap(GTK_LABEL(value), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(value), PANGO_ELLIPSIZE_END);
    gtk_label_set_selectable(GTK_LABEL(value), TRUE);
    gtk_box_pack_start(GTK_BOX(col), value, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), col, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);
    gtk_widget_set_hexpand(card, TRUE);
    return card;
}

GtkWidget *profile_build(void)
{
    static const int order[9] = { 1, 7, 8, 3, 2, 4, 5, 6, 9 };                 /* p_val index of each info card */
    static const int icons[9] = { ICON_PROFILE, ICON_BOOK_OPEN, ICON_CODE, ICON_MAIL, ICON_BUILDING, ICON_STUDENTS, ICON_CALENDAR, ICON_CALENDAR, ICON_BUILDING };
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16), *hero, *row, *col, *grid, *inner, *card, *sw, *brow;
    int i;
    gtk_box_pack_start(GTK_BOX(page), ui_title("Faculty Profile", "Your account, assigned subject and class"), FALSE, FALSE, 0);

    /* hero card: avatar, name, subject and the two actions */
    hero = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    gtk_style_context_add_class(gtk_widget_get_style_context(hero), "hero-card");
    p_avatar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(hero), p_avatar_box, FALSE, FALSE, 0);
    col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_valign(col, GTK_ALIGN_CENTER);
    for (i = 0; i < NP; i++) p_val[i] = ui_label("", NULL);
    gtk_style_context_add_class(gtk_widget_get_style_context(p_val[0]), "hero-title");
    gtk_style_context_add_class(gtk_widget_get_style_context(p_val[7]), "hero-sub");
    gtk_label_set_ellipsize(GTK_LABEL(p_val[0]), PANGO_ELLIPSIZE_END);
    gtk_label_set_ellipsize(GTK_LABEL(p_val[7]), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(col), p_val[0], FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(col), p_val[7], FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hero), col, TRUE, TRUE, 0);
    brow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_valign(brow, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(brow), ui_icon_button(ICON_EDIT, "Edit profile", NULL, G_CALLBACK(on_edit_profile), NULL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(brow), ui_icon_button(ICON_LOGOUT, "Logout", "danger", G_CALLBACK(app_logout), NULL), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(hero), brow, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), hero, FALSE, FALSE, 0);

    /* information cards (3 columns) */
    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 14);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 14);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    p_subj_card = ui_label("", NULL);                                  /* the hero shows p_val[7]; the card needs its own label */
    for (i = 0; i < 9; i++)
        gtk_grid_attach(GTK_GRID(grid), info_card(icons[i], P_LABEL[order[i]], order[i] == 7 ? p_subj_card : p_val[order[i]]), i % 3, i / 3, 1, 1);
    gtk_box_pack_start(GTK_BOX(page), grid, FALSE, FALSE, 0);

    /* preferences */
    card = ui_card(&inner);
    gtk_box_pack_start(GTK_BOX(inner), ui_label("Preferences", "card-head"), FALSE, FALSE, 0);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(row), ui_icon_new(ICON_SETTINGS, 36, "#6842F5", "#EDE9FE"), FALSE, FALSE, 0);
    col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(col), ui_label("Interface animations", "info-card-value"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(col), ui_label("Short, subtle motion (icons, charts, count-up numbers, page fades). Turn off for reduced motion.", "hint"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), col, TRUE, TRUE, 0);
    sw = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(sw), settings.animations != 0);
    gtk_widget_set_valign(sw, GTK_ALIGN_CENTER);
    g_signal_connect(sw, "notify::active", G_CALLBACK(on_anim_toggle), NULL);
    gtk_box_pack_end(GTK_BOX(row), sw, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner), row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), card, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), ui_label("Login is a simple local demonstration account stored in data/teachers.csv.", "hint"), FALSE, FALSE, 0);
    return page;
}
