#ifndef H_FRONTEND_ESP32S3_WAVESHARE
#define H_FRONTEND_ESP32S3_WAVESHARE

int frontend_esp32s3_ui_init(Frontend*, const char *title);
void frontend_esp32s3_ui_exit();

int frontend_esp32s3_input_init(Frontend*);
void frontend_esp32s3_input_exit();

#endif
