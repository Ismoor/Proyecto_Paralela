#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <winsock2.h> // Librería de Sockets para Windows

// Linker para Visual Studio
#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define DATA_FILE "bitcoin_prices.txt"

using namespace std;

// Cargar datos (Igual que antes)
vector<double> loadData(const string& filename) {
    vector<double> data;
    ifstream file(filename);
    double price;

    if (!file.is_open()) {
        cerr << "Error: No se pudo abrir " << filename << endl;
        // En Windows pausamos para ver el error antes de cerrar
        system("pause");
        exit(EXIT_FAILURE);
    }

    while (file >> price) {
        data.push_back(price);
    }

    file.close();
    cout << "Dataset cargado: " << data.size() << " registros en memoria." << endl;
    return data;
}

int main() {
    // 1. Inicializar Winsock (Exclusivo de Windows)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "Fallo en WSAStartup" << endl;
        return -1;
    }

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    vector<double> marketData = loadData(DATA_FILE);

    // 2. Crear Socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        cout << "Fallo al crear socket: " << WSAGetLastError() << endl;
        WSACleanup();
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 3. Bind
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        cout << "Fallo en bind: " << WSAGetLastError() << endl;
        closesocket(server_fd);
        WSACleanup();
        return -1;
    }

    // 4. Listen
    if (listen(server_fd, 3) == SOCKET_ERROR) {
        cout << "Fallo en listen" << endl;
        return -1;
    }

    cout << "Servidor Windows escuchando en puerto " << PORT << "..." << endl;

    while (true) {
        // 5. Accept
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen)) == INVALID_SOCKET) {
            cout << "Fallo en accept" << endl;
            continue;
        }

        cout << "Cliente conectado." << endl;

        // 6. Recibir petición
        int dataRequested = 0;
        recv(new_socket, (char*)&dataRequested, sizeof(dataRequested), 0);
        cout << "Cliente pide " << dataRequested << " registros." << endl;

        if (dataRequested > marketData.size()) dataRequested = marketData.size();

        // Enviar confirmación de tamaño
        send(new_socket, (char*)&dataRequested, sizeof(dataRequested), 0);

        // 7. Enviar datos
        int bytesToSend = dataRequested * sizeof(double);
        send(new_socket, (char*)marketData.data(), bytesToSend, 0);

        cout << "Datos enviados." << endl;
        closesocket(new_socket);
    }

    closesocket(server_fd);
    WSACleanup(); // Limpieza final
    return 0;
}