#ifndef UI_HEADER
#define UI_HEADER

int UI_init();
int UI_loop();
int UI_exit();
void UI_update_time(milliseconds_t);

void UI_toggle_widgetwin();

#endif
