#pragma once 

class ProtocolEngine {
public:
    ProtocolEngine(const std::string& nodeName, PhysicalLayer& hardware) : name(nodeName), hw(hardware) {
        hw.setReceiveCallback([this](const std::vector<uint8_t>& rawData) { this->parseIncomingBytes(rawData); });
    }

    void sendPacket(const std::vector<uint8_t>& appPayload) {
        std::cout << "[" << name << "] Отправка пакета уровня ...\n";
        
        // Канальный уровень
        hw.send(appPayload);
    }

private:
    std::string name;
    PhysicalLayer& hw;

    void parseIncomingBytes(const std::vector<uint8_t>& rawData) {
        std::cout << "[" << name << "] Протокол принял " << rawData.size() << " байт: ";
        for (uint8_t b : rawData) {
            std::cout << static_cast<int>(b) << " ";
        }
        std::cout << "\n";
        
        // Транспортный/Канальный уровень
    }
};