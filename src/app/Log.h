#pragma once

bool LogInit(const char *filename);
void LogTerm();
bool Log(const char *fmt, ...);