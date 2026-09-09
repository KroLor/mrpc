#pragma once

/**
 * @file main.h
 * @brief Заголовочный файл для тестового приложения MRPC под Windows.
 * 
 * Подключает реализации физического уровня для Windows (PhysicsForWin),
 * а также классы сервера и клиента для работы с Named Pipe.
 */

#include "physics_win.h"
#include "server.h"
#include "client.h"
#include <memory>
#include <vector>
#include <string>
#include <cstdint>