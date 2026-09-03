#pragma once

#include "server.h"
#include "client.h"

void receServer(PipeServer* server);
void sendClient(PipeClient* client, const std::vector<byte>& testPacket);
void sendServer(PipeServer* server, const std::vector<byte>& testPacket);
void receClient(PipeClient* client);
void protocolTesting(const std::vector<byte>& testPacket1, const std::vector<byte>& testPacket2);