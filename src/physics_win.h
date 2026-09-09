#pragma once

#include "physics.h"
#include "server.h"
#include "client.h"
#include <memory>
#include <vector>
#include <string>
#include <cstdint>

class PhysicsForWin : public Physics {
public:
    PhysicsForWin(const char* pipeName, bool isServer);
    ~PhysicsForWin() override;
    
    bool init() override;
    void deinit() override;

    bool send(const uint8_t* data, uint16_t len) override;
    uint16_t recv(uint8_t* data, uint16_t maxSize, uint32_t timeoutMs) override;

    void update() override;

    bool isConnected() const override;

private:
    std::string m_pipeName;
    bool m_isServer;
    std::unique_ptr<PipeServer> m_server;
    std::unique_ptr<PipeClient> m_client;
    std::vector<uint8_t> m_leftover;
    bool m_init = false;
};
