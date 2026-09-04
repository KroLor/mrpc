#pragma once

#include <vector>
#include <cstdint>
#include <functional>

class PhysicalLayer {
public:
    virtual ~PhysicalLayer() = default;
    
    // Метод для отправки данных в канал
    virtual bool send(const std::vector<uint8_t>& data) = 0;
    
    // Регистрация колбэка, который будет вызываться при получении сырых байт
    void setReceiveCallback(std::function<void(const std::vector<uint8_t>&)> callback) {
        onReceive = callback;
    }

protected:
    std::function<void(const std::vector<uint8_t>&)> onReceive;
};