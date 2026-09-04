#include "app.h"

extern "C" {
    #include "FreeRTOS.h"
    #include "task.h"
}

#include <iostream>

App::App(Physics& phys) : 
    m_physics(phys), 
    m_channels(phys), 
    m_transport(m_channels), 
    m_rxTaskHandle(nullptr) {}

bool App::start() {
    // Инициализируем физический уровень (UART или Named Pipe)
    if (!m_physics.init()) {
        std::cerr << "[App] Failed to initialize physical layer." << std::endl;
        return false;
    }

    // Создаем задачу приема.
    const uint32_t stackSizeBytes = 4096; 
    const uint32_t stackSizeWords = stackSizeBytes / sizeof(StackType_t);

    BaseType_t result = xTaskCreate(
        rxTaskEntry, // Функция задачи
        "AppRxTask", // Имя для отладчика
        stackSizeWords, // Глубина стека (размер)
        this, // Параметр (указатель на экземпляр App)
        2, // Приоритет
        &m_rxTaskHandle // Дескриптор задачи для удаления
    );

    if (result != pdPASS) {
        std::cerr << "[App] Failed to create RX task." << std::endl;
        m_physics.deinit();
        return false;
    }

    return true;
}

void App::stop() {
    // Безопасное удаление задачи
    if (m_rxTaskHandle != nullptr) {
        vTaskDelete(m_rxTaskHandle);
        m_rxTaskHandle = nullptr;
    }

    // Деинициализация физического уровня
    m_physics.deinit();
}

bool App::regFunc(const char* name, RpcHandler handler) {
    return m_transport.regFunc(name, handler);
}

CallStatus App::call(const char* name, 
                    const uint8_t* args, uint16_t argsLen, 
                    uint8_t* out, uint16_t* outLen, 
                    uint32_t timeoutMs) {
    return m_transport.call(name, args, argsLen, out, outLen, timeoutMs);
}

bool App::isConnected() const {
    return m_physics.isConnected();
}

void App::rxTaskEntry(void* param) {
    App* appInst = static_cast<App*>(param);

    appInst->rxLoop();
    
    // Если цикл завершился, задача удаляет сама себя
    vTaskDelete(nullptr);
}

void App::rxLoop() {
    const uint32_t pollTimeoutMs = 10; 

    for (;;) {
        m_physics.update();

        if (m_physics.isConnected()) {
            m_transport.dispatchOnce(pollTimeoutMs);
        } else {
            m_transport.linkDown();
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}