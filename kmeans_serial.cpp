#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <limits>
#include <string>

using namespace std;
using namespace chrono;

struct Punto
{
    double x, y, z; // coordenadas (z=0 si es 2D)
    int cluster;    // cluster asignado (-1 = sin asignar)
    int dims;       // 2 o 3
};

// Distancia euclidiana entre dos puntos
double distancia(const Punto &a, const Punto &b)
{
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    double dz = a.z - b.z; // si es 2D, ambos z=0, no afecta
    return sqrt(dx * dx + dy * dy + dz * dz);
}

// Leer CSV de entrada
vector<Punto> leer_csv(const string &ruta)
{
    vector<Punto> puntos;
    ifstream archivo(ruta);

    if (!archivo.is_open())
    {
        cerr << "Error: no se pudo abrir el archivo: " << ruta << endl;
        exit(1);
    }

    string linea;
    int dims = -1; // se detecta en la primera línea

    while (getline(archivo, linea))
    {
        if (linea.empty())
            continue;

        stringstream ss(linea);
        string token;
        vector<double> vals;

        // Separamos por coma y convertimos a double
        while (getline(ss, token, ','))
        {
            vals.push_back(stod(token));
        }

        // Detectamos dimensión en la primera fila
        if (dims == -1)
        {
            dims = (int)vals.size();
            if (dims != 2 && dims != 3)
            {
                cerr << "Error: el CSV debe tener 2 o 3 columnas." << endl;
                exit(1);
            }
        }

        Punto p;
        p.x = vals[0];
        p.y = vals[1];
        p.z = (dims == 3) ? vals[2] : 0.0;
        p.cluster = -1;
        p.dims = dims;
        puntos.push_back(p);
    }

    archivo.close();
    return puntos;
}

vector<Punto> inicializar_centroides(const vector<Punto> &puntos, int k)
{
    vector<Punto> centroides;
    for (int i = 0; i < k; i++)
    {
        centroides.push_back(puntos[i]);
    }
    return centroides;
}

// -------------------------------------------------------
// FUNCIÓN: asignar cada punto al centroide más cercano
// Retorna true si algún punto cambió de cluster (no convergió)
// -------------------------------------------------------
bool asignar_clusters(vector<Punto> &puntos, const vector<Punto> &centroides)
{
    bool hubo_cambio = false;

    for (int i = 0; i < (int)puntos.size(); i++)
    {
        double min_dist = numeric_limits<double>::max();
        int mejor_cluster = 0;

        // Comparamos contra cada centroide
        for (int j = 0; j < (int)centroides.size(); j++)
        {
            double d = distancia(puntos[i], centroides[j]);
            if (d < min_dist)
            {
                min_dist = d;
                mejor_cluster = j;
            }
        }

        // Si el cluster cambió, marcamos que no convergió
        if (puntos[i].cluster != mejor_cluster)
        {
            puntos[i].cluster = mejor_cluster;
            hubo_cambio = true;
        }
    }

    return hubo_cambio;
}

// -------------------------------------------------------
// FUNCIÓN: recalcular centroides
// El nuevo centroide es el promedio de todos los puntos del cluster
// -------------------------------------------------------
vector<Punto> recalcular_centroides(const vector<Punto> &puntos, int k)
{
    // Acumuladores de suma y conteo por cluster
    vector<double> suma_x(k, 0.0), suma_y(k, 0.0), suma_z(k, 0.0);
    vector<int> conteo(k, 0);

    for (const Punto &p : puntos)
    {
        suma_x[p.cluster] += p.x;
        suma_y[p.cluster] += p.y;
        suma_z[p.cluster] += p.z;
        conteo[p.cluster]++;
    }

    vector<Punto> nuevos_centroides(k);
    for (int j = 0; j < k; j++)
    {
        if (conteo[j] > 0)
        {
            nuevos_centroides[j].x = suma_x[j] / conteo[j];
            nuevos_centroides[j].y = suma_y[j] / conteo[j];
            nuevos_centroides[j].z = suma_z[j] / conteo[j];
        }
        nuevos_centroides[j].cluster = j;
        nuevos_centroides[j].dims = puntos[0].dims;
    }

    return nuevos_centroides;
}

// -------------------------------------------------------
// FUNCIÓN: guardar CSV de salida
// Agrega columna "cluster" al final
// -------------------------------------------------------
void guardar_csv(const vector<Punto> &puntos, const string &ruta)
{
    ofstream archivo(ruta);

    if (!archivo.is_open())
    {
        cerr << "Error: no se pudo crear el archivo de salida." << endl;
        exit(1);
    }

    // Encabezado
    if (puntos[0].dims == 2)
        archivo << "x,y,cluster\n";
    else
        archivo << "x,y,z,cluster\n";

    // Datos
    for (const Punto &p : puntos)
    {
        if (p.dims == 2)
            archivo << p.x << "," << p.y << "," << p.cluster << "\n";
        else
            archivo << p.x << "," << p.y << "," << p.z << "," << p.cluster << "\n";
    }

    archivo.close();
}

// -------------------------------------------------------
// MAIN
// -------------------------------------------------------
int main(int argc, char *argv[])
{

    // Validar argumentos
    if (argc != 4)
    {
        cerr << "Uso: ./kmeans_serial <archivo.csv> <K> <max_iter>" << endl;
        cerr << "Ejemplo: ./kmeans_serial datos.csv 5 100" << endl;
        return 1;
    }

    string ruta_entrada = argv[1];
    int K = atoi(argv[2]);
    int MAX_ITER = atoi(argv[3]);

    // --- Leer datos ---
    cout << "Leyendo: " << ruta_entrada << endl;
    vector<Punto> puntos = leer_csv(ruta_entrada);
    cout << "Puntos : " << puntos.size() << endl;
    cout << "K      : " << K << endl;
    cout << "MaxIter: " << MAX_ITER << endl;

    // --- Inicializar centroides ---
    vector<Punto> centroides = inicializar_centroides(puntos, K);

    // --- Medir tiempo de inicio ---
    auto t_inicio = high_resolution_clock::now();
