#include <iostream> //
#include <chrono>
#include "main.h"

void whileServer(PipeServer* server);
void whileClient(PipeClient* client);

int main() {
    PipeServer server;
    server.init("myServer");

    PipeClient client;
    client.connect("myServer");

    HANDLE serverThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)whileServer, &server, 0, NULL);
    Sleep(100);

    HANDLE clientThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)whileClient, &client, 0, NULL);

    std::cin.get();

    client.disconnect();
    server.close();

    if (clientThread) { WaitForSingleObject(clientThread, INFINITE); CloseHandle(clientThread); }
    if (serverThread) { WaitForSingleObject(serverThread, INFINITE); CloseHandle(serverThread); }

    return 0;
}

void whileServer(PipeServer* server) {
    if (server == nullptr) { return; }

    while (true) {
        server->update();
        std::vector<byte> rawBytes = server->getReceivedData();

        if (!rawBytes.empty()) {
            std::cout << "[Server] Received " << rawBytes.size() << " bytes: ";
            for (byte b : rawBytes) {
                std::cout << static_cast<int>(b) << " ";
            }
            std::cout << "\n";
        }

        Sleep(10);
    }
}

void whileClient(PipeClient* client) {
    if (client == nullptr) { return; }

    auto lastSendTime = std::chrono::steady_clock::now();
    std::vector<byte> testPacket = { 0x03, 0x00, 10, 20, 30 };

    while (true) {
        client->update();

        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastSendTime).count();

        // Если прошла 1 секунда - ставим пакет в очередь на отправку
        if (elapsedTime >= 1) {
            std::cout << "[Client] Putting packet...\n";
            
            client->sendData(testPacket);
            
            lastSendTime = currentTime;
        }

        Sleep(16);
    }
}
