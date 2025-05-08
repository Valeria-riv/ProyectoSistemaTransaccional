#include <iostream>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

void limpiarPantalla() {
    system("cls");
}

void mostrarEncabezado(const string& nombre) {
    cout << "===================================\n";
    cout << "| Sistema de Trading - Usuario: " << nombre << " |\n";
    cout << "===================================\n";
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET cliente = socket(AF_INET, SOCK_STREAM, 0);
    
    sockaddr_in direccion;
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = inet_addr("127.0.0.1");
    direccion.sin_port = htons(12345);
    
    if(connect(cliente, (sockaddr*)&direccion, sizeof(direccion)) == SOCKET_ERROR) {
        cerr << "Error al conectar con el servidor\n";
        return 1;
    }
    
    cout << "Ingrese su nombre: ";
    string nombre;
    getline(cin, nombre);
    send(cliente, nombre.c_str(), nombre.size(), 0);
    
    char buffer[1024];
    int bytesRecibidos;
    
    while(true) {
        limpiarPantalla();
        mostrarEncabezado(nombre);
        
        bytesRecibidos = recv(cliente, buffer, sizeof(buffer), 0);
        if(bytesRecibidos <= 0) break;
        
        cout << string(buffer, bytesRecibidos);
        
        if(string(buffer, bytesRecibidos).find("Seleccione opcion:") != string::npos) {
            string opcion;
            cout << "> ";
            getline(cin, opcion);
            send(cliente, opcion.c_str(), opcion.size(), 0);
            
            if(opcion == "1" || opcion == "2") {
                // Recibir solicitud de datos
                bytesRecibidos = recv(cliente, buffer, sizeof(buffer), 0);
                if(bytesRecibidos <= 0) break;
                cout << string(buffer, bytesRecibidos) << "> ";
                
                string datos;
                getline(cin, datos);
                send(cliente, datos.c_str(), datos.size(), 0);
                
                // Recibir solicitud de cantidad
                bytesRecibidos = recv(cliente, buffer, sizeof(buffer), 0);
                if(bytesRecibidos <= 0) break;
                cout << string(buffer, bytesRecibidos) << "> ";
                
                string cantidad;
                getline(cin, cantidad);
                send(cliente, cantidad.c_str(), cantidad.size(), 0);
                
                // Recibir confirmación o notificación
                bytesRecibidos = recv(cliente, buffer, sizeof(buffer), 0);
                if(bytesRecibidos > 0) {
                    cout << string(buffer, bytesRecibidos);
                    system("pause");
                }
            }
            else if(opcion == "6") {
                bytesRecibidos = recv(cliente, buffer, sizeof(buffer), 0);
                cout << string(buffer, bytesRecibidos);
                break;
            }
            else if(opcion == "3" || opcion == "4" || opcion == "5") {
                // Mostrar información recibida para órdenes o historial
                bytesRecibidos = recv(cliente, buffer, sizeof(buffer), 0);
                if(bytesRecibidos > 0) {
                    cout << string(buffer, bytesRecibidos);
                    system("pause");
                }
            }
        } else {
            system("pause");
        }
    }
    
    closesocket(cliente);
    WSACleanup();
    return 0;
}