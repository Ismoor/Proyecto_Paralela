#include <iostream>
#include <vector>
#include <cmath>
#include <numeric>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <omp.h>
#include <random>
#include <ctime>

// Linker para Visual Studio
#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "172.17.42.176"
#define PORT 8080
#define PREDICTION_DAYS 30

using namespace std;

struct SimulationResult {
    double expectedPrice;
    double executionTime;
};

// --- RED (WINSOCK) ---
vector<double> getDataFromServer(int n_data) {
    WSADATA wsaData;
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in serv_addr;
    vector<double> data;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Error WSAStartup" << endl;
        exit(EXIT_FAILURE);
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        cerr << "Error creando socket" << endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        cerr << "Direccion IP invalida" << endl;
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Error de conexion. Revisa el servidor." << endl;
        exit(EXIT_FAILURE);
    }

    send(sock, (char*)&n_data, sizeof(n_data), 0);

    int dataIncoming = 0;
    recv(sock, (char*)&dataIncoming, sizeof(dataIncoming), 0);
    cout << "--- RED: Descargando " << dataIncoming << " registros del servidor... ---" << endl;

    data.resize(dataIncoming);
    int totalBytes = dataIncoming * sizeof(double);
    int bytesReceived = 0;
    char* ptr = (char*)data.data();

    while (bytesReceived < totalBytes) {
        int valread = recv(sock, ptr + bytesReceived, totalBytes - bytesReceived, 0);
        if (valread <= 0) break;
        bytesReceived += valread;
    }

    closesocket(sock);
    WSACleanup();
    return data;
}

// --- LOGICA FINANCIERA ---
void calculateStats(const vector<double>& prices, double& mu, double& sigma, double& lastPrice) {
    if (prices.size() < 2) return;
    vector<double> returns;
    for (size_t i = 1; i < prices.size(); i++) {
        returns.push_back(log(prices[i] / prices[i - 1]));
    }
    double sum = accumulate(returns.begin(), returns.end(), 0.0);
    double mean = sum / returns.size();
    double sq_sum = 0.0;
    for (double r : returns) sq_sum += (r - mean) * (r - mean);

    mu = mean;
    sigma = sqrt(sq_sum / returns.size());
    lastPrice = prices.back();
}

// --- SECUENCIAL ---
// Ahora recibe "num_sims" como parámetro
SimulationResult runSequential(double lastPrice, double mu, double sigma, long long num_sims) {
    double start_time = omp_get_wtime();
    double total_final_price = 0.0;

    random_device rd;
    mt19937 gen(rd());
    normal_distribution<> d(0, 1);

    for (long long i = 0; i < num_sims; i++) {
        double current_price = lastPrice;
        for (int day = 0; day < PREDICTION_DAYS; day++) {
            double shock = d(gen);
            double drift = (mu - 0.5 * sigma * sigma);
            current_price = current_price * exp(drift + sigma * shock);
        }
        total_final_price += current_price;
    }
    double end_time = omp_get_wtime();
    return { total_final_price / (double)num_sims, end_time - start_time };
}

// --- PARALELO (OPENMP) ---
SimulationResult runParallel(double lastPrice, double mu, double sigma, long long num_sims) {
    double start_time = omp_get_wtime();
    double total_final_price = 0.0;

    // Detectar hilos antes de empezar
    int n_threads = omp_get_max_threads();
    cout << "   [OpenMP] Lanzando hilos de procesamiento: " << n_threads << endl;

#pragma omp parallel reduction(+:total_final_price)
    {
        unsigned int seed = omp_get_thread_num() + (unsigned int)time(NULL);
        mt19937 gen(seed);
        normal_distribution<> d(0, 1);

#pragma omp for
        for (long long i = 0; i < num_sims; i++) {
            double current_price = lastPrice;
            for (int day = 0; day < PREDICTION_DAYS; day++) {
                double shock = d(gen);
                double drift = (mu - 0.5 * sigma * sigma);
                current_price = current_price * exp(drift + sigma * shock);
            }
            total_final_price += current_price;
        }
    }
    double end_time = omp_get_wtime();
    return { total_final_price / (double)num_sims, end_time - start_time };
}

int main() {
    int n_data;
    long long num_sims; // Variable para guardar el input del usuario
    char opcion = 's';

    cout << "===========================================" << endl;
    cout << "      CLIENTE HPC - MONTE CARLO BITCOIN    " << endl;
    cout << "===========================================" << endl;

    do {
        // 1. Configuración de parámetros
        cout << "1. Cuantos datos historicos solicitar al servidor? (Recomendado: 5000+): ";
        cin >> n_data;

        cout << "2. Cuantas simulaciones Monte Carlo ejecutar? (Prueba HPC: 10000000+): ";
        cin >> num_sims;

        // 2. Obtener datos
        vector<double> prices = getDataFromServer(n_data);

        if (prices.size() < 2) {
            cout << "Error: No hay suficientes datos." << endl;
            system("pause");
            return -1;
        }

        // 3. Calcular estadísticas
        double mu, sigma, lastPrice;
        calculateStats(prices, mu, sigma, lastPrice);

        cout << "\n--- ESTADISTICAS ---" << endl;
        cout << "Precio Actual: $" << lastPrice << endl;
        cout << "Volatilidad Diaria: " << sigma << endl;
        cout << "--------------------" << endl;

        // 4. Ejecución
        cout << "\n>>> Iniciando Ejecucion SECUENCIAL..." << endl;
        SimulationResult seq = runSequential(lastPrice, mu, sigma, num_sims);
        cout << "Tiempo: " << seq.executionTime << " s. || Prediccion: $" << seq.expectedPrice << endl;

        cout << "\n>>> Iniciando Ejecucion PARALELA..." << endl;
        SimulationResult par = runParallel(lastPrice, mu, sigma, num_sims);
        cout << "Tiempo: " << par.executionTime << " s. || Prediccion: $" << par.expectedPrice << endl;

        // 5. Resultados finales
        cout << "\n================ RESULTADOS ================" << endl;
        double speedup = seq.executionTime / par.executionTime;

        // Obtener hilos usados para calcular eficiencia
        int num_threads = omp_get_max_threads();
        double efficiency = (speedup / num_threads) * 100.0;

        cout << "Speedup:    " << speedup << "x" << endl;
        cout << "Eficiencia: " << efficiency << "%" << endl;
        cout << "============================================" << endl;

        // Preguntar si quiere seguir
        cout << "\nQuieres realizar otra prueba? (s/n): ";
        cin >> opcion;

    } while (opcion == 's' || opcion == 'S');

    cout << "Fin del programa." << endl;
}