#include "lcd_ui.h"
#include <string.h>
#include <stdio.h>

// These must exist in LCD driver (To be renamed if different)
extern void lcd_clear(void);
extern void lcd_set_cursor(int col, int row);
extern void lcd_print(const char *s);

void lcd_init(void)
{
    lcd_clear();
}

void lcd_show_message(const char *msg)
{
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print(msg ? msg : "");
}

static void build_bar(int percent, char out[21])
{
    // 20 chars bar: [##########----------] style without brackets to fit
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    int filled = (percent * 20) / 100;
    for (int i = 0; i < 20; i++)
        out[i] = (i < filled) ? '#' : '-';
    out[20] = '\0';
}

void lcd_show_progress_bar(int percent, const char *label)
{
    char bar[21];
    char line0[21];
    char line1[21];

    build_bar(percent, bar);

    snprintf(line0, sizeof(line0), "%s", (label && label[0]) ? label : "Downloading");
    snprintf(line1, sizeof(line1), "%3d%%", percent);

    lcd_clear();

    // Row 0: label
    lcd_set_cursor(0, 0);
    lcd_print(line0);

    // Row 1: bar
    lcd_set_cursor(0, 1);
    lcd_print(bar);

    // Row 2: percent text
    lcd_set_cursor(0, 2);
    lcd_print(line1);
}
