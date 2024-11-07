#ifdef __cplusplus
extern "C" {
#endif

void Syncy_StartApp(void *app);
void Syncy_StopApp();

void Syncy_InitWindow(void *app);
void Syncy_TermWindow();
void Syncy_MainLoopStep();

#ifdef __cplusplus
}
#endif