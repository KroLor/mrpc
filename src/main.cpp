#include <iostream>
#include "main.h"

int main() {
    std::vector<byte> testPacket = { 0x03, 0x00, 10, 20, 30 };

    protocolTesting(&testPacket);



    return 0;
}

void receServer(PipeServer* server) {
    if (server == nullptr) { return; }

    server->update();
    std::vector<byte> rawBytes = server->getReceivedData();

    if (!rawBytes.empty()) {
        std::cout << "[Server] Received " << rawBytes.size() << " bytes: ";
        for (byte b : rawBytes) {
            std::cout << static_cast<int>(b) << " ";
        }
        std::cout << "\n";
    }
}

void sendClient(PipeClient* client, std::vector<byte>* testPacket) {
    if (client == nullptr) { return; }

    std::cout << "[Client] Putting packet...\n";
    
    client->sendData(*testPacket);
    client->update();
}

void protocolTesting(std::vector<byte>* testPacket) {
    PipeServer server;
    server.init("myServer");
    PipeClient client;
    client.connect("myServer");

    receServer(&server);

    sendClient(&client, testPacket);
    Sleep(500);
    receServer(&server);

    client.disconnect();
    server.close();
}