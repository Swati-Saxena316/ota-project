#ifndef LCD_UI_H
#define LCD_UI_H

void lcd_init(void);
void lcd_show_message(const char *msg);

// Step 10 additions
void lcd_show_progress_bar(int percent, const char *label); // label can be NULL

#endif
