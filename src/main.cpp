#include <iostream>
#include "main.h"

int main() {
    std::vector<byte> testPacket1 = { 0x03, 0x00, 10, 20, 30 };
    std::vector<byte> testPacket2 = { 30, 0x00, 0x03 };

    protocolTesting(testPacket1, testPacket2);

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

void sendClient(PipeClient* client, const std::vector<byte>& testPacket) {
    if (client == nullptr) { return; }

    std::cout << "[Client] Putting packet...\n";
    
    client->sendData(testPacket);
    client->update();
}

void sendServer(PipeServer* server, const std::vector<byte>& testPacket) {
    if (server == nullptr) { return; }

    std::cout << "[Server] Putting packet...\n";
    
    server->sendData(testPacket);
    server->update();
}

void receClient(PipeClient* client) {
    if (client == nullptr) { return; }

    client->update();
    std::vector<byte> rawBytes = client->getReceivedData();

    if (!rawBytes.empty()) {
        std::cout << "[Client] Received " << rawBytes.size() << " bytes: ";
        for (byte b : rawBytes) {
            std::cout << static_cast<int>(b) << " ";
        }
        std::cout << "\n";
    }
}

void protocolTesting(const std::vector<byte>& testPacket1, const std::vector<byte>& testPacket2) {
    PipeServer server;
    server.init("myServer");
    PipeClient client;
    client.connect("myServer");

    receServer(&server);

    sendClient(&client, testPacket1);
    Sleep(500);
    receServer(&server);
    Sleep(500);
    sendServer(&server, testPacket2);
    Sleep(500);
    receClient(&client);

    client.disconnect();
    server.close();
}