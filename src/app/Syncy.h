#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void Syncy_StartApp(void *app);
void Syncy_StopApp();

void Syncy_InitWindow(void *app);
void Syncy_TermWindow();

bool Syncy_MainLoopStep();

#ifdef __cplusplus
}
#endif