#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <iostream>

int main() {
    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt;

    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

    SQLCHAR conn[] =
        "DRIVER={ODBC Driver 17 for SQL Server};"
        "SERVER=WIN-GALFN6F7I32\\SQLEXPRESS;"
        "DATABASE=UNI;Trusted_Connection=yes;";

    SQLRETURN ret = SQLDriverConnect(dbc, NULL, conn, SQL_NTS,
                                     NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(ret)) {
        std::cout << "Error\n";
        return 1;
    }
    std::cout << "Conexion exitosa\n";

    // Consultas
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    SQLExecDirect(stmt, (SQLCHAR*)"SELECT TOP 0 * FROM Empleados", SQL_NTS);

    SQLSMALLINT cols = 0;
    SQLNumResultCols(stmt, &cols);
    std::cout << "Columnas: " << cols << "\n";

    for (int i = 1; i <= cols; i++) {
        SQLCHAR name[128] = {0};
        SQLSMALLINT len = 0;
        SQLRETURN r = SQLDescribeCol(stmt, i, name, sizeof(name),
                                     &len, NULL, NULL, NULL, NULL);
        if (SQL_SUCCEEDED(r))
            std::cout << (char*)name << "\t";
        else
            std::cout << "[error col " << i << "]\t";
    }
    std::cout << "\n";
    // EndConsulta

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    return 0;
}