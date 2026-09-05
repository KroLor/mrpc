#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "physics.h"

class Channels {
public:
    Channels(Physics& physics);

    /**
     * @param payload Указатель на данные транспортного уровня.
     * @param len Длина данных.
     * @return true при успешной отправке.
     */
    bool send(const uint8_t* payload, uint16_t len);

    /**
     * @param out Буфер для принятой полезной нагрузки.
     * @param outSize Размер буфера out.
     * @param outLen Фактически принятая длина payload.
     * @param timeoutMs Таймаут ожидания байтов от physics.
     * @return true если за время таймаута был успешно принят и проверен целый пакет.
     */
    bool poll(uint8_t* out, uint16_t outSize, uint16_t* outLen, uint32_t timeoutMs);

    void reset();

private:
    enum class State : uint8_t {
        WaitStart, // Ожидание 0xFA
        ReadLenLo, // Чтение l_l 
        ReadLenHi, // Чтение l_h
        ReadHdrCrc, // Чтение и проверка crc8 заголовка
        WaitData, // Ожидание маркера 0xFB
        ReadPayload, // Чтение тела пакета (сообщения транспортного уровня)
        ReadPktCrc, // Чтение и проверка crc8 всего пакета
        WaitStop // Ожидание 0xFE
    };

    State m_state = State::WaitStart;
    
    uint16_t m_payloadLen = 0; // Ожидаемая длина payload (из l_l и l_h)
    uint16_t m_payloadPos = 0; // Текущая позиция записи в m_payloadBuf
    uint8_t  m_hdrCrc = 0; // Вычисленная CRC заголовка
    uint8_t  m_pktCrc = 0; // CRC всего пакета (от 0xFA до конца payload)

    static constexpr uint16_t kMaxPayload = 256; // Максимальный размер payload, байты
    uint8_t m_payloadBuf[kMaxPayload]; // Внутренний буфер 

    Physics& m_phys;

    // Вычисление CRC8 для массива байт
    static uint8_t calcCrc8(const uint8_t* data, uint16_t len);
    // Обновление CRC8 новым байтом
    static uint8_t updateCrc8(uint8_t crc, uint8_t byte);
};