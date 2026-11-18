#ifndef BROWSER_NATIVE_DIALOG_H
#define BROWSER_NATIVE_DIALOG_H
/* Sony utility UI; finished!=NULL makes this a cancellable progress dialog.
 * -2 means utility ownership could not be resolved: retain live resources. */
int browser_native_dialog(const char *message,volatile int *cancelled,volatile int *finished);
void browser_ui_begin(void);
void browser_ui_frame_begin(void);
void browser_ui_frame_end(void);
void browser_ui_end(void);
#endif
