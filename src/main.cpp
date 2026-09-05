#include <iostream>
#include <cstring>

#include "main.h"
#include "app.h"

PhysicsForWin::PhysicsForWin(const char* pipeName, bool isServer): 
    m_pipeName(pipeName ? pipeName : "mrpc_pipe"),
    m_isServer(isServer),
    m_server(isServer ? std::make_unique<PipeServer>() : nullptr),
    m_client(isServer ? nullptr : std::make_unique<PipeClient>()),
    m_init(false) {}

PhysicsForWin::~PhysicsForWin() {
    deinit();
}

bool PhysicsForWin::init() {
    if (m_init) return true;

    if (m_isServer) {
        if (m_server && m_server->init(m_pipeName.c_str())) {
            m_init = true;
            return true;
        }
    } else {
        if (m_client && m_client->connect(m_pipeName.c_str())) {
            m_init = true;
            return true;
        }
    }
    return false;
}

void PhysicsForWin::deinit() {
    if (!m_init) return;

    if (m_isServer && m_server) {
        m_server->close();
    } else if (!m_isServer && m_client) {
        m_client->disconnect();
    }
    m_init = false;
}

bool PhysicsForWin::send(const uint8_t* data, uint16_t len) {
    if (!m_init || !data || len == 0) return false;

    std::vector<uint8_t> vec(data, data + len);
    if (m_isServer && m_server) {
        return m_server->sendData(vec);
    } else if (!m_isServer && m_client) {
        return m_client->sendData(vec);
    }
    return false;
}

uint16_t PhysicsForWin::recv(uint8_t* data, uint16_t maxSize, uint32_t timeoutMs) {
    if (!m_init || !data || maxSize == 0) return 0;

    if (m_leftover.empty()) {
        bool hasData = false;
        if (m_isServer && m_server) {
            hasData = m_server->waitForData(timeoutMs);
        } else if (!m_isServer && m_client) {
            hasData = m_client->waitForData(timeoutMs);
        }
    }

    std::vector<uint8_t> avail;
    if (m_isServer && m_server) {
        avail = m_server->getReceivedData();
    } else {
        avail = m_client->getReceivedData();
    }

    avail.insert(avail.begin(), m_leftover.begin(), m_leftover.end());
    m_leftover.clear();

    if (avail.empty()) return 0;

    // Копируем в пользовательский буфер
    uint16_t copyLen = std::min(static_cast<uint16_t>(avail.size()), maxSize);
    std::memcpy(data, avail.data(), copyLen);

    if (avail.size() > copyLen) {
        m_leftover.assign(avail.begin() + copyLen, avail.end());
    }

    return copyLen;
}

void PhysicsForWin::update() {
    if (!m_init) return;
    
    if (m_isServer && m_server) {
        m_server->update();
    } else if (m_client) {
        m_client->update();
    }
}

bool PhysicsForWin::isConnected() const {
    if (!m_init) return false;
    
    if (m_isServer && m_server) {
        return m_server->isConnected();
    } else if (!m_isServer && m_client) {
        return m_client->isConnected();
    }
    return false;
}

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

// [3.2]
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

// [4.2]
// Обработчик чанка стрима
void StreamChunk(const uint8_t* data, uint16_t len) {
    std::cout << "[Client] Stream: " << std::string((const char*)data, len) << std::endl;
}
void clientTaskStream(void* param) {
    App& app = *static_cast<App*>(param);
    const char* msg = "Hello World! _Stream";

    for (;;) {
        CallStatus status = app.stream("echo", (const uint8_t*)msg, strlen(msg), StreamChunk);

        std::cout << "[Client] Stream finished, status: " << static_cast<int>(status) << std::endl;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

int main(int argc, char* argv[]) {
    bool isServer = true;
    if (argc == 2 && strcmp(argv[1], "client") == 0) {
        isServer = false;
    }

    std::cout << "[Main] Starting as " << (isServer ? "SERVER" : "CLIENT") << std::endl;

    // Создаем физический уровень ForWin
    PhysicsForWin phys("mrpc_pipe", isServer);
    
    // Создаем приложение [1]
    App app(phys);

    // Запускаем [2]
    if (!app.start()) {
        std::cerr << "[Main] Failed to start App." << std::endl;
        return 2;
    }

    // Настраиваем роль
    if (isServer) {
        // Регистрируем функции, которые будут доступны клиенту, задача приема остается [3.1]
        // app.regFunc("echo", rpcEcho);
        // app.regFunc("sum", rpcSum);
        // std::cout << "[Server] Registered 'echo' and 'add'. Waiting for requests..." << std::endl;


        // [4.1]
        app.regFunc("echo", rpcEcho);
    } else {
        // Запускаем задачу, которая будет слать запросы [3.2]
        // 8192 / sizeof(StackType_t) - расчет стека для Win32, где StackType_t для Win32 = 4 байта
        // xTaskCreate(clientTask, "ClientTask", 8192 / sizeof(StackType_t), &app, 1, NULL); // Приоритет 1, так как приём важнее отправки


        // [4.2]
        xTaskCreate(clientTaskStream, "ClientTaskStream", 8192 / sizeof(StackType_t), &app, 1, NULL);
    }

    // Запускаем планировщик RTOS [4]
    std::cout << "[Main] Starting FreeRTOS Scheduler..." << std::endl;
    vTaskStartScheduler();

    return 0;
}