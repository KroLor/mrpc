#include <iostream>
#include <cstring>

#include "main.h"
#include "app.h"

extern "C" {
    #include "FreeRTOS.h"
    #include "task.h"
}

// Тестовая функция "эхо"
bool rpcEcho(const uint8_t* args, uint16_t argsLen, uint8_t* out, uint16_t* outLen) {
    if (*outLen < argsLen) return false; 
    
    memcpy(out, args, argsLen);
    *outLen = argsLen;
    return true;
}

// Тестовая функция "сумма"
bool rpcSum(const uint8_t* args, uint16_t argsLen, uint8_t* out, uint16_t* outLen) {
    if (*outLen < sizeof(int)) return false;

    int a, b;
    memcpy(&a, args, sizeof(int));
    memcpy(&b, args + sizeof(int), sizeof(int));

    int result = a + b;

    memcpy(out, &result, sizeof(int));
    *outLen = sizeof(result);

    std::cout << "[Server] add(" << a << ", " << b << ") = " << result << std::endl;
    return true;
}

void clientTask(void* param) {
    App& app = *static_cast<App*>(param);
    
    const char* msg1 = "Hello World!";
    uint8_t resp1[64];
    uint16_t respLen1 = sizeof(resp1);

    int a = 10;
    int b = 25;
    // Буфер для аргументов
    uint8_t argsBuf[2 * sizeof(int)];
    memcpy(argsBuf, &a, sizeof(int));
    memcpy(argsBuf + sizeof(int), &b, sizeof(int));
    // Буфер для ответа
    int result = 0;
    uint16_t respLen = sizeof(result);

    for (;;) {
        // 1

        // Вызываем удаленную функцию echo и ждем ответа 1000 мс
        CallStatus status1 = app.call("echo", (const uint8_t*)msg1, strlen(msg1), resp1, &respLen1, 1000);

        if (status1 == CallStatus::Success) {
            std::cout << "[Client] Success: " << std::string((char*)resp1, respLen1) << std::endl;
        } else {
            std::cout << "[Client] Error! Status: " << static_cast<int>(status1) << std::endl;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));

        // 2

        CallStatus status2 = app.call("sum", argsBuf, sizeof(argsBuf), (uint8_t*)&result, &respLen, 1000);

        if (status2 == CallStatus::Success && respLen == sizeof(int)) {
            std::cout << "[Client] Result of add(" << a << ", " << b << "): " << result << std::endl;
        } else {
            std::cout << "[Client] Add failed! Status: " << static_cast<int>(status2) << std::endl;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// Обработчик чанка стрима
void StreamChunk(const uint8_t* data, uint16_t len) {
    std::cout << "[Client] Stream: " << std::string((const char*)data, len) << std::endl;
}
void clientTaskStream(void* param) {
    App& app = *static_cast<App*>(param);
    const char* msg = "Hello World! _Stream";

    for (;;) {
        // Таймаут 3000 мс для каждого чанка (сервер отправляет с задержкой 500 мс)
        CallStatus status = app.stream("echo", (const uint8_t*)msg, strlen(msg), StreamChunk, 3000);

        std::cout << "[Client] Stream finished, status: " << static_cast<int>(status) << std::endl;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// Пример использования
int main(int argc, char* argv[]) {
    bool isServer = true;
    if (argc == 2 && strcmp(argv[1], "client") == 0) {
        isServer = false;
    }

    std::cout << "[Main] Starting as " << (isServer ? "SERVER" : "CLIENT") << std::endl;

    PhysicsForWin phys("mrpc_pipe", isServer);

    App app(phys);

    if (!app.start()) {
        std::cerr << "[Main] Failed to start App." << std::endl;
        return 2;
    }

    if (isServer) {
        app.regFunc("echo", rpcEcho);
        app.regFunc("sum", rpcSum);
    } else {
        // app.createClientStreamTask(clientTaskStream);
        app.createClientTask(clientTask);
    }

    std::cout << "[Main] Starting FreeRTOS scheduler..." << std::endl;
    vTaskStartScheduler();

    return 0;
}