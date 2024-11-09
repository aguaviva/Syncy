#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void Syncy_CreateApp(void *app);
void Syncy_DestroyApp();

void Syncy_InitWindow(void *app);
void Syncy_TermWindow();

bool Syncy_MainLoopStep();

#ifdef __cplusplus
}
#endif