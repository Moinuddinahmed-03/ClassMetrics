/*
 * ui_common.c : shared GTK helpers - styling (CSS), dialogs, tables, combo boxes
 * and the two cairo charts (bar chart and line chart).
 */
#include "ui.h"

/* ================= dialogs ================= */
static void message(GtkMessageType t, const char *msg)
{
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(g_window), GTK_DIALOG_MODAL, t, GTK_BUTTONS_OK, "%s", msg);
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}
void ui_info(const char *msg)  { message(GTK_MESSAGE_INFO, msg); }
void ui_error(const char *msg) { message(GTK_MESSAGE_ERROR, msg); }

gboolean ui_confirm(const char *msg)
{
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(g_window), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
                                          GTK_BUTTONS_YES_NO, "%s", msg);
    int r = gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    return r == GTK_RESPONSE_YES;
}

/* Shows long monospace text (profiles, calculation steps) in a dialog. */
void ui_text_dialog(const char *title, const char *text)
{
    GtkWidget *d = gtk_dialog_new_with_buttons(title, GTK_WINDOW(g_window), GTK_DIALOG_MODAL, "Close", GTK_RESPONSE_CLOSE, NULL);
    GtkTextView *tv;
    GtkWidget *sw = ui_textview_new(&tv, 420);
    gtk_window_set_default_size(GTK_WINDOW(d), 720, 560);
    ui_set_text(tv, text);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(d))), sw, TRUE, TRUE, 6);
    gtk_widget_show_all(d);
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}

/* Small calendar dialog that writes YYYY-MM-DD into an entry. */
void ui_pick_date(GtkWindow *parent, GtkEntry *entry)
{
    GtkWidget *d = gtk_dialog_new_with_buttons("Select date", parent, GTK_DIALOG_MODAL,
                                               "Cancel", GTK_RESPONSE_CANCEL, "Select", GTK_RESPONSE_OK, NULL);
    GtkWidget *cal = gtk_calendar_new();
    guint y, m, day;
    int yy, mm, dd;
    const char *cur = gtk_entry_get_text(entry);
    if (valid_date(cur) && sscanf(cur, "%d-%d-%d", &yy, &mm, &dd) == 3) {
        gtk_calendar_select_month(GTK_CALENDAR(cal), mm - 1, yy);
        gtk_calendar_select_day(GTK_CALENDAR(cal), dd);
    }
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(d))), cal, TRUE, TRUE, 8);
    gtk_widget_show_all(d);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK) {
        char buf[16];
        gtk_calendar_get_date(GTK_CALENDAR(cal), &y, &m, &day);
        snprintf(buf, sizeof buf, "%04u-%02u-%02u", y, m + 1, day);
        gtk_entry_set_text(entry, buf);
    }
    gtk_widget_destroy(d);
}

/* ================= small widget builders ================= */
GtkWidget *ui_label(const char *text, const char *css_class)
{
    GtkWidget *l = gtk_label_new(text);
    gtk_widget_set_halign(l, GTK_ALIGN_START);
    if (css_class) gtk_style_context_add_class(gtk_widget_get_style_context(l), css_class);
    /* long descriptions wrap instead of widening the page (no horizontal overflow) */
    if (css_class && (strcmp(css_class, "hint") == 0 || strcmp(css_class, "page-sub") == 0)) {
        gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
        gtk_label_set_xalign(GTK_LABEL(l), 0);
    }
    return l;
}

GtkWidget *ui_title(const char *title, const char *subtitle)
{
    GtkWidget *b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(b), ui_label(title, "page-title"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(b), ui_label(subtitle, "page-sub"), FALSE, FALSE, 0);
    return b;
}

GtkWidget *ui_button(const char *text, const char *css_class, GCallback cb, gpointer data)
{
    GtkWidget *b = gtk_button_new_with_label(text);
    if (css_class) gtk_style_context_add_class(gtk_widget_get_style_context(b), css_class);
    if (cb) g_signal_connect(b, "clicked", cb, data);
    return b;
}

/* White rounded card; *inner_box receives the vertical box to fill. */
GtkWidget *ui_card(GtkWidget **inner_box)
{
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(outer), "card");
    gtk_box_pack_start(GTK_BOX(outer), inner, TRUE, TRUE, 0);
    if (inner_box) *inner_box = inner;
    return outer;
}

GtkWidget *ui_labeled(const char *label, GtkWidget *w)
{
    GtkWidget *b = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(b), ui_label(label, "field-label"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(b), w, FALSE, FALSE, 0);
    return b;
}

/* Every page is wrapped in a scrolled window so it can always be scrolled. */
GtkWidget *ui_page_wrap(GtkWidget *content)
{
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_margin_start(content, 20);
    gtk_widget_set_margin_end(content, 20);
    gtk_widget_set_margin_top(content, 20);
    gtk_widget_set_margin_bottom(content, 24);
    gtk_container_add(GTK_CONTAINER(sw), content);
    return sw;
}

/* ================= tables (GtkTreeView) ================= */
/* Store layout: ncols text columns, then a colour column, then an int index. */
GtkListStore *ui_store_new(int ncols)
{
    GType types[24];
    int i;
    for (i = 0; i < ncols + 1; i++) types[i] = G_TYPE_STRING;
    types[ncols + 1] = G_TYPE_INT;
    return gtk_list_store_newv(ncols + 2, types);
}

/* Alternating row background (not on the selected row, so the selection colour stays visible). */
static void zebra_cb(GtkTreeViewColumn *col, GtkCellRenderer *cell, GtkTreeModel *model, GtkTreeIter *it, gpointer data)
{
    GtkTreeView *tv = GTK_TREE_VIEW(data);
    GtkTreePath *p = gtk_tree_model_get_path(model, it);
    int odd = gtk_tree_path_get_indices(p)[0] % 2;
    gboolean sel = gtk_tree_selection_iter_is_selected(gtk_tree_view_get_selection(tv), it);
    (void)col;
    if (odd && !sel) g_object_set(cell, "cell-background", "#F5F6FB", NULL);
    else g_object_set(cell, "cell-background-set", FALSE, NULL);
    gtk_tree_path_free(p);
}

GtkWidget *ui_tree_new(GtkListStore *st, const char **titles, int ncols, GtkTreeView **tv_out, int min_h)
{
    GtkWidget *tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(st));
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    int i;
    for (i = 0; i < ncols; i++) {
        GtkCellRenderer *r = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(titles[i], r, "text", i,
                                                                        "foreground", ncols, NULL);
        gtk_tree_view_column_set_resizable(c, TRUE);
        gtk_tree_view_column_set_min_width(c, 60);
        gtk_tree_view_column_set_cell_data_func(c, r, zebra_cb, tv, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tv), c);
    }
    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(tv), GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(sw), min_h);
    gtk_container_add(GTK_CONTAINER(sw), tv);
    if (tv_out) *tv_out = GTK_TREE_VIEW(tv);
    return sw;
}

void ui_store_add(GtkListStore *st, int ncols, const char *color, int index, ...)
{
    GtkTreeIter it;
    va_list ap;
    int i;
    gtk_list_store_append(st, &it);
    va_start(ap, index);
    for (i = 0; i < ncols; i++) gtk_list_store_set(st, &it, i, va_arg(ap, const char *), -1);
    va_end(ap);
    gtk_list_store_set(st, &it, ncols, color ? color : "#1F2937", ncols + 1, index, -1);
}

int ui_tree_selected(GtkTreeView *tv)
{
    GtkTreeModel *model;
    GtkTreeIter it;
    int idx = -1;
    if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection(tv), &model, &it)) return -1;
    gtk_tree_model_get(model, &it, gtk_tree_model_get_n_columns(model) - 1, &idx, -1);
    return idx;
}

/* ================= text view ================= */
GtkWidget *ui_textview_new(GtkTextView **tv_out, int min_h)
{
    GtkWidget *tv = gtk_text_view_new();
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tv), FALSE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv), 10);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tv), 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(tv), "mono");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(sw), min_h);
    gtk_container_add(GTK_CONTAINER(sw), tv);
    if (tv_out) *tv_out = GTK_TEXT_VIEW(tv);
    return sw;
}

void ui_set_text(GtkTextView *tv, const char *text)
{
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(tv), text, -1);
}

/* ================= combo boxes (keep the selection when refreshed) ================= */
/* Long entries are cut with "..." instead of making the combo (and the page) wider. */
static void compact_combo(GtkComboBoxText *c)
{
    GList *cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(c));
    if (cells && !g_object_get_data(G_OBJECT(c), "compact")) {
        g_object_set(cells->data, "ellipsize", PANGO_ELLIPSIZE_END, "width-chars", 16, NULL);
        g_object_set_data(G_OBJECT(c), "compact", GINT_TO_POINTER(1));
    }
    g_list_free(cells);
}

static int keep_or_first(GtkComboBoxText *c, int n)
{
    int act = gtk_combo_box_get_active(GTK_COMBO_BOX(c));
    return (act >= 0 && act < n) ? act : (n > 0 ? 0 : -1);
}

void ui_combo_students(GtkComboBoxText *c)
{
    compact_combo(c);
    char t[160];
    int i, act = keep_or_first(c, n_students);
    gtk_combo_box_text_remove_all(c);
    for (i = 0; i < n_students; i++) {
        snprintf(t, sizeof t, "%s - %s", students[i].roll, students[i].name);
        gtk_combo_box_text_append_text(c, t);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(c), act);
}

void ui_combo_subjects(GtkComboBoxText *c, int with_all)
{
    compact_combo(c);
    char t[160];
    int i, off = with_all ? 1 : 0, act = keep_or_first(c, n_subjects + off);
    gtk_combo_box_text_remove_all(c);
    if (with_all) gtk_combo_box_text_append_text(c, "All subjects");
    for (i = 0; i < n_subjects; i++) {
        snprintf(t, sizeof t, "%s - %s", subjects[i].code, subjects[i].name);
        gtk_combo_box_text_append_text(c, t);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(c), act);
}

void ui_combo_exams(GtkComboBoxText *c, int default_latest)
{
    compact_combo(c);
    int i, act = gtk_combo_box_get_active(GTK_COMBO_BOX(c));
    if (act < 0 || act >= n_exams) act = (default_latest && latest_exam() >= 0) ? latest_exam() : (n_exams ? 0 : -1);
    gtk_combo_box_text_remove_all(c);
    for (i = 0; i < n_exams; i++) gtk_combo_box_text_append_text(c, exams[i].name);
    gtk_combo_box_set_active(GTK_COMBO_BOX(c), act);
}

/* ================= charts drawn with cairo (ClassMetrics 2.0) =================
 * Rounded bars, soft grid, axis labels, accurate reference line, line chart with markers.
 * ui_chart_progress (0..1) is the entrance animation of the chart that is being drawn:
 * bars grow, the line is revealed.  Value labels always show the real values. */
static double ui_chart_progress = 1.0;

static void set_rgb(cairo_t *cr, unsigned hex)
{
    cairo_set_source_rgb(cr, ((hex >> 16) & 255) / 255.0, ((hex >> 8) & 255) / 255.0, (hex & 255) / 255.0);
}

static void text_at(cairo_t *cr, const char *s, double x, double y, int center, double size)
{
    cairo_text_extents_t e;
    cairo_select_font_face(cr, "Segoe UI", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cairo_text_extents(cr, s, &e);
    cairo_move_to(cr, center ? x - e.width / 2 - e.x_bearing : x, y);
    cairo_show_text(cr, s);
}

/* Bar with rounded top corners. */
static void top_rounded(cairo_t *cr, double x, double y, double w, double h, double r)
{
    if (h < 1) { cairo_rectangle(cr, x, y, w, h); return; }
    if (r > h) r = h;
    if (r > w / 2) r = w / 2;
    cairo_new_sub_path(cr);
    cairo_move_to(cr, x, y + h);
    cairo_line_to(cr, x, y + r);
    cairo_arc(cr, x + r, y + r, r, G_PI, 1.5 * G_PI);
    cairo_line_to(cr, x + w - r, y);
    cairo_arc(cr, x + w - r, y + r, r, -G_PI / 2, 0);
    cairo_line_to(cr, x + w, y + h);
    cairo_close_path(cr);
}

/* Shared frame: title, 0..100 axis with soft grid lines and a "%" axis label.  Returns the plot area. */
static void frame(cairo_t *cr, int w, int h, const char *title, double *px, double *py, double *pw, double *ph)
{
    int g;
    char t[8];
    *px = 40; *py = 34; *pw = w - 58; *ph = h - 34 - 38;
    cairo_set_source_rgba(cr, 1, 1, 1, 0);                      /* the card behind stays white */
    set_rgb(cr, 0x64748B);
    text_at(cr, title, 4, 16, 0, 12);
    for (g = 0; g <= 4; g++) {
        double y = *py + *ph - *ph * g / 4.0;
        set_rgb(cr, g == 0 ? 0xCBD5E1 : 0xEEF2F7);
        cairo_set_line_width(cr, 1);
        cairo_move_to(cr, *px, y + 0.5);
        cairo_line_to(cr, *px + *pw, y + 0.5);
        cairo_stroke(cr);
        set_rgb(cr, 0x94A3B8);
        snprintf(t, sizeof t, "%d", g * 25);
        text_at(cr, t, *px - 8 - 6.0 * (int)strlen(t), y + 4, 0, 11);
    }
    cairo_save(cr);                                              /* rotated axis label */
    cairo_translate(cr, 11, *py + *ph / 2 + 8);
    cairo_rotate(cr, -G_PI / 2);
    set_rgb(cr, 0x94A3B8);
    text_at(cr, "Percent (%)", 0, 0, 0, 10.5);
    cairo_restore(cr);
}

void ui_draw_bars(cairo_t *cr, int w, int h, const char **labels, const double *val, int n, double thr, const char *title)
{
    double px, py, pw, ph, slot, bw, x, bh, p = ui_chart_progress;
    int i;
    char t[16];
    frame(cr, w, h, title, &px, &py, &pw, &ph);
    if (n == 0) { set_rgb(cr, 0x94A3B8); text_at(cr, "No data yet", w / 2.0, h / 2.0, 1, 13); return; }
    slot = pw / n;
    bw = slot * 0.52 > 44 ? 44 : slot * 0.52;
    for (i = 0; i < n; i++) {
        double v = val[i] < 0 ? 0 : val[i] > 100 ? 100 : val[i];
        x = px + slot * i + (slot - bw) / 2;
        bh = ph * v / 100.0 * p;
        if (val[i] >= thr) set_rgb(cr, 0x16A34A); else set_rgb(cr, 0xDC2626);
        top_rounded(cr, x, py + ph - bh, bw, bh, 7);
        cairo_fill(cr);
        if (p > 0.8) {                                           /* real value above the bar */
            set_rgb(cr, 0x17213D);
            snprintf(t, sizeof t, "%.0f%%", val[i]);
            text_at(cr, t, x + bw / 2, py + ph - bh - 5, 1, 11);
        }
        set_rgb(cr, 0x475569);
        text_at(cr, labels[i], px + slot * i + slot / 2, py + ph + 17, 1, 11);
    }
    if (thr > 0) {                                               /* reference line at the exact threshold */
        double dash[2] = { 6, 4 }, y = py + ph - ph * thr / 100.0;
        set_rgb(cr, 0x6842F5);
        cairo_set_line_width(cr, 1.5);
        cairo_set_dash(cr, dash, 2, 0);
        cairo_move_to(cr, px, y + 0.5); cairo_line_to(cr, px + pw, y + 0.5);
        cairo_stroke(cr);
        cairo_set_dash(cr, NULL, 0, 0);
        snprintf(t, sizeof t, "%.0f%%", thr);
        text_at(cr, t, px + pw - 26, y - 4, 0, 10.5);
    }
}

void ui_draw_line(cairo_t *cr, int w, int h, const char **labels, const double *val, int n, const char *title)
{
    double px, py, pw, ph, x, y, p = ui_chart_progress, reveal;
    int i;
    char t[16];
    frame(cr, w, h, title, &px, &py, &pw, &ph);
    if (n == 0) { set_rgb(cr, 0x94A3B8); text_at(cr, "No data yet", w / 2.0, h / 2.0, 1, 13); return; }
    reveal = px + pw * p + 6;                                    /* the line is revealed from left to right */
    cairo_save(cr);
    cairo_rectangle(cr, 0, 0, reveal, h);
    cairo_clip(cr);
    for (i = 0; i < n; i++) {                                    /* soft area under the line */
        x = px + (n == 1 ? pw / 2 : pw * (0.08 + 0.84 * i / (n - 1)));
        y = py + ph - ph * (val[i] < 0 ? 0 : val[i] > 100 ? 100 : val[i]) / 100.0;
        if (i == 0) { cairo_move_to(cr, x, py + ph); cairo_line_to(cr, x, y); } else cairo_line_to(cr, x, y);
        if (i == n - 1) { cairo_line_to(cr, x, py + ph); cairo_close_path(cr); }
    }
    cairo_set_source_rgba(cr, 0.408, 0.259, 0.961, 0.10);
    cairo_fill(cr);
    set_rgb(cr, 0x6842F5);
    cairo_set_line_width(cr, 3);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    for (i = 0; i < n; i++) {
        x = px + (n == 1 ? pw / 2 : pw * (0.08 + 0.84 * i / (n - 1)));
        y = py + ph - ph * (val[i] < 0 ? 0 : val[i] > 100 ? 100 : val[i]) / 100.0;
        if (i == 0) cairo_move_to(cr, x, y); else cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);
    for (i = 0; i < n; i++) {
        x = px + (n == 1 ? pw / 2 : pw * (0.08 + 0.84 * i / (n - 1)));
        y = py + ph - ph * (val[i] < 0 ? 0 : val[i] > 100 ? 100 : val[i]) / 100.0;
        set_rgb(cr, 0xFFFFFF);
        cairo_arc(cr, x, y, 6, 0, 2 * G_PI); cairo_fill(cr);
        set_rgb(cr, 0x6842F5);
        cairo_set_line_width(cr, 2.5);
        cairo_arc(cr, x, y, 5, 0, 2 * G_PI); cairo_stroke(cr);
        set_rgb(cr, 0x17213D);
        snprintf(t, sizeof t, "%.1f", val[i]);
        text_at(cr, t, x, y - 12, 1, 11);
    }
    cairo_restore(cr);
    for (i = 0; i < n; i++) {
        x = px + (n == 1 ? pw / 2 : pw * (0.08 + 0.84 * i / (n - 1)));
        set_rgb(cr, 0x475569);
        text_at(cr, labels[i], x, py + ph + 17, 1, 11);
    }
}

/* Entrance animation of a chart: progress 0 -> 1 in 480 ms every time the chart is shown. */
static void chart_step(double p, gpointer d) { *(double *)d = p; }

static gboolean chart_pre_draw(GtkWidget *w, cairo_t *cr, gpointer d)
{
    double *p = g_object_get_data(G_OBJECT(w), "chart-p");
    (void)cr; (void)d;
    ui_chart_progress = p ? *p : 1.0;
    return FALSE;                                                /* continue with the page's own draw handler */
}

static void chart_map(GtkWidget *w, gpointer d)
{
    double *p = g_object_get_data(G_OBJECT(w), "chart-p");
    (void)d;
    if (p) ui_animate(w, "chart", 480, chart_step, p, NULL);
}

/* Adds the entrance animation to any drawing area whose draw handler calls ui_draw_bars/ui_draw_line.
 * Call it BEFORE connecting the draw handler. */
void ui_chart_attach_anim(GtkWidget *da)
{
    double *p = g_new(double, 1);
    *p = 1.0;
    g_object_set_data_full(G_OBJECT(da), "chart-p", p, g_free);
    g_signal_connect(da, "draw", G_CALLBACK(chart_pre_draw), NULL);
    g_signal_connect(da, "map", G_CALLBACK(chart_map), NULL);
}

GtkWidget *ui_chart_area(GCallback draw_cb, int w, int h)
{
    GtkWidget *da = gtk_drawing_area_new();
    gtk_widget_set_size_request(da, w, h);
    gtk_widget_set_hexpand(da, TRUE);
    ui_chart_attach_anim(da);
    g_signal_connect(da, "draw", draw_cb, NULL);
    return da;
}

/* ================= file dialogs =================
 * On Windows the NATIVE Open/Save dialog (comdlg32) is used.  The GTK file
 * chooser fails on OneDrive-managed folders (Desktop, Downloads) with an
 * input/output error, and its sidebar text was unreadable on the dark theme.
 * OFN_NOCHANGEDIR keeps the program's working folder (and so the data/ folder)
 * unchanged.  Returns a newly allocated path (free with g_free) or NULL.
 */
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <gdk/gdkwin32.h>

char *ui_choose_file(int save, const char *title, const char *default_name)
{
    OPENFILENAMEA ofn;
    char path[MAX_PATH] = "";
    if (default_name) strncpy(path, default_name, MAX_PATH - 1);
    memset(&ofn, 0, sizeof ofn);
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = (g_window && gtk_widget_get_window(g_window)) ? GDK_WINDOW_HWND(gtk_widget_get_window(g_window)) : NULL;
    ofn.lpstrFilter = "CSV files (*.csv)\0*.csv\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = "csv";
    ofn.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
                (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (!(save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn))) return NULL;   /* cancelled */
    return g_strdup(path);
}
#else
char *ui_choose_file(int save, const char *title, const char *default_name)
{
    GtkWidget *dlg = gtk_file_chooser_dialog_new(title, GTK_WINDOW(g_window),
        save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL, save ? "Save" : "Open", GTK_RESPONSE_ACCEPT, NULL);
    char *r = NULL;
    if (save && default_name) gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), default_name);
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) r = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
    gtk_widget_destroy(dlg);
    return r;
}
#endif

/* Examination combo box with an optional first item "All examinations (total)".
 * The selection is kept when the list is refilled. */
void ui_combo_exams_ex(GtkComboBoxText *c, int with_all)
{
    compact_combo(c);
    int i, off = with_all ? 1 : 0, act = gtk_combo_box_get_active(GTK_COMBO_BOX(c));
    if (act < 0 || act >= n_exams + off) act = (with_all || n_exams > 0) ? 0 : -1;
    gtk_combo_box_text_remove_all(c);
    if (with_all) gtk_combo_box_text_append_text(c, "All examinations (total)");
    for (i = 0; i < n_exams; i++) gtk_combo_box_text_append_text(c, exams[i].name);
    gtk_combo_box_set_active(GTK_COMBO_BOX(c), act);
}

/* Examination chosen in such a combo: EXAM_ALL (-1) for "all", -2 when nothing is selected. */
int ui_exam_sel(GtkWidget *combo, int with_all)
{
    int a = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    if (a < 0) return with_all ? -2 : -1;
    return with_all ? a - 1 : a;
}

/* ================= trees with per-cell colours (used by the Marks sheet and View Student) =================
 * Store layout: ncols text columns, ncols foreground columns, ncols background columns,
 * then one tooltip text column and one integer key column (last).  An empty background
 * means "none".  The background is not applied to the selected row, so the selection stays readable. */
GtkListStore *ui_cell_store_new(int ncols)
{
    GType types[64];
    int i;
    for (i = 0; i < 3 * ncols + 1; i++) types[i] = G_TYPE_STRING;
    types[3 * ncols + 1] = G_TYPE_INT;
    return gtk_list_store_newv(3 * ncols + 2, types);
}

void ui_cell_store_add(GtkListStore *st, int ncols, const char **cells, const char **fg, const char **bg,
                       const char *tip, int key)
{
    GtkTreeIter it;
    int i;
    gtk_list_store_append(st, &it);
    for (i = 0; i < ncols; i++) {
        gtk_list_store_set(st, &it, i, cells[i], ncols + i, (fg && fg[i]) ? fg[i] : "#1F2937",
                           2 * ncols + i, (bg && bg[i]) ? bg[i] : "", -1);
    }
    gtk_list_store_set(st, &it, 3 * ncols, tip ? tip : "", 3 * ncols + 1, key, -1);
}

typedef struct { GtkTreeView *tv; int col; } BgData;

static void cellbg_cb(GtkTreeViewColumn *col, GtkCellRenderer *cell, GtkTreeModel *model, GtkTreeIter *it, gpointer data)
{
    BgData *d = data;
    gchar *bg = NULL;
    gboolean sel = gtk_tree_selection_iter_is_selected(gtk_tree_view_get_selection(d->tv), it);
    (void)col;
    gtk_tree_model_get(model, it, d->col, &bg, -1);
    if (bg && bg[0] && !sel) g_object_set(cell, "cell-background", bg, NULL);
    else g_object_set(cell, "cell-background-set", FALSE, NULL);
    g_free(bg);
}

/* The tree itself (NOT inside a scrolled window).  xalign: 0 left, 0.5 centre, 1 right (may be NULL). */
GtkWidget *ui_cell_tree_new(GtkListStore *st, const char **titles, int ncols, const float *xalign, int min_w)
{
    GtkWidget *tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(st));
    int i;
    for (i = 0; i < ncols; i++) {
        GtkCellRenderer *r = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(titles[i], r, "text", i, "foreground", ncols + i, NULL);
        BgData *d = g_malloc(sizeof *d);
        d->tv = GTK_TREE_VIEW(tv); d->col = 2 * ncols + i;
        g_object_set_data_full(G_OBJECT(c), "bgdata", d, g_free);
        gtk_tree_view_column_set_cell_data_func(c, r, cellbg_cb, d, NULL);
        if (xalign) { g_object_set(r, "xalign", xalign[i], NULL); gtk_tree_view_column_set_alignment(c, xalign[i]); }
        gtk_tree_view_column_set_expand(c, TRUE);               /* columns share the available width: no overflow */
        gtk_tree_view_column_set_min_width(c, min_w);
        gtk_tree_view_column_set_resizable(c, TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tv), c);
    }
    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(tv), GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
    gtk_widget_set_has_tooltip(tv, TRUE);
    return tv;
}

/* ================= 2.0 widgets: icon buttons and badges ================= */

/* Button with a vector icon and a text label.  The icon colour follows the button style. */
GtkWidget *ui_icon_button(int icon, const char *text, const char *css_class, GCallback cb, gpointer data)
{
    GtkWidget *b = gtk_button_new(), *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7), *ic, *l = gtk_label_new(text);
    const char *col = "#17213D";
    if (css_class && strcmp(css_class, "accent") == 0) col = "#FFFFFF";
    else if (css_class && strcmp(css_class, "danger") == 0) col = "#DC2626";
    ic = ui_icon_new(icon, 17, col, NULL);
    gtk_box_pack_start(GTK_BOX(box), ic, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), l, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(b), box);
    if (css_class) gtk_style_context_add_class(gtk_widget_get_style_context(b), css_class);
    if (cb) g_signal_connect(b, "clicked", cb, data);
    g_object_set_data(G_OBJECT(b), "icon", ic);
    return b;
}

/* Small rounded status pill: css_class is badge-good / badge-bad / badge-warn / badge-neutral. */
GtkWidget *ui_badge(const char *text, const char *css_class)
{
    GtkWidget *l = gtk_label_new(text);
    gtk_style_context_add_class(gtk_widget_get_style_context(l), css_class);
    gtk_widget_set_halign(l, GTK_ALIGN_START);
    gtk_widget_set_valign(l, GTK_ALIGN_CENTER);
    return l;
}

/* Motion preference: the page crossfade follows settings.animations (called from Profile). */
void ui_apply_motion_setting(void)
{
    extern void app_apply_motion(void);
    app_apply_motion();
}

/* ================= table with a round initials avatar in the first column =================
 * Store: 3 x ncols strings (text, foreground, background), tooltip string, avatar pixbuf, int key (last). */
GtkListStore *ui_avatar_store_new(int ncols)
{
    GType types[64];
    int i;
    for (i = 0; i < 3 * ncols + 1; i++) types[i] = G_TYPE_STRING;
    types[3 * ncols + 1] = GDK_TYPE_PIXBUF;
    types[3 * ncols + 2] = G_TYPE_INT;
    return gtk_list_store_newv(3 * ncols + 3, types);
}

void ui_avatar_store_add(GtkListStore *st, int ncols, const char **cells, const char **fg, const char **bg, GdkPixbuf *av, int key)
{
    GtkTreeIter it;
    int i;
    gtk_list_store_append(st, &it);
    for (i = 0; i < ncols; i++)
        gtk_list_store_set(st, &it, i, cells[i], ncols + i, (fg && fg[i]) ? fg[i] : "#17213D", 2 * ncols + i, (bg && bg[i]) ? bg[i] : "", -1);
    gtk_list_store_set(st, &it, 3 * ncols, "", 3 * ncols + 1, av, 3 * ncols + 2, key, -1);
}

GtkWidget *ui_avatar_tree_new(GtkListStore *st, const char **titles, int ncols, const float *xalign, int min_w)
{
    GtkWidget *tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(st));
    GtkCellRenderer *pr = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn *pc = gtk_tree_view_column_new_with_attributes("", pr, "pixbuf", 3 * ncols + 1, NULL);
    BgData *pd = g_malloc(sizeof *pd);
    int i;
    pd->tv = GTK_TREE_VIEW(tv); pd->col = 2 * ncols;
    g_object_set_data_full(G_OBJECT(pc), "bgdata", pd, g_free);
    gtk_tree_view_column_set_cell_data_func(pc, pr, cellbg_cb, pd, NULL);
    g_object_set(pr, "xpad", 8, "ypad", 3, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tv), pc);
    for (i = 0; i < ncols; i++) {
        GtkCellRenderer *r = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(titles[i], r, "text", i, "foreground", ncols + i, NULL);
        BgData *d = g_malloc(sizeof *d);
        d->tv = GTK_TREE_VIEW(tv); d->col = 2 * ncols + i;
        g_object_set_data_full(G_OBJECT(c), "bgdata", d, g_free);
        gtk_tree_view_column_set_cell_data_func(c, r, cellbg_cb, d, NULL);
        if (xalign) { g_object_set(r, "xalign", xalign[i], NULL); gtk_tree_view_column_set_alignment(c, xalign[i]); }
        gtk_tree_view_column_set_expand(c, i == 1);
        gtk_tree_view_column_set_min_width(c, min_w);
        gtk_tree_view_column_set_resizable(c, TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tv), c);
    }
    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(tv), GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
    return tv;
}

/* Screenshot aid: CLASSMETRICS_TAB=n opens tab n of a notebook once the window is shown. */
static gboolean shot_tab_idle(gpointer nb)
{
    const char *t = getenv("CLASSMETRICS_TAB");
    if (t) gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), atoi(t));
    return G_SOURCE_REMOVE;
}

void ui_shot_tab(GtkWidget *nb)
{
    if (getenv("CLASSMETRICS_TAB")) g_idle_add(shot_tab_idle, nb);
}
