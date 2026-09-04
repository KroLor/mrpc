#include <iostream>
#include <vector>
#include "main.h"

extern "C" {
    #include "FreeRTOS.h"
    #include "task.h"
}



void WindowsPipeLayer::init(const std::string& pipeName) {
    server.init(pipeName.c_str());
    client.connect(pipeName.c_str());

    // Запускаем асинхронную задачу FreeRTOS для постоянного опроса пайпов
    xTaskCreate(readTaskWrapper, "PipeReadTask", 2048, this, 5, nullptr);
}

bool WindowsPipeLayer::send(const std::vector<uint8_t>& data) {
    std::vector<byte> _data(data.begin(), data.end());
    
    // Отправляем через клиентский пайп
    client.sendData(_data);
    client.update();
    return true;
}

// Асинхронная задача FreeRTOS
void WindowsPipeLayer::readTaskWrapper(void* pvParameters) {
    auto* self = static_cast<WindowsPipeLayer*>(pvParameters);
    
    std::cout << "[System] Задача чтения FreeRTOS запущена.\n";

    for (;;) {
        // 1. Опрашиваем серверный пайп
        self->server.update();
        std::vector<byte> serverBytes = self->server.getReceivedData();
        
        if (!serverBytes.empty() && self->onReceive) {
            std::vector<uint8_t> bytes(serverBytes.begin(), serverBytes.end());
            // Передаем байты вверх
            self->onReceive(bytes); 
        }

        // 2. Опрашиваем клиентский пайп
        self->client.update();
        std::vector<byte> clientBytes = self->client.getReceivedData();
        
        if (!clientBytes.empty() && self->onReceive) {
            std::vector<uint8_t> bytes(clientBytes.begin(), clientBytes.end());
            self->onReceive(bytes);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void demo(void* pvParameters) {
    auto* protocol = static_cast<ProtocolEngine*>(pvParameters);
    
    std::vector<uint8_t> testPacket1 = { 0x03, 0x00, 10, 20, 30 };
    std::vector<uint8_t> testPacket2 = { 30, 0x00, 0x03 };

    vTaskDelay(pdMS_TO_TICKS(500)); 
    
    protocol->sendPacket(testPacket1);

    vTaskDelay(pdMS_TO_TICKS(1000));

    protocol->sendPacket(testPacket2);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main() {
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);

    std::cout << "[System] Инициализация физического уровня Windows...\n";
    
    static WindowsPipeLayer windowsHardware;
    windowsHardware.init("myServer");

    // Создаем кроссплатформенный движок протокола и привязываем его к железу
    static ProtocolEngine protocolEngine("MRPC", windowsHardware);

    // Создаем задачу FreeRTOS, которая будет исполнять тестовый сценарий отправки
    xTaskCreate(demo, "Demo", 2048, &protocolEngine, 1, nullptr);

    std::cout << "[System] Запуск планировщика FreeRTOS...\n";

    vTaskStartScheduler(); 

    return 0;
}