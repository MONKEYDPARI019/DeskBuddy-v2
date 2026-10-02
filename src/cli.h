#ifndef CLI_H
#define CLI_H

#include <Arduino.h>

void cliInit();
void cliProcess();
void cliHandleCommand(const char* line);

#endif // CLI_H
