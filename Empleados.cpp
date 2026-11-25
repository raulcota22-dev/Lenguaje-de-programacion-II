// =========================================================================
// Sistema de empleados (UNI) en C++ con conexion ODBC a SQL Server
// Lenguajes de Programacion II - Actividad 3. Ejecucion
// Base de datos: UNI
// Tablas: CentrosTrabajo, Empleados, Directivos
// =========================================================================

#define NOMINMAX             // Evita que windows.h defina las macros min/max
#include <windows.h>         // Requerido por ODBC en Windows
#include <sql.h>             // Tipos y funciones base de ODBC
#include <sqlext.h>          // Extensiones ODBC (SQL_NTS, etc.)
#pragma comment(lib, "odbc32.lib")

#include <algorithm>         // transform
#include <cctype>            // toupper
#include <iomanip>           // setw, left
#include <iostream>          // cin, cout, cerr
#include <limits>            // numeric_limits
#include <memory>            // unique_ptr, make_unique
#include <string>
#include <vector>

using namespace std;

// -------------------------------------------------------------------------
// Funciones auxiliares
// -------------------------------------------------------------------------

string aMayusculas(string texto) {
    transform(texto.begin(), texto.end(), texto.begin(),
              [](unsigned char c) { return static_cast<char>(toupper(c)); });
    return texto;
}

string primeraLetra(const string& texto) {
    return texto.empty() ? string("X") : texto.substr(0, 1);
}

string escapar(const string& texto) {
    string salida;
    for (char c : texto) {
        if (c == '\'') salida += "''";
        else salida += c;
    }
    return salida;
}

string recortar(const string& texto) {
    size_t fin = texto.find_last_not_of(' ');
    return fin == string::npos ? string() : texto.substr(0, fin + 1);
}

string leerTexto(const string& mensaje) {
    cout << mensaje;
    string linea;
    getline(cin, linea);
    return linea;
}

bool leerEntero(int& valor) {
    cin >> valor;
    if (cin.eof()) return false;
    if (cin.fail()) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return false;
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    return true;
}

// -------------------------------------------------------------------------
// CLASE DBConexion: encapsula la conexion ODBC a SQL Server
// -------------------------------------------------------------------------
class DBConexion {
private:
    SQLHENV env = SQL_NULL_HENV;
    SQLHDBC dbc = SQL_NULL_HDBC;

    static void mostrarErrorODBC(SQLSMALLINT tipo, SQLHANDLE handle) {
        SQLCHAR estado[6];
        SQLCHAR mensaje[SQL_MAX_MESSAGE_LENGTH];
        SQLINTEGER nativo = 0;
        SQLSMALLINT longitud = 0;
        for (SQLSMALLINT i = 1;
             SQLGetDiagRecA(tipo, handle, i, estado, &nativo,
                            mensaje, sizeof(mensaje), &longitud) == SQL_SUCCESS;
             ++i) {
            cerr << "[SQL " << estado << "] " << mensaje << "\n";
        }
    }

public:
    ~DBConexion() { desconectar(); }

    bool conectar(const string& cadena) {
        if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env)))
            return false;
        SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
        if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc)))
            return false;

        SQLCHAR salida[1024];
        SQLSMALLINT lonSalida = 0;
        SQLRETURN r = SQLDriverConnectA(dbc, NULL, (SQLCHAR*)cadena.c_str(), SQL_NTS,
                                        salida, sizeof(salida), &lonSalida,
                                        SQL_DRIVER_NOPROMPT);
        if (!SQL_SUCCEEDED(r)) mostrarErrorODBC(SQL_HANDLE_DBC, dbc);
        return SQL_SUCCEEDED(r);
    }

    void desconectar() {
        if (dbc != SQL_NULL_HDBC) {
            SQLDisconnect(dbc);
            SQLFreeHandle(SQL_HANDLE_DBC, dbc);
            dbc = SQL_NULL_HDBC;
        }
        if (env != SQL_NULL_HENV) {
            SQLFreeHandle(SQL_HANDLE_ENV, env);
            env = SQL_NULL_HENV;
        }
    }

    bool ejecutar(const string& sql) {
        SQLHSTMT stmt = SQL_NULL_HSTMT;
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
        SQLRETURN r = SQLExecDirectA(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);
        if (!SQL_SUCCEEDED(r)) mostrarErrorODBC(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return SQL_SUCCEEDED(r);
    }

    vector<vector<string>> consultar(const string& sql) {
        vector<vector<string>> filas;
        SQLHSTMT stmt = SQL_NULL_HSTMT;
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
        SQLRETURN r = SQLExecDirectA(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);
        if (SQL_SUCCEEDED(r)) {
            SQLSMALLINT columnas = 0;
            SQLNumResultCols(stmt, &columnas);
            while (SQL_SUCCEEDED(SQLFetch(stmt))) {
                vector<string> fila;
                for (SQLUSMALLINT c = 1; c <= columnas; ++c) {
                    char buffer[256];
                    SQLLEN indicador = 0;
                    SQLGetData(stmt, c, SQL_C_CHAR, buffer, sizeof(buffer), &indicador);
                    fila.push_back(indicador == SQL_NULL_DATA ? string("") : string(buffer));
                }
                filas.push_back(fila);
            }
        } else {
            mostrarErrorODBC(SQL_HANDLE_STMT, stmt);
        }
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return filas;
    }
};

// Revisa que el centro exista en dbo.CentrosTrabajo
bool centroExiste(DBConexion& db, const string& codigo) {
    return !db.consultar("SELECT numero_centro FROM CentrosTrabajo WHERE numero_centro = '"
                         + escapar(codigo) + "'").empty();
}

// -------------------------------------------------------------------------
// CLASE BASE: Empleado
// -------------------------------------------------------------------------
class Empleado {
protected:
    int numeroEmpleado = 0;
    string nombre;
    string apellidoPaterno;
    string apellidoMaterno;
    string fechaNacimiento;       // Formato AAAA-MM-DD
    string rfc;
    string numeroCentro;          // ej. 000201
    string puesto;
    string descripcionPuesto;
    bool esDirectivo = false;

public:
    Empleado(string nom, string ap, string am, string fn, string centro,
             string pto, string descPuesto)
        : nombre(nom), apellidoPaterno(ap), apellidoMaterno(am),
          fechaNacimiento(fn), numeroCentro(centro), puesto(pto),
          descripcionPuesto(descPuesto) {
        rfc = calcularRFC();
    }

    virtual ~Empleado() {}

    string calcularRFC() const {
        string ap = aMayusculas(apellidoPaterno);
        string am = aMayusculas(apellidoMaterno);
        string no = aMayusculas(nombre);

        string vocal = "X";
        for (size_t i = 1; i < ap.size(); ++i) {
            if (string("AEIOU").find(ap[i]) != string::npos) {
                vocal = string(1, ap[i]);
                break;
            }
        }

        string aa = fechaNacimiento.substr(2, 2);
        string mm = fechaNacimiento.substr(5, 2);
        string dd = fechaNacimiento.substr(8, 2);

        return primeraLetra(ap) + vocal + primeraLetra(am) + primeraLetra(no)
               + aa + mm + dd + "XXX";
    }

    virtual bool guardar(DBConexion& db) {
        string sql =
            "INSERT INTO Empleados (nombre, apellido_paterno, apellido_materno, "
            "fecha_nacimiento, rfc, numero_centro, puesto, descripcion_puesto, es_directivo) "
            "OUTPUT INSERTED.numero_empleado "
            "VALUES ('" + escapar(nombre) + "','"
            + escapar(apellidoPaterno) + "','"
            + escapar(apellidoMaterno) + "','"
            + fechaNacimiento + "','"
            + rfc + "','"
            + numeroCentro + "','"
            + escapar(puesto) + "','"
            + escapar(descripcionPuesto) + "',"
            + (esDirectivo ? "1" : "0") + ")";

        auto f = db.consultar(sql);
        if (f.empty()) return false;
        numeroEmpleado = stoi(f[0][0]);
        return true;
    }

    int getNumeroEmpleado() const { return numeroEmpleado; }
    string getRFC() const { return rfc; }
};

// -------------------------------------------------------------------------
// CLASE DERIVADA: Directivo 
// -------------------------------------------------------------------------
class Directivo : public Empleado {
private:
    string centroSupervisa;
    bool prestacionCombustible;

public:
    Directivo(string nom, string ap, string am, string fn, string centro,
              string pto, string descPuesto, string centroSup, bool combustible)
        : Empleado(nom, ap, am, fn, centro, pto, descPuesto),
          centroSupervisa(centroSup),
          prestacionCombustible(combustible) {
        esDirectivo = true;
    }

    bool guardar(DBConexion& db) override {
        if (!Empleado::guardar(db)) return false;
        string sql =
            "INSERT INTO Directivos (numero_empleado, centro_supervisa, prestacion_combustible) "
            "VALUES (" + to_string(numeroEmpleado) + ",'"
            + centroSupervisa + "',"
            + (prestacionCombustible ? "1" : "0") + ")";
        return db.ejecutar(sql);
    }
};

// -------------------------------------------------------------------------
// Captura de un empleado desde consola
// -------------------------------------------------------------------------
unique_ptr<Empleado> capturarEmpleado(DBConexion& db) {
    string nombre = leerTexto("Nombre: ");
    string ap = leerTexto("Apellido paterno: ");
    string am = leerTexto("Apellido materno: ");
    string fecha = leerTexto("Fecha de nacimiento (AAAA-MM-DD): ");
    if (fecha.size() != 10) {
        cout << "Formato de fecha invalido.\n";
        return nullptr;
    }

    string centro = leerTexto("Centro de trabajo (ej. 000201): ");
    if (centro.size() != 6 || !centroExiste(db, centro)) {
        cout << "El centro de trabajo no existe en el catalogo CentrosTrabajo.\n";
        return nullptr;
    }

    string puesto = leerTexto("Puesto (ej. Vendedor, Gerente): ");
    string descPuesto = leerTexto("Descripcion del puesto: ");

    int esDir = 0;
    cout << "Es directivo? (1=Si, 0=No): ";
    if (!leerEntero(esDir)) {
        cout << "Respuesta invalida.\n";
        return nullptr;
    }

    unique_ptr<Empleado> emp;
    if (esDir == 1) {
        string centroSup = leerTexto("Numero del centro que supervisa: ");
        if (centroSup.size() != 6 || !centroExiste(db, centroSup)) {
            cout << "El centro que supervisa no existe en el catalogo.\n";
            return nullptr;
        }
        int combustible = 0;
        cout << "Prestacion de combustible (1=Si, 0=No): ";
        if (!leerEntero(combustible)) {
            cout << "Respuesta invalida.\n";
            return nullptr;
        }
        emp = make_unique<Directivo>(nombre, ap, am, fecha, centro, puesto, descPuesto,
                                     centroSup, combustible == 1);
    } else {
        emp = make_unique<Empleado>(nombre, ap, am, fecha, centro, puesto, descPuesto);
    }

    cout << "RFC calculado: " << emp->getRFC() << "\n";
    return emp;
}

// -------------------------------------------------------------------------
// Reporte tabular desde SQL Server
// -------------------------------------------------------------------------
void imprimirReporte(DBConexion& db) {
    auto filas = db.consultar(
        "SELECT e.numero_empleado, "
        "RTRIM(e.nombre) + ' ' + RTRIM(e.apellido_paterno) + ' ' + RTRIM(e.apellido_materno), "
        "CONVERT(VARCHAR(10), CAST(RTRIM(e.fecha_nacimiento) AS DATE), 103), "
        "RTRIM(e.rfc), c.nombre_centro, RTRIM(e.puesto), "
        "CASE WHEN e.es_directivo = 1 THEN 'Si' ELSE 'No' END "
        "FROM Empleados e "
        "INNER JOIN CentrosTrabajo c ON c.numero_centro = e.numero_centro "
        "ORDER BY e.numero_empleado");

    cout << "\n" << left
         << setw(12) << "No. Emp."
         << setw(34) << "Nombre Completo"
         << setw(18) << "Fecha Nacimiento"
         << setw(16) << "RFC"
         << setw(30) << "Nombre de Centro"
         << setw(18) << "Puesto"
         << "Es Directivo?\n";
    cout << string(142, '-') << "\n";

    for (const auto& f : filas) {
        cout << setw(12) << f[0]
             << setw(34) << f[1]
             << setw(18) << f[2]
             << setw(16) << f[3]
             << setw(30) << f[4]
             << setw(18) << f[5]
             << f[6] << "\n";
    }
    cout << "Total de empleados: " << filas.size() << "\n";
}

// -------------------------------------------------------------------------
// Programa principal: menu de captura y reporte
// -------------------------------------------------------------------------
int main() {
    DBConexion db;

  
   const string cadena =
        "DRIVER={ODBC Driver 17 for SQL Server};"
        "SERVER=WIN-GALFN6F7I32\\SQLEXPRESS;"
        "DATABASE=UNI;"
        "Trusted_Connection=yes;";

    if (!db.conectar(cadena)) {
        cout << "No fue posible conectar con SQL Server.\n";
        return 1;
    }

    cout << "Conexion exitosa a la base de datos UNI.\n";
    int opcion = -1;
    do {
        cout << "\n=== MENU ===\n"
             << "1) Capturar empleado\n"
             << "2) Mostrar reporte\n"
             << "0) Salir\n"
             << "Opcion: ";
        opcion = -1;
        if (!leerEntero(opcion)) {
            if (cin.eof()) break;
            cout << "Opcion invalida.\n";
            continue;
        }

        if (opcion == 1) {
            unique_ptr<Empleado> emp = capturarEmpleado(db);
            if (emp && emp->guardar(db))
                cout << "Empleado guardado con numero " << emp->getNumeroEmpleado() << ".\n";
            else
                cout << "No se pudo guardar el empleado.\n";
        } else if (opcion == 2) {
            imprimirReporte(db);
        } else if (opcion != 0) {
            cout << "Opcion invalida.\n";
        }
    } while (opcion != 0);

    return 0;
}