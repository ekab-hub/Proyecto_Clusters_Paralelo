#include <iostream> // cout
#include <fstream>  // leer y escribir archivos
#include <sstream>  // separar cadenas
#include <vector>   // arreglos dinamicos
#include <cmath>    // math
#include <cstdlib>  // conversiones utiles
#include <chrono>   // tiempo
#include <limits>   // max o min
#include <string>   // string
#include <omp.h>    // PARALELISMO

using namespace std; // facilita escritura como cout

struct Punto // agrupa los puntos
{
    double x, y, z; // coordenadas (z=0 si es 2D)
    int cluster;    // cluster asignado (-1 = sin asignar)
    int dims;       // 2 o 3
};

double distancia(const Punto &a, const Punto &b) // calcula distancia euclidiana. Notacion sirve para que solo copie los valores y prohibe modificar
{
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    double dz = a.z - b.z;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

vector<Punto> leer_csv(string ruta) // lee csv y va a guardar los puntos encontrados en un vector
{
    vector<Punto> puntos;   // crea el vector vacio
    ifstream archivo(ruta); // abre el archivo para leerlo

    if (!archivo.is_open())
    {
        cerr << "Error: no se pudo abrir el archivo: " << ruta << endl;
        exit(1);
    }

    string linea;
    int dims = -1; // no se sabe todavia la dimension

    while (getline(archivo, linea)) // va linea por linea dentro del archivo
    {
        if (linea.empty())
            continue; // si hay una linea vacia, no rompe, solo se la salta

        stringstream ss(linea); // permite separar la linea
        string token;
        vector<double> valores;

        while (getline(ss, token, ',')) // mientras pueda leer la linea hasta una coma
        {
            valores.push_back(stod(token)); // guarda el pedazo en token, stod convierte a numero, lo mete al vector valores.
        }

        if (dims == -1)
        {
            dims = (int)valores.size(); // cuenta el numero de valores en el vector. Si es 2: 2D, si es 3: 3D
            if (dims != 2 && dims != 3)
            {
                cerr << "Error: el CSV debe tener 2 o 3 columnas." << endl;
                exit(1);
            }
        }

        Punto p; // crea el punto y llena con valores del archivo
        p.x = valores[0];
        p.y = valores[1];
        if (dims == 3)
            p.z = valores[2];
        else
            p.z = 0.0;
        p.cluster = -1;
        p.dims = dims;
        puntos.push_back(p);
    }

    archivo.close();
    return puntos; // cierra el archivo y regresa el vector con todos los puntos
}

vector<Punto> inicializar_centroides(vector<Punto> &puntos, int k)
{
    vector<Punto> centroides; // crea vector de puntos donde se guardarán centroides

    // Validar que K sea válido
    if (k <= 0)
    {
        cerr << "Error: K debe ser mayor que 0." << endl;
        exit(1);
    }

    if (k > puntos.size())
    {
        cerr << "Error: K no puede ser mayor que el numero de puntos." << endl;
        exit(1);
    }

    for (int i = 0; i < k; i++)
    {
        centroides.push_back(puntos[i]); // Toma los primeros k puntos del vector de puntos y los agrega al vector de centroides
    }

    return centroides;
}

bool asignar_clusters(vector<Punto> &puntos, const vector<Punto> &centroides) // si al menos un punto cambio de centroide, regresa True
{
    bool hubo_cambio = false; // variable de control

#pragma omp parallel for schedule(static) reduction(|| : hubo_cambio)
    for (int i = 0; i < (int)puntos.size(); i++) // recorre todos los puntos del vector
    {
        double min_dist = numeric_limits<double>::max(); // inicializa con un valor muy grande
        int mejor_cluster = 0;                           // para guardar el indice del centroide mas cercano

        // Comparamos contra cada centroide
        for (int j = 0; j < (int)centroides.size(); j++)
        {
            double d = distancia(puntos[i], centroides[j]); // calcula distancia entre punto i y centroide j
            if (d < min_dist)                               // si la distancia calculada es menor a la que se tenia registrada, actualiza variables
            {
                min_dist = d;
                mejor_cluster = j;
            }
        }

        if (puntos[i].cluster != mejor_cluster) // Si el cluster cambió, actualizo el su cluster
        {
            puntos[i].cluster = mejor_cluster;
            hubo_cambio = true;
        }
    }

    return hubo_cambio;
}

vector<Punto> recalcular_centroides(vector<Punto> puntos, vector<Punto> centroides, int k)
{
    // Usamos arreglos C puros (no vector) porque OpenMP reduction solo soporta reduccion de arreglos con esta sintaxis
    double suma_x[k] = {};
    double suma_y[k] = {};
    double suma_z[k] = {};
    int conteo[k] = {};

#pragma omp parallel for schedule(static) reduction(+ : suma_x[ : k], suma_y[ : k], suma_z[ : k], conteo[ : k])
    for (int i = 0; i < (int)puntos.size(); i++)
    {
        int c = puntos[i].cluster;
        suma_x[c] += puntos[i].x;
        suma_y[c] += puntos[i].y;
        suma_z[c] += puntos[i].z;
        conteo[c]++;
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
        else
        {
            nuevos_centroides[j].x = centroides[j].x;
            nuevos_centroides[j].y = centroides[j].y;
            nuevos_centroides[j].z = centroides[j].z;
        }
        nuevos_centroides[j].cluster = j;
        nuevos_centroides[j].dims = puntos[0].dims;
    }

    return nuevos_centroides;
}

void guardar_csv(const vector<Punto> &puntos, const string &ruta)
{
    ofstream archivo(ruta); // abre el archivo para escritura

    if (!archivo.is_open())
    {
        cerr << "Error: no se pudo crear el archivo de salida." << endl;
        exit(1);
    }

    if (puntos[0].dims == 2) // Encabezado del csv. Si dims = 2, solo pone 3 columnas, si dims = 3 pone 4
        archivo << "x,y,cluster\n";
    else
        archivo << "x,y,z,cluster\n";

    for (int i = 0; i < puntos.size(); i++) // Llena el archivo con los puntos
    {
        if (puntos[i].dims == 2)
            archivo << puntos[i].x << "," << puntos[i].y << "," << puntos[i].cluster << "\n";
        else
            archivo << puntos[i].x << "," << puntos[i].y << "," << puntos[i].z << "," << puntos[i].cluster << "\n";
    }

    archivo.close();
}

int main(int argc, char *argv[])
{

    if (argc != 5) // Si no se pasan exactamente 4 argumentos, muestra como se debe de ejecutar
    {
        cerr << "Uso: ./kmeans_serial <archivo.csv> <K> <max_iter> <num_hilos>" << endl;
        cerr << "Ejemplo: ./kmeans_serial datos.csv 5 100 4" << endl;
        return 1;
    }

    string ruta_entrada = argv[1]; // primer argumento es la ruta del csv
    int K = stoi(argv[2]);         // segundo argumento es numero de clusters y convierte a entero
    int MAX_ITER = stoi(argv[3]);  // tercer argumento maximas iteraciones y convierte a entero
    int num_hilos = stoi(argv[4]); // cuarto argumento numero de hilos

    omp_set_num_threads(num_hilos);

    // imprime info y carga al archivo
    cout << "Leyendo: " << ruta_entrada << endl;
    vector<Punto> puntos = leer_csv(ruta_entrada);
    cout << "Puntos : " << puntos.size() << endl;
    cout << "K      : " << K << endl;
    cout << "MaxIter: " << MAX_ITER << endl;

    // --- Inicializar centroides ---
    vector<Punto> centroides = inicializar_centroides(puntos, K);

    // --- Medir tiempo de inicio ---
    auto t_inicio = chrono::high_resolution_clock::now();

    // --- Ciclo principal de K-Means ---
    int iteracion = 0;
    bool cambio = true;

    while (cambio && iteracion < MAX_ITER) // mientras siga habiendo cambios de clusters
    {
        cambio = asignar_clusters(puntos, centroides);
        centroides = recalcular_centroides(puntos, centroides, K);
        iteracion++;
    }

    // --- Medir tiempo de fin ---
    auto t_fin = chrono::high_resolution_clock::now();
    double tiempo_ms = chrono::duration<double, std::milli>(t_fin - t_inicio).count(); // diferencia entre final e inicial, pasa a ms.

    cout << "Iteraciones: " << iteracion << endl;
    cout << "Tiempo     : " << tiempo_ms << " ms" << endl;

    // --- Guardar resultados ---
    string ruta_salida = ruta_entrada.substr(0, ruta_entrada.find_last_of('.')) + "_clusters.csv"; // crea un csv con el mismo nombre que el de entrada y agrega "_clusters,csv" al final
    guardar_csv(puntos, ruta_salida);
    cout << "Salida     : " << ruta_salida << endl;

    return 0;
}