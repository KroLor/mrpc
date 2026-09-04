#pragma once

#include "server.h"
#include "client.h"
#include "app.h"

class WindowsPipeLayer : public PhysicalLayer {
public:
    void init(const std::string& pipeName);
    bool send(const std::vector<uint8_t>& data) override;

private:
    PipeServer server;
    PipeClient client;

    static void readTaskWrapper(void* pvParameters);
};