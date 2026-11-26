/*
 * ui_icons.c : ClassMetrics 2.0 vector icon set (drawn with Cairo, no image files)
 *              and the small animation engine used for micro-interactions.
 *
 * Icons are drawn on a 24 x 24 grid with round 1.9 px strokes so every icon has the same
 * weight.  An animation progress value p (0..1) lets selected icons move a little:
 *   dashboard bars rise, student/trophy pop, graduation cap lifts, calendar check draws,
 *   lock closes, check/warning fade in.
 * Animations are short (150-500 ms), start only on events (page open, click, save) and stop
 * on their own.  They can be switched off in Profile (settings.animations).
 */
#include "ui.h"

/* ================= animation engine ================= */

int ui_anim_on(void) { return settings.animations != 0; }

typedef struct {
    gint64 start;
    int    dur;
    UiAnimStep step;
    gpointer data;
    GDestroyNotify free_data;
} Anim;

static double ease_out(double t) { double u = 1.0 - t; return 1.0 - u * u * u; }

static void anim_free(gpointer p)
{
    Anim *a = p;
    if (a->free_data) a->free_data(a->data);
    g_free(a);
}

static gboolean anim_tick(GtkWidget *w, GdkFrameClock *clock, gpointer p)
{
    Anim *a = p;
    double t;
    if (a->start == 0) a->start = gdk_frame_clock_get_frame_time(clock);
    t = (double)(gdk_frame_clock_get_frame_time(clock) - a->start) / 1000.0 / (a->dur > 0 ? a->dur : 1);
    if (t >= 1.0) t = 1.0;
    a->step(ease_out(t), a->data);
    gtk_widget_queue_draw(w);
    return t < 1.0;                                    /* FALSE removes the tick callback */
}

/*
 * Runs step(progress, data) for dur_ms (progress eased 0..1) on the frame clock of widget w.
 * key names the animation: starting the same key again replaces the running one.
 * With animations switched off the end state (progress 1) is applied at once.
 */
void ui_animate(GtkWidget *w, const char *key, int dur_ms, UiAnimStep step, gpointer data, GDestroyNotify free_data)
{
    char k[48];
    guint old;
    Anim *a;
    snprintf(k, sizeof k, "anim-%s", key);
    old = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(w), k));
    if (old) gtk_widget_remove_tick_callback(w, old);
    if (!ui_anim_on()) {                               /* reduced motion: jump to the final state */
        step(1.0, data);
        if (free_data) free_data(data);
        gtk_widget_queue_draw(w);
        g_object_set_data(G_OBJECT(w), k, NULL);
        return;
    }
    a = g_new0(Anim, 1);
    a->dur = dur_ms; a->step = step; a->data = data; a->free_data = free_data;
    step(0.0, data);
    g_object_set_data(G_OBJECT(w), k, GUINT_TO_POINTER(gtk_widget_add_tick_callback(w, anim_tick, a, anim_free)));
}

/* ---- count-up for numbers (the real value is always shown at the end) ---- */
typedef struct { GtkWidget *label; double value; char fmt[24]; char suffix[16]; int decimals; } CountUp;

static void countup_step(double p, gpointer d)
{
    CountUp *c = d;
    char t[64];
    snprintf(t, sizeof t, "%.*f%s", c->decimals, c->value * p, c->suffix);
    gtk_label_set_text(GTK_LABEL(c->label), t);
}

/* Counts a label from 0 up to its real value (decimals digits, e.g. "%" suffix). */
void ui_countup(GtkWidget *label, double value, int decimals, const char *suffix)
{
    CountUp *c = g_new0(CountUp, 1);
    c->label = label; c->value = value; c->decimals = decimals;
    snprintf(c->suffix, sizeof c->suffix, "%s", suffix ? suffix : "");
    ui_animate(label, "countup", 520, countup_step, c, g_free);
}

/* ================= icons ================= */

typedef struct { int id, size; double fr, fg, fb, br, bg, bb; int has_bg; double p; } IconData;

static void parse_hex(const char *h, double *r, double *g, double *b)
{
    unsigned v = 0;
    if (h && h[0] == '#') sscanf(h + 1, "%x", &v);
    *r = ((v >> 16) & 255) / 255.0; *g = ((v >> 8) & 255) / 255.0; *b = (v & 255) / 255.0;
}

static void rrect(cairo_t *cr, double x, double y, double w, double h, double r)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -G_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, G_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, G_PI / 2, G_PI);
    cairo_arc(cr, x + r, y + r, r, G_PI, 1.5 * G_PI);
    cairo_close_path(cr);
}

/* Draws icon id on the 24 x 24 grid.  p = animation progress (1 = finished). */
static void draw_glyph(cairo_t *cr, int id, double p)
{
    cairo_set_line_width(cr, 1.9);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    switch (id) {
    case ICON_DASHBOARD: {                                  /* three bars that rise */
        static const double x[3] = { 4, 10, 16 }, hgt[3] = { 8, 13, 17 };
        int i;
        for (i = 0; i < 3; i++) { double h = hgt[i] * (0.25 + 0.75 * p); rrect(cr, x[i], 20 - h, 4, h, 1.4); cairo_stroke(cr); }
        break; }
    case ICON_STUDENTS:
        cairo_translate(cr, 12, 12); cairo_scale(cr, 0.85 + 0.15 * p, 0.85 + 0.15 * p); cairo_translate(cr, -12, -12);
        cairo_arc(cr, 12, 8, 3.4, 0, 2 * G_PI); cairo_stroke(cr);
        cairo_move_to(cr, 5, 20); cairo_curve_to(cr, 5, 15, 8.5, 13.5, 12, 13.5); cairo_curve_to(cr, 15.5, 13.5, 19, 15, 19, 20); cairo_stroke(cr);
        break;
    case ICON_SUBJECTS:
        cairo_move_to(cr, 5, 20); cairo_line_to(cr, 17, 20); cairo_line_to(cr, 17, 4); cairo_line_to(cr, 8, 4);
        cairo_curve_to(cr, 6, 4, 5, 5, 5, 7); cairo_close_path(cr); cairo_stroke(cr);
        cairo_move_to(cr, 8.5, 8.5); cairo_line_to(cr, 14, 8.5); cairo_move_to(cr, 8.5, 12); cairo_line_to(cr, 14, 12); cairo_stroke(cr);
        cairo_move_to(cr, 17, 7); cairo_line_to(cr, 20, 7); cairo_line_to(cr, 20, 20); cairo_line_to(cr, 17, 20); cairo_stroke(cr);
        break;
    case ICON_MARKS:
        rrect(cr, 5, 5, 14, 16, 2.2); cairo_stroke(cr);
        rrect(cr, 9, 3, 6, 4, 1.2); cairo_stroke(cr);
        cairo_move_to(cr, 8.5, 12); cairo_line_to(cr, 15.5, 12); cairo_move_to(cr, 8.5, 15.5); cairo_line_to(cr, 13, 15.5); cairo_stroke(cr);
        break;
    case ICON_ATTENDANCE: case ICON_CALENDAR:
        rrect(cr, 4, 5.5, 16, 14.5, 2.5); cairo_stroke(cr);
        cairo_move_to(cr, 4, 10); cairo_line_to(cr, 20, 10);
        cairo_move_to(cr, 8.5, 3.5); cairo_line_to(cr, 8.5, 7.5); cairo_move_to(cr, 15.5, 3.5); cairo_line_to(cr, 15.5, 7.5); cairo_stroke(cr);
        if (id == ICON_ATTENDANCE) {                        /* the check mark draws itself */
            double len = 9.5 * p;
            cairo_move_to(cr, 8.5, 15);
            if (len > 0) { double a = len < 3 ? len : 3; cairo_line_to(cr, 8.5 + a * 0.72, 15 + a * 0.72); }
            if (len > 3) { double b = len - 3; cairo_line_to(cr, 10.65 + b * 0.62, 17.15 - b * 0.62); }
            cairo_stroke(cr);
        }
        break;
    case ICON_MATH:                                         /* sigma */
        cairo_move_to(cr, 17.5, 5); cairo_line_to(cr, 6.5, 5); cairo_line_to(cr, 12.5, 12); cairo_line_to(cr, 6.5, 19); cairo_line_to(cr, 17.5, 19); cairo_stroke(cr);
        break;
    case ICON_REPORTS:
        cairo_move_to(cr, 6, 3.5); cairo_line_to(cr, 14, 3.5); cairo_line_to(cr, 19, 8.5); cairo_line_to(cr, 19, 20.5); cairo_line_to(cr, 6, 20.5); cairo_close_path(cr); cairo_stroke(cr);
        cairo_move_to(cr, 14, 3.5); cairo_line_to(cr, 14, 8.5); cairo_line_to(cr, 19, 8.5);
        cairo_move_to(cr, 9, 13); cairo_line_to(cr, 16, 13); cairo_move_to(cr, 9, 16.5); cairo_line_to(cr, 14, 16.5); cairo_stroke(cr);
        break;
    case ICON_PROFILE:
        cairo_arc(cr, 12, 12, 9, 0, 2 * G_PI); cairo_stroke(cr);
        cairo_arc(cr, 12, 10, 3, 0, 2 * G_PI); cairo_stroke(cr);
        cairo_move_to(cr, 6.5, 18); cairo_curve_to(cr, 7.5, 15, 9.5, 14.2, 12, 14.2); cairo_curve_to(cr, 14.5, 14.2, 16.5, 15, 17.5, 18); cairo_stroke(cr);
        break;
    case ICON_LOGOUT:
        cairo_move_to(cr, 10, 4); cairo_line_to(cr, 6, 4); cairo_curve_to(cr, 4.9, 4, 4, 4.9, 4, 6); cairo_line_to(cr, 4, 18);
        cairo_curve_to(cr, 4, 19.1, 4.9, 20, 6, 20); cairo_line_to(cr, 10, 20); cairo_stroke(cr);
        cairo_move_to(cr, 15.5, 7.5); cairo_line_to(cr, 20, 12); cairo_line_to(cr, 15.5, 16.5); cairo_move_to(cr, 20, 12); cairo_line_to(cr, 9, 12); cairo_stroke(cr);
        break;
    case ICON_SEARCH:
        cairo_arc(cr, 10.5, 10.5, 6, 0, 2 * G_PI); cairo_stroke(cr);
        cairo_move_to(cr, 15, 15); cairo_line_to(cr, 20, 20); cairo_stroke(cr);
        break;
    case ICON_SETTINGS: {
        int i;
        cairo_arc(cr, 12, 12, 3, 0, 2 * G_PI); cairo_stroke(cr);
        cairo_arc(cr, 12, 12, 6.3, 0, 2 * G_PI); cairo_stroke(cr);
        for (i = 0; i < 8; i++) {
            double a = i * G_PI / 4;
            cairo_move_to(cr, 12 + 6.3 * cos(a), 12 + 6.3 * sin(a)); cairo_line_to(cr, 12 + 9 * cos(a), 12 + 9 * sin(a));
        }
        cairo_stroke(cr);
        break; }
    case ICON_GRAD:                                         /* graduation cap lifts up */
        cairo_translate(cr, 0, 2.2 * (1.0 - p));
        cairo_move_to(cr, 2, 9); cairo_line_to(cr, 12, 4); cairo_line_to(cr, 22, 9); cairo_line_to(cr, 12, 14); cairo_close_path(cr); cairo_stroke(cr);
        cairo_move_to(cr, 6, 11.5); cairo_line_to(cr, 6, 15.5); cairo_curve_to(cr, 6, 17.5, 9, 19, 12, 19); cairo_curve_to(cr, 15, 19, 18, 17.5, 18, 15.5); cairo_line_to(cr, 18, 11.5); cairo_stroke(cr);
        cairo_move_to(cr, 22, 9); cairo_line_to(cr, 22, 14); cairo_stroke(cr);
        break;
    case ICON_PERFORMANCE:
        cairo_move_to(cr, 3, 17); cairo_line_to(cr, 9, 11); cairo_line_to(cr, 13, 15); cairo_line_to(cr, 21, 6.5); cairo_stroke(cr);
        cairo_move_to(cr, 15, 6.5); cairo_line_to(cr, 21, 6.5); cairo_line_to(cr, 21, 12.5); cairo_stroke(cr);
        break;
    case ICON_TROPHY:
        cairo_translate(cr, 12, 12); cairo_scale(cr, 0.86 + 0.14 * p, 0.86 + 0.14 * p); cairo_translate(cr, -12, -12);
        cairo_move_to(cr, 7, 4); cairo_line_to(cr, 17, 4); cairo_line_to(cr, 17, 9); cairo_curve_to(cr, 17, 12, 14.8, 14, 12, 14); cairo_curve_to(cr, 9.2, 14, 7, 12, 7, 9); cairo_close_path(cr); cairo_stroke(cr);
        cairo_move_to(cr, 7, 6); cairo_curve_to(cr, 3, 6, 3, 11, 7.5, 11.5); cairo_move_to(cr, 17, 6); cairo_curve_to(cr, 21, 6, 21, 11, 16.5, 11.5); cairo_stroke(cr);
        cairo_move_to(cr, 12, 14); cairo_line_to(cr, 12, 18); cairo_move_to(cr, 8, 20.5); cairo_line_to(cr, 16, 20.5); cairo_stroke(cr);
        break;
    case ICON_WARNING:
        cairo_translate(cr, 12, 12); cairo_scale(cr, 0.8 + 0.2 * p, 0.8 + 0.2 * p); cairo_translate(cr, -12, -12);
        cairo_move_to(cr, 12, 3.5); cairo_line_to(cr, 21.5, 19.5); cairo_line_to(cr, 2.5, 19.5); cairo_close_path(cr); cairo_stroke(cr);
        cairo_move_to(cr, 12, 9.5); cairo_line_to(cr, 12, 13.5); cairo_stroke(cr);
        cairo_arc(cr, 12, 16.5, 0.6, 0, 2 * G_PI); cairo_fill(cr);
        break;
    case ICON_BLOOD:
        cairo_move_to(cr, 12, 3); cairo_curve_to(cr, 12, 3, 5, 10.5, 5, 15); cairo_curve_to(cr, 5, 18.9, 8.1, 22, 12, 22);
        cairo_curve_to(cr, 15.9, 22, 19, 18.9, 19, 15); cairo_curve_to(cr, 19, 10.5, 12, 3, 12, 3); cairo_close_path(cr); cairo_stroke(cr);
        break;
    case ICON_CREDITS:
        cairo_arc(cr, 12, 9, 6, 0, 2 * G_PI); cairo_stroke(cr);
        cairo_move_to(cr, 9, 14.2); cairo_line_to(cr, 7.5, 21); cairo_line_to(cr, 12, 18.5); cairo_line_to(cr, 16.5, 21); cairo_line_to(cr, 15, 14.2); cairo_stroke(cr);
        break;
    case ICON_EXPORT:
        cairo_move_to(cr, 12, 15); cairo_line_to(cr, 12, 3.5); cairo_move_to(cr, 7.5, 8); cairo_line_to(cr, 12, 3.5); cairo_line_to(cr, 16.5, 8); cairo_stroke(cr);
        cairo_move_to(cr, 4, 15); cairo_line_to(cr, 4, 18); cairo_curve_to(cr, 4, 19.7, 5.3, 21, 7, 21); cairo_line_to(cr, 17, 21); cairo_curve_to(cr, 18.7, 21, 20, 19.7, 20, 18); cairo_line_to(cr, 20, 15); cairo_stroke(cr);
        break;
    case ICON_IMPORT:
        cairo_move_to(cr, 12, 3.5); cairo_line_to(cr, 12, 15); cairo_move_to(cr, 7.5, 10.5); cairo_line_to(cr, 12, 15); cairo_line_to(cr, 16.5, 10.5); cairo_stroke(cr);
        cairo_move_to(cr, 4, 15); cairo_line_to(cr, 4, 18); cairo_curve_to(cr, 4, 19.7, 5.3, 21, 7, 21); cairo_line_to(cr, 17, 21); cairo_curve_to(cr, 18.7, 21, 20, 19.7, 20, 18); cairo_line_to(cr, 20, 15); cairo_stroke(cr);
        break;
    case ICON_EDIT:
        cairo_move_to(cr, 4, 20); cairo_line_to(cr, 5, 15); cairo_line_to(cr, 15.5, 4.5); cairo_curve_to(cr, 16.5, 3.5, 18.2, 3.5, 19.2, 4.5);
        cairo_curve_to(cr, 20.2, 5.5, 20.2, 7.2, 19.2, 8.2); cairo_line_to(cr, 8.7, 18.7); cairo_close_path(cr); cairo_stroke(cr);
        cairo_move_to(cr, 14, 6); cairo_line_to(cr, 18, 10); cairo_stroke(cr);
        break;
    case ICON_DELETE:
        cairo_move_to(cr, 4.5, 7); cairo_line_to(cr, 19.5, 7); cairo_move_to(cr, 9, 7); cairo_line_to(cr, 9, 4); cairo_line_to(cr, 15, 4); cairo_line_to(cr, 15, 7); cairo_stroke(cr);
        cairo_move_to(cr, 6.5, 7); cairo_line_to(cr, 7.6, 19.5); cairo_curve_to(cr, 7.7, 20.3, 8.3, 21, 9.1, 21); cairo_line_to(cr, 14.9, 21); cairo_curve_to(cr, 15.7, 21, 16.3, 20.3, 16.4, 19.5); cairo_line_to(cr, 17.5, 7); cairo_stroke(cr);
        cairo_move_to(cr, 10, 11); cairo_line_to(cr, 10, 17); cairo_move_to(cr, 14, 11); cairo_line_to(cr, 14, 17); cairo_stroke(cr);
        break;
    case ICON_LOCK: {                                       /* the shackle comes down and closes */
        double lift = 3.5 * (1.0 - p);
        rrect(cr, 5, 11, 14, 10, 2.4); cairo_stroke(cr);
        cairo_move_to(cr, 8, 11 - 0.0); cairo_line_to(cr, 8, 8 - lift); cairo_curve_to(cr, 8, 5.5 - lift, 10, 3.5 - lift, 12, 3.5 - lift);
        cairo_curve_to(cr, 14, 3.5 - lift, 16, 5.5 - lift, 16, 8 - lift); cairo_line_to(cr, 16, 11); cairo_stroke(cr);
        cairo_arc(cr, 12, 16, 1.2, 0, 2 * G_PI); cairo_fill(cr);
        break; }
    case ICON_CHECK_CIRCLE: {
        double len = 11.0 * p;
        cairo_arc(cr, 12, 12, 9, 0, 2 * G_PI); cairo_stroke(cr);
        cairo_move_to(cr, 7.8, 12.4);
        if (len > 0) { double a = len < 3.5 ? len : 3.5; cairo_line_to(cr, 7.8 + a * 0.72, 12.4 + a * 0.72); }
        if (len > 3.5) { double b = len - 3.5; cairo_line_to(cr, 10.3 + b * 0.62, 14.9 - b * 0.62); }
        cairo_stroke(cr);
        break; }
    case ICON_CROSS_CIRCLE:
        cairo_arc(cr, 12, 12, 9, 0, 2 * G_PI); cairo_stroke(cr);
        cairo_move_to(cr, 8.8, 8.8); cairo_line_to(cr, 15.2, 15.2); cairo_move_to(cr, 15.2, 8.8); cairo_line_to(cr, 8.8, 15.2); cairo_stroke(cr);
        break;
    case ICON_PERCENT:
        cairo_arc(cr, 7.5, 7.5, 2.8, 0, 2 * G_PI); cairo_stroke(cr);
        cairo_arc(cr, 16.5, 16.5, 2.8, 0, 2 * G_PI); cairo_stroke(cr);
        cairo_move_to(cr, 18, 5); cairo_line_to(cr, 6, 19); cairo_stroke(cr);
        break;
    case ICON_PLUS:
        cairo_move_to(cr, 12, 5); cairo_line_to(cr, 12, 19); cairo_move_to(cr, 5, 12); cairo_line_to(cr, 19, 12); cairo_stroke(cr);
        break;
    case ICON_EYE:
        cairo_move_to(cr, 2.5, 12); cairo_curve_to(cr, 5.5, 6.5, 18.5, 6.5, 21.5, 12); cairo_curve_to(cr, 18.5, 17.5, 5.5, 17.5, 2.5, 12); cairo_stroke(cr);
        cairo_arc(cr, 12, 12, 3, 0, 2 * G_PI); cairo_stroke(cr);
        break;
    case ICON_EYE_OFF:
        cairo_move_to(cr, 2.5, 12); cairo_curve_to(cr, 5.5, 6.5, 18.5, 6.5, 21.5, 12); cairo_curve_to(cr, 18.5, 17.5, 5.5, 17.5, 2.5, 12); cairo_stroke(cr);
        cairo_arc(cr, 12, 12, 3, 0, 2 * G_PI); cairo_stroke(cr);
        cairo_move_to(cr, 4, 4); cairo_line_to(cr, 20, 20); cairo_stroke(cr);
        break;
    case ICON_USER_CHECK:
        cairo_arc(cr, 10, 8, 3.4, 0, 2 * G_PI); cairo_stroke(cr);
        cairo_move_to(cr, 3, 20); cairo_curve_to(cr, 3, 15, 6, 13.5, 10, 13.5); cairo_curve_to(cr, 12, 13.5, 13.5, 14, 14.6, 14.8); cairo_stroke(cr);
        cairo_move_to(cr, 14.5, 18.5); cairo_line_to(cr, 16.8, 20.8); cairo_line_to(cr, 21, 16); cairo_stroke(cr);
        break;
    case ICON_MAIL:
        rrect(cr, 3, 5.5, 18, 13, 2.5); cairo_stroke(cr);
        cairo_move_to(cr, 3.5, 7.5); cairo_line_to(cr, 12, 13.5); cairo_line_to(cr, 20.5, 7.5); cairo_stroke(cr);
        break;
    case ICON_BUILDING:
        rrect(cr, 5, 4, 14, 17, 1.5); cairo_stroke(cr);
        cairo_move_to(cr, 9, 8); cairo_line_to(cr, 9.01, 8); cairo_move_to(cr, 15, 8); cairo_line_to(cr, 15.01, 8);
        cairo_move_to(cr, 9, 12); cairo_line_to(cr, 9.01, 12); cairo_move_to(cr, 15, 12); cairo_line_to(cr, 15.01, 12);
        cairo_move_to(cr, 10, 21); cairo_line_to(cr, 10, 17); cairo_line_to(cr, 14, 17); cairo_line_to(cr, 14, 21); cairo_stroke(cr);
        break;
    case ICON_FLASK:
        cairo_move_to(cr, 9, 3.5); cairo_line_to(cr, 15, 3.5); cairo_move_to(cr, 10, 3.5); cairo_line_to(cr, 10, 10); cairo_line_to(cr, 4.5, 19);
        cairo_curve_to(cr, 3.8, 20.3, 4.6, 21, 5.8, 21); cairo_line_to(cr, 18.2, 21); cairo_curve_to(cr, 19.4, 21, 20.2, 20.3, 19.5, 19);
        cairo_line_to(cr, 14, 10); cairo_line_to(cr, 14, 3.5); cairo_stroke(cr);
        cairo_move_to(cr, 7.5, 15); cairo_line_to(cr, 16.5, 15); cairo_stroke(cr);
        break;
    case ICON_CODE:
        cairo_move_to(cr, 8.5, 7); cairo_line_to(cr, 3.5, 12); cairo_line_to(cr, 8.5, 17); cairo_move_to(cr, 15.5, 7); cairo_line_to(cr, 20.5, 12); cairo_line_to(cr, 15.5, 17);
        cairo_move_to(cr, 13.5, 5); cairo_line_to(cr, 10.5, 19); cairo_stroke(cr);
        break;
    case ICON_CHAT:
        cairo_move_to(cr, 4, 6); cairo_curve_to(cr, 4, 4.9, 4.9, 4, 6, 4); cairo_line_to(cr, 18, 4); cairo_curve_to(cr, 19.1, 4, 20, 4.9, 20, 6);
        cairo_line_to(cr, 20, 14); cairo_curve_to(cr, 20, 15.1, 19.1, 16, 18, 16); cairo_line_to(cr, 11, 16); cairo_line_to(cr, 7, 20); cairo_line_to(cr, 7, 16);
        cairo_line_to(cr, 6, 16); cairo_curve_to(cr, 4.9, 16, 4, 15.1, 4, 14); cairo_close_path(cr); cairo_stroke(cr);
        break;
    case ICON_HEART:
        cairo_move_to(cr, 12, 20); cairo_curve_to(cr, 3, 14, 3, 7, 7.5, 5.5); cairo_curve_to(cr, 10, 4.7, 11.5, 6, 12, 7.5);
        cairo_curve_to(cr, 12.5, 6, 14, 4.7, 16.5, 5.5); cairo_curve_to(cr, 21, 7, 21, 14, 12, 20); cairo_close_path(cr); cairo_stroke(cr);
        break;
    case ICON_LEAF:
        cairo_move_to(cr, 5, 19); cairo_curve_to(cr, 3, 10, 9, 4, 20, 4); cairo_curve_to(cr, 20, 14, 14, 20, 5, 19); cairo_stroke(cr);
        cairo_move_to(cr, 5, 19); cairo_line_to(cr, 14, 10); cairo_stroke(cr);
        break;
    case ICON_BOOK_OPEN:
        cairo_move_to(cr, 12, 6); cairo_curve_to(cr, 9, 4, 5.5, 4, 3, 5); cairo_line_to(cr, 3, 18.5); cairo_curve_to(cr, 5.5, 17.5, 9, 17.5, 12, 19.5);
        cairo_curve_to(cr, 15, 17.5, 18.5, 17.5, 21, 18.5); cairo_line_to(cr, 21, 5); cairo_curve_to(cr, 18.5, 4, 15, 4, 12, 6); cairo_line_to(cr, 12, 19.5); cairo_stroke(cr);
        break;
    default: break;
    }
}

static gboolean icon_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
    IconData *ic = d;
    double s = ic->size, pad = ic->has_bg ? s * 0.22 : 0, g = (s - 2 * pad) / 24.0;
    (void)w;
    if (ic->has_bg) {
        cairo_set_source_rgb(cr, ic->br, ic->bg, ic->bb);
        rrect(cr, 0, 0, s, s, s * 0.28);
        cairo_fill(cr);
    }
    cairo_save(cr);
    cairo_translate(cr, pad, pad);
    cairo_scale(cr, g, g);
    cairo_set_source_rgb(cr, ic->fr, ic->fg, ic->fb);
    draw_glyph(cr, ic->id, ic->p);
    cairo_restore(cr);
    return FALSE;
}

/* A new icon widget.  fg/bg are "#RRGGBB" (bg may be NULL for no soft background square). */
GtkWidget *ui_icon_new(int id, int size, const char *fg, const char *bg)
{
    IconData *ic = g_new0(IconData, 1);
    GtkWidget *da = gtk_drawing_area_new();
    ic->id = id; ic->size = size; ic->p = 1.0;
    parse_hex(fg ? fg : "#6842F5", &ic->fr, &ic->fg, &ic->fb);
    if (bg) { parse_hex(bg, &ic->br, &ic->bg, &ic->bb); ic->has_bg = 1; }
    gtk_widget_set_size_request(da, size, size);
    gtk_widget_set_halign(da, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(da, GTK_ALIGN_CENTER);
    g_object_set_data_full(G_OBJECT(da), "icon", ic, g_free);
    g_signal_connect(da, "draw", G_CALLBACK(icon_draw), ic);
    return da;
}

/* Changes the colour of an existing icon (e.g. when a navigation item becomes active). */
void ui_icon_set_color(GtkWidget *icon, const char *fg)
{
    IconData *ic = g_object_get_data(G_OBJECT(icon), "icon");
    if (!ic) return;
    parse_hex(fg, &ic->fr, &ic->fg, &ic->fb);
    gtk_widget_queue_draw(icon);
}

static void icon_step(double p, gpointer d) { ((IconData *)d)->p = p; }

/* Plays the icon's small animation once (the end state is the normal icon). */
void ui_icon_animate(GtkWidget *icon)
{
    IconData *ic = g_object_get_data(G_OBJECT(icon), "icon");
    if (ic) ui_animate(icon, "icon", 420, icon_step, ic, NULL);
}

/* Icon as a pixbuf (for entry icons etc.) */
GdkPixbuf *ui_icon_pixbuf(int id, int size, const char *fg)
{
    cairo_surface_t *sf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cairo_t *cr = cairo_create(sf);
    GdkPixbuf *pb;
    double r, g, b;
    parse_hex(fg, &r, &g, &b);
    cairo_scale(cr, size / 24.0, size / 24.0);
    cairo_set_source_rgb(cr, r, g, b);
    draw_glyph(cr, id, 1.0);
    cairo_destroy(cr);
    pb = gdk_pixbuf_get_from_surface(sf, 0, 0, size, size);
    cairo_surface_destroy(sf);
    return pb;
}

/* Round avatar with initials (cached pixbufs are created by the caller). */
GdkPixbuf *ui_avatar_pixbuf(const char *name, int size, const char *bg, const char *fg)
{
    cairo_surface_t *sf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cairo_t *cr = cairo_create(sf);
    cairo_text_extents_t e;
    char ini[4] = "";
    const char *sp;
    GdkPixbuf *pb;
    double r, g, b;
    {   /* skip titles such as "Ms." or "Dr." */
        const char *p = name;
        for (;;) {
            const char *e = strchr(p, ' ');
            if (e && e - p <= 4 && e[-1] == '.') { p = e + 1; while (*p == ' ') p++; } else break;
        }
        ini[0] = (char)toupper((unsigned char)p[0]);
        sp = strrchr(p, ' ');
        if (sp && sp[1]) ini[1] = (char)toupper((unsigned char)sp[1]);
    }
    parse_hex(bg, &r, &g, &b);
    cairo_set_source_rgb(cr, r, g, b);
    cairo_arc(cr, size / 2.0, size / 2.0, size / 2.0, 0, 2 * G_PI);
    cairo_fill(cr);
    parse_hex(fg, &r, &g, &b);
    cairo_set_source_rgb(cr, r, g, b);
    cairo_select_font_face(cr, "Segoe UI", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, size * 0.40);
    cairo_text_extents(cr, ini, &e);
    cairo_move_to(cr, size / 2.0 - e.width / 2 - e.x_bearing, size / 2.0 - e.height / 2 - e.y_bearing);
    cairo_show_text(cr, ini);
    cairo_destroy(cr);
    pb = gdk_pixbuf_get_from_surface(sf, 0, 0, size, size);
    cairo_surface_destroy(sf);
    return pb;
}
