/*
 * ui_theme.c : ClassMetrics 2.0 design system (colours, typography, components) as GTK CSS.
 *
 * Palette   primary purple #6842F5 | navy #14213D | lavender #EDE9FE | background #F3F4F9 | card #FFFFFF
 *           text #17213D / #64748B | success #16A34A | error #DC2626 | warning #F59E0B | border #E2E8F0
 * Typography  Inter (if installed) -> Segoe UI -> Sans.  Sizes: brand 24, page title 27, section 17,
 *           stat value 30, labels 13, table 13.5, buttons 13.5, hints 12.5.
 * All colours / sizes live here so every page looks the same.
 */
#include "ui.h"

static const char *CSS =
    /* ---------- base ---------- */
    "* { font-family: 'Inter', 'Segoe UI', 'Sans'; }"
    "window { background-color: #F3F4F9; color: #17213D; font-size: 13.5px; }"
    "label { color: #17213D; }"
    "tooltip { background-color: #14213D; border-radius: 8px; }"
    "tooltip label { color: #FFFFFF; font-size: 12.5px; padding: 2px 4px; }"
    /* ---------- sidebar ---------- */
    ".sidebar { background-color: #14213D; }"
    ".sidebar label { color: #C7D0E6; }"
    ".nav-item { background-image: none; background-color: transparent; border: none; border-radius: 12px;"
    "   padding: 9px 12px; box-shadow: none; transition: all 180ms ease-in-out; }"
    ".nav-item label { color: #C7D0E6; font-size: 14px; font-weight: 500; }"
    ".nav-item:hover { background-color: rgba(255,255,255,0.08); }"
    ".nav-item.active { background-color: #6842F5; box-shadow: 0 6px 16px rgba(104,66,245,0.45); }"
    ".nav-item.active label { color: #FFFFFF; font-weight: 600; }"
    ".brand { color: #FFFFFF; font-size: 22px; font-weight: 700; }"
    ".brand-sub { color: #9FB0D8; font-size: 12px; }"
    ".side-user { color: #FFFFFF; font-size: 13px; font-weight: 600; }"
    ".side-sub { color: #9FB0D8; font-size: 11.5px; }"
    ".side-profile { background-color: rgba(255,255,255,0.06); border-radius: 14px; padding: 10px; }"
    ".logout-btn { background-image: none; background-color: rgba(255,255,255,0.08); color: #FFFFFF; border: none;"
    "   border-radius: 10px; padding: 8px 12px; box-shadow: none; transition: all 180ms ease-in-out; }"
    ".logout-btn label { color: #FFFFFF; font-weight: 500; }"
    ".logout-btn:hover { background-color: #DC2626; }"
    /* ---------- typography ---------- */
    ".page-title { font-size: 27px; font-weight: 700; color: #17213D; }"
    ".page-sub { font-size: 13.5px; color: #64748B; }"
    ".card-head { font-size: 17px; font-weight: 600; color: #17213D; }"
    ".card-label { font-size: 13px; font-weight: 500; color: #64748B; }"
    ".card-value { font-size: 30px; font-weight: 700; color: #17213D; }"
    ".card-value-sm { font-size: 20px; }"
    ".hint { font-size: 12.5px; color: #64748B; }"
    ".field-label { font-size: 12.5px; font-weight: 500; color: #64748B; }"
    ".section-label { font-size: 12px; font-weight: 600; color: #64748B; letter-spacing: 1px; }"
    ".card-icon { color: #6842F5; font-size: 16px; }"
    /* ---------- cards ---------- */
    ".card { background-color: #FFFFFF; border-radius: 16px; border: 1px solid #E2E8F0; padding: 16px;"
    "   box-shadow: 0 1px 3px rgba(20,33,61,0.06), 0 6px 16px rgba(20,33,61,0.04); }"
    ".stat-card { transition: all 200ms ease-in-out; }"
    ".stat-card:hover { box-shadow: 0 10px 26px rgba(104,66,245,0.16); border-color: #C4B5FD; }"
    ".hero-card { background-image: linear-gradient(135deg, #5B34E6, #8B5CF6); border-radius: 18px; padding: 22px 24px;"
    "   box-shadow: 0 10px 28px rgba(104,66,245,0.30); }"
    ".hero-title { color: #FFFFFF; font-size: 26px; font-weight: 700; }"
    ".hero-sub { color: rgba(255,255,255,0.88); font-size: 13.5px; }"
    ".hero-chip { background-color: rgba(255,255,255,0.18); border-radius: 999px; padding: 4px 12px; }"
    ".hero-chip label { color: #FFFFFF; font-size: 12.5px; font-weight: 500; }"
    ".rank-row { background-color: #F8FAFC; border-radius: 12px; padding: 8px 12px; border: 1px solid #EEF2F7;"
    "   transition: all 160ms ease-in-out; }"
    ".rank-row:hover { background-color: #F5F3FF; border-color: #DDD6FE; }"
    ".rank-badge { background-color: #EDE9FE; color: #5B34D6; font-weight: 700; border-radius: 999px; padding: 2px 9px; font-size: 12.5px; }"
    ".rank-badge-1 { background-color: #FEF3C7; color: #B45309; }"
    ".unit-row { background-color: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 12px; padding: 9px 12px; }"
    ".unit-num { background-color: #6842F5; color: #FFFFFF; font-weight: 700; border-radius: 15px; min-width: 30px; min-height: 30px; }"
    ".subject-card { transition: all 200ms ease-in-out; border-width: 1px; }"
    ".subject-card:hover { box-shadow: 0 10px 26px rgba(104,66,245,0.16); }"
    "flowboxchild:selected .subject-card { border: 2px solid #6842F5; background-color: #FAF8FF; }"
    "flowboxchild { background: transparent; padding: 6px; }"
    "flowboxchild:selected { background: transparent; }"
    ".info-card-title { font-size: 12px; font-weight: 600; color: #64748B; }"
    ".info-card-value { font-size: 15px; font-weight: 600; color: #17213D; }"
    /* ---------- badges ---------- */
    ".badge-good { background-color: #DCFCE7; color: #15803D; border-radius: 999px; padding: 2px 10px; font-weight: 600; font-size: 12px; }"
    ".badge-bad { background-color: #FEE2E2; color: #B91C1C; border-radius: 999px; padding: 2px 10px; font-weight: 600; font-size: 12px; }"
    ".badge-warn { background-color: #FEF3C7; color: #B45309; border-radius: 999px; padding: 2px 10px; font-weight: 600; font-size: 12px; }"
    ".badge-neutral { background-color: #EDE9FE; color: #5B34D6; border-radius: 999px; padding: 2px 10px; font-weight: 600; font-size: 12px; }"
    ".counter { border-radius: 12px; padding: 8px 14px; background-color: #F8FAFC; border: 1px solid #E2E8F0; }"
    ".counter-good { background-color: #F0FDF4; border-color: #BBF7D0; }"
    ".counter-bad { background-color: #FEF2F2; border-color: #FECACA; }"
    ".counter-value { font-size: 20px; font-weight: 700; }"
    /* ---------- buttons ---------- */
    "button { border-radius: 10px; padding: 7px 14px; font-size: 13.5px; font-weight: 500; background-image: none;"
    "   background-color: #FFFFFF; color: #17213D; border: 1px solid #E2E8F0; box-shadow: 0 1px 2px rgba(20,33,61,0.06);"
    "   transition: all 160ms ease-in-out; }"
    "button label { color: inherit; }"
    "button:hover { background-color: #F5F3FF; border-color: #C4B5FD; }"
    "button:active { background-color: #EDE9FE; }"
    "button.accent { background-color: #6842F5; color: #FFFFFF; border-color: #6842F5; box-shadow: 0 4px 12px rgba(104,66,245,0.30); }"
    "button.accent label { color: #FFFFFF; font-weight: 600; }"
    "button.accent:hover { background-color: #5933E0; border-color: #5933E0; box-shadow: 0 6px 16px rgba(104,66,245,0.40); }"
    "button.danger { color: #DC2626; border-color: #FECACA; background-color: #FFFFFF; }"
    "button.danger label { color: #DC2626; }"
    "button.danger:hover { background-color: #FEF2F2; border-color: #DC2626; }"
    "button.success-flash { background-color: #16A34A; border-color: #16A34A; }"
    /* ---------- inputs ---------- */
    "entry, spinbutton { border-radius: 10px; padding: 7px 10px; background-color: #FFFFFF; border: 1px solid #E2E8F0;"
    "   transition: all 160ms ease-in-out; box-shadow: none; }"
    ".period-val { font-weight: 700; font-size: 15px; min-width: 28px; color: #17213D; }"
    "entry:focus, spinbutton:focus { border-color: #6842F5; box-shadow: 0 0 0 3px rgba(104,66,245,0.16); }"
    "entry.entry-error { border: 1px solid #DC2626; background-color: #FEF2F2; }"
    "combobox button.combo { padding: 6px 10px; }"
    "check, radio { border-radius: 6px; }"
    /* ---------- status colours ---------- */
    ".good { color: #16A34A; } .warn { color: #DC2626; } .amber { color: #F59E0B; }"
    ".error { color: #DC2626; font-weight: 600; }"
    /* ---------- tables ---------- */
    "treeview { font-size: 13.5px; }"
    "treeview.view { background-color: #FFFFFF; color: #17213D; }"
    "treeview.view:selected, treeview.view:selected:focus, treeview.view:selected:backdrop,"
    "treeview.view:selected:hover { background-color: #E9E4FF; color: #17213D; }"
    "treeview.view:hover { background-color: #F5F3FF; }"
    "treeview header button { background-image: none; background-color: #F1F5F9; color: #334155; font-weight: 600; font-size: 13px;"
    "   border-radius: 0; border: none; border-bottom: 1px solid #E2E8F0; padding: 8px 10px; box-shadow: none; }"
    "scrolledwindow { border-radius: 12px; }"
    "scrollbar slider { background-color: #CBD5E1; border-radius: 8px; min-width: 9px; min-height: 28px; transition: all 160ms; }"
    "scrollbar slider:hover { background-color: #94A3B8; }"
    "scrollbar trough { background-color: transparent; }"
    /* ---------- notebook / tabs ---------- */
    "notebook > header { background: transparent; }"
    "notebook > header > tabs > tab { padding: 9px 18px; font-weight: 500; color: #64748B; transition: all 160ms; }"
    "notebook > header > tabs > tab:checked { color: #6842F5; font-weight: 600; }"
    "notebook > header > tabs > tab label { color: inherit; }"
    /* ---------- progress ---------- */
    "progressbar trough { min-height: 10px; border-radius: 6px; background-color: #E2E8F0; }"
    "progressbar progress { min-height: 10px; border-radius: 6px; background-image: none; background-color: #8B6CF7; transition: all 400ms ease-out; }"
    ".progressbar.trough { background-color: #E2E8F0; border-radius: 6px; min-height: 12px; }"
    ".progressbar.progress { background-image: none; background-color: #8B6CF7; border-radius: 6px; min-height: 12px; border: none; }"
    "treeview progressbar trough, treeview.view progressbar trough { background-color: #E2E8F0; background-image: none; border-radius: 6px; border: none; min-height: 12px; }"
    "treeview progressbar progress, treeview.view progressbar progress { background-color: #8B6CF7; background-image: none; border-radius: 6px; border: none; min-height: 12px; }"
    "treeview progressbar, treeview.view progressbar { color: #17213D; }"
    "progressbar.att-good progress { background-color: #16A34A; }"
    "progressbar.att-low progress { background-color: #DC2626; }"
    /* ---------- math / formulas ---------- */
    ".mono { font-family: Consolas, 'Courier New', monospace; font-size: 12px; }"
    ".formula { background-color: #F5F3FF; border: 1px solid #DDD6FE; border-radius: 12px; padding: 12px 16px; color: #17213D;"
    "   font-family: 'Cambria Math', 'Segoe UI Symbol', 'Inter', 'Segoe UI'; font-size: 14px; }"
    ".note { background-color: #FFFBEB; border: 1px solid #FDE68A; border-radius: 12px; padding: 10px 14px; color: #92400E; }"
    ".tbl { border: 1px solid #E2E8F0; border-radius: 10px; }"
    ".mhead { background-color: #F1F5F9; color: #334155; font-weight: 600; padding: 7px 12px; font-size: 13px; }"
    ".mcell { padding: 6px 12px; font-size: 13px; }"
    ".mzebra { background-color: #F8FAFC; }"
    /* ---------- login ---------- */
    ".login-bg { background-color: #14213D; }"
    ".login-card { background-color: #FFFFFF; border-radius: 22px; padding: 30px 32px; box-shadow: 0 24px 60px rgba(0,0,0,0.40); }"
    ".toast { background-color: #14213D; border-radius: 14px; padding: 10px 18px; box-shadow: 0 10px 30px rgba(20,33,61,0.35); }"
    ".toast label { color: #FFFFFF; font-weight: 500; font-size: 13.5px; }"
    ".login-title { font-size: 26px; font-weight: 700; color: #17213D; }";

void ui_load_css(void)
{
    GtkCssProvider *p = gtk_css_provider_new();
    gtk_css_provider_load_from_data(p, CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(p),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(p);
}
