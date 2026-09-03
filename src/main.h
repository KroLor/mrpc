#pragma once

#include "server.h"
#include "client.h"

void receServer(PipeServer* server);
void sendClient(PipeClient* client, std::vector<byte>* testPacket);
void protocolTesting(std::vector<byte>* testPacket);