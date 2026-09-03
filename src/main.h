#pragma once

#include "server.h"
#include "client.h"

void receServer(PipeServer* server);
void sendClient(PipeClient* client, std::vector<byte>& testPacket);
void sendServer(PipeServer* server, std::vector<byte>& testPacket);
void receClient(PipeClient* client);
void protocolTesting(std::vector<byte>& testPacket1, std::vector<byte>& testPacket2);