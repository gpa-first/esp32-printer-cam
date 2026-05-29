#pragma once

#include <Arduino.h>
#include <functional>

using TelegramCommandHandler = std::function<void(const String &cmd, const String &args)>;

bool telegramInit();
void telegramLoop();
bool telegramSendMessage(const String &text);
bool telegramSendPhoto(const uint8_t *jpg, size_t len, const String &caption = "");
void telegramSetCommandHandler(TelegramCommandHandler handler);
