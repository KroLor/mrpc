#include "physics_win.h"
#include <cstring>
#include <algorithm>

// For Win //

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

    // Копируем в буфер
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

// For Win //
