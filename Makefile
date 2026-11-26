# ClassMetrics build file (run from the MSYS2 "MINGW64" shell, or use build.bat)
CC      = gcc
CFLAGS  = -Wall -Wextra -Wno-stringop-truncation -O2 -std=gnu99
LOGIC   = storage.c students.c subjects.c marks.c attendance.c math_analyzer.c reports.c
GUI     = main.c login.c ui_theme.c ui_icons.c ui_common.c dashboard.c ui_students.c ui_subjects.c ui_marks.c \
          ui_attendance.c ui_math.c ui_reports.c ui_profile.c
GTK_CF  = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIB = $(shell pkg-config --libs gtk+-3.0)

all: ClassMetrics_V2.exe

ClassMetrics_V2.exe: $(LOGIC) $(GUI) common.h ui.h
	$(CC) $(CFLAGS) $(GTK_CF) -mwindows -o $@ $(LOGIC) $(GUI) $(GTK_LIB) -lcomdlg32 -lm

# console tests of the non-GUI modules
test: tests/test_logic.c $(LOGIC) common.h
	rm -rf testdata testdata_old testdata_old_backup_v1_* testdata_v2 testdata_v2_backup_v2_*
	mkdir -p testdata
	$(CC) $(CFLAGS) -o tests/test_logic.exe tests/test_logic.c $(LOGIC) -lm
	./tests/test_logic.exe

clean:
	rm -f ClassMetrics_V2.exe tests/test_logic.exe
