#include <iostream>
#include <vector>
#include <string>
#include <winsock2.h>
#include <windows.h>
#include <thread>
#include <mutex>
#include <ctime>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// Estructuras de datos
struct ClienteInfo {
    int id;
    string nombre;
    SOCKET socket;
};

struct Orden {
    string accion;
    char tipo; // 'C' para compra, 'V' para venta
    int cantidad;
    double precio;
    int clienteId;
    string clienteNombre;
    time_t fecha;
};

struct Transaccion {
    string accion;
    int cantidad;
    double precio;
    string comprador;
    string vendedor;
    time_t fecha;
};

// Variables globales
mutex mtx;
vector<ClienteInfo> clientesConectados;
vector<Orden> ordenes;
vector<Transaccion> historial;

// Funciones auxiliares
string formatoFecha(time_t tiempo) {
    char buffer[80];
    tm* tiempoLocal = localtime(&tiempo);
    strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", tiempoLocal);
    return string(buffer);
}

void notificarTransaccion(const Transaccion& trans, SOCKET socketComprador, SOCKET socketVendedor) {
    string msgComprador = "\nOPERACION EJECUTADA\nHas comprado " + to_string(trans.cantidad) + 
                         " de " + trans.accion + " a $" + to_string(trans.precio) + 
                         "\nVendedor: " + trans.vendedor + "\n";
    
    string msgVendedor = "\nOPERACION EJECUTADA\nHas vendido " + to_string(trans.cantidad) + 
                        " de " + trans.accion + " a $" + to_string(trans.precio) + 
                        "\nComprador: " + trans.comprador + "\n";
    
    send(socketComprador, msgComprador.c_str(), msgComprador.size(), 0);
    send(socketVendedor, msgVendedor.c_str(), msgVendedor.size(), 0);
}

void procesarOrden(Orden nuevaOrden, SOCKET clienteSocket) {
    lock_guard<mutex> guard(mtx);
    
    // Buscar matches
    for(auto it = ordenes.begin(); it != ordenes.end(); ) {
        if(it->accion == nuevaOrden.accion && it->tipo != nuevaOrden.tipo &&
          ((nuevaOrden.tipo == 'C' && it->precio <= nuevaOrden.precio) || 
           (nuevaOrden.tipo == 'V' && it->precio >= nuevaOrden.precio))) {
            
            int cantidadTransada = min(it->cantidad, nuevaOrden.cantidad);
            double precioTransaccion = nuevaOrden.tipo == 'C' ? it->precio : nuevaOrden.precio;
            
            // Registrar transacción
            Transaccion trans;
            trans.accion = nuevaOrden.accion;
            trans.cantidad = cantidadTransada;
            trans.precio = precioTransaccion;
            trans.comprador = nuevaOrden.tipo == 'C' ? nuevaOrden.clienteNombre : it->clienteNombre;
            trans.vendedor = nuevaOrden.tipo == 'V' ? nuevaOrden.clienteNombre : it->clienteNombre;
            trans.fecha = time(0);
            historial.push_back(trans);
            
            // Buscar sockets para notificación
            SOCKET socketComprador, socketVendedor;
            for(const auto& cliente : clientesConectados) {
                if(cliente.id == (nuevaOrden.tipo == 'C' ? nuevaOrden.clienteId : it->clienteId)) 
                    socketComprador = cliente.socket;
                if(cliente.id == (nuevaOrden.tipo == 'V' ? nuevaOrden.clienteId : it->clienteId)) 
                    socketVendedor = cliente.socket;
            }
            
            // Notificar
            notificarTransaccion(trans, socketComprador, socketVendedor);
            
            // Ajustar cantidades
            it->cantidad -= cantidadTransada;
            nuevaOrden.cantidad -= cantidadTransada;
            
            if(it->cantidad <= 0) {
                it = ordenes.erase(it);
                continue;
            }
        }
        ++it;
    }
    
    if(nuevaOrden.cantidad > 0) {
        ordenes.push_back(nuevaOrden);
        string confirmacion = "Orden registrada exitosamente\n";
        send(clienteSocket, confirmacion.c_str(), confirmacion.size(), 0);
    }
}

DWORD WINAPI manejarCliente(LPVOID lpParam) {
    SOCKET clienteSocket = (SOCKET)lpParam;
    char buffer[1024];
    int bytesRecibidos;
    
    // Recibir nombre
    bytesRecibidos = recv(clienteSocket, buffer, sizeof(buffer), 0);
    if(bytesRecibidos <= 0) {
        closesocket(clienteSocket);
        return 0;
    }
    buffer[bytesRecibidos] = '\0';
    string nombreCliente(buffer);
    
    // Registrar cliente
    int clienteId;
    {
        lock_guard<mutex> guard(mtx);
        clienteId = clientesConectados.size() + 1;
        clientesConectados.push_back({clienteId, nombreCliente, clienteSocket});
        
        // Mostrar mensaje de conexión en el servidor
        cout << "Cliente " << nombreCliente << " se ha conectado (ID: " << clienteId << ")" << endl;
    }
    
    string menu = "\n=== MENU PRINCIPAL ===\n"
                "1. Comprar acciones\n"
                "2. Vender acciones\n"
                "3. Ver ordenes de COMPRA\n"
                "4. Ver ordenes de VENTA\n"
                "5. Ver mi historial\n"
                "6. Salir\n"
                "Seleccione opcion: ";
    
    while(true) {
        send(clienteSocket, menu.c_str(), menu.size(), 0);
        
        bytesRecibidos = recv(clienteSocket, buffer, sizeof(buffer), 0);
        if(bytesRecibidos <= 0) break;
        
        string opcion(buffer, bytesRecibidos);
        
        if(opcion == "6") {
            string despedida = "Sesion finalizada. Hasta pronto\n";
            send(clienteSocket, despedida.c_str(), despedida.size(), 0);
            break;
        }
        else if(opcion == "1" || opcion == "2") {
            char tipo = opcion == "1" ? 'C' : 'V';
            
            string mensaje = "Ingrese accion y precio (ej: AAPL 150.50): ";
            send(clienteSocket, mensaje.c_str(), mensaje.size(), 0);
            
            bytesRecibidos = recv(clienteSocket, buffer, sizeof(buffer), 0);
            if(bytesRecibidos <= 0) break;
            
            string datos(buffer, bytesRecibidos);
            size_t espacio = datos.find(' ');
            if(espacio == string::npos) continue;
            
            string accion = datos.substr(0, espacio);
            double precio = stod(datos.substr(espacio+1));
            
            mensaje = "Ingrese cantidad: ";
            send(clienteSocket, mensaje.c_str(), mensaje.size(), 0);
            
            bytesRecibidos = recv(clienteSocket, buffer, sizeof(buffer), 0);
            if(bytesRecibidos <= 0) break;
            
            int cantidad = stoi(string(buffer, bytesRecibidos));
            
            Orden nuevaOrden;
            nuevaOrden.accion = accion;
            nuevaOrden.tipo = tipo;
            nuevaOrden.cantidad = cantidad;
            nuevaOrden.precio = precio;
            nuevaOrden.clienteId = clienteId;
            nuevaOrden.clienteNombre = nombreCliente;
            nuevaOrden.fecha = time(0);
            
            procesarOrden(nuevaOrden, clienteSocket);
        }
        else if(opcion == "3") {
            string libro = "=== ORDENES DE COMPRA ===\n";
            libro += "ACCION\tCANTIDAD\tPRECIO\t\tCLIENTE\n";
            libro += "----------------------------------------\n";
            {
                lock_guard<mutex> guard(mtx);
                for(const auto& orden : ordenes) {
                    if(orden.tipo == 'C') {
                        libro += orden.accion + "\t" + to_string(orden.cantidad) + "\t\t$" + 
                               to_string(orden.precio) + "\t\t" + orden.clienteNombre + "\n";
                    }
                }
            }
            send(clienteSocket, libro.c_str(), libro.size(), 0);
        }
        else if(opcion == "4") {
            string libro = "=== ORDENES DE VENTA ===\n";
            libro += "ACCION\tCANTIDAD\tPRECIO\t\tCLIENTE\n";
            libro += "----------------------------------------\n";
            {
                lock_guard<mutex> guard(mtx);
                for(const auto& orden : ordenes) {
                    if(orden.tipo == 'V') {
                        libro += orden.accion + "\t" + to_string(orden.cantidad) + "\t\t$" + 
                               to_string(orden.precio) + "\t\t" + orden.clienteNombre + "\n";
                    }
                }
            }
            send(clienteSocket, libro.c_str(), libro.size(), 0);
        }
        else if(opcion == "5") {
            string historialCliente = "=== TU HISTORIAL ===\n";
            {
                lock_guard<mutex> guard(mtx);
                for(const auto& trans : historial) {
                    if(trans.comprador == nombreCliente || trans.vendedor == nombreCliente) {
                        historialCliente += trans.accion + " " + to_string(trans.cantidad) + 
                                          " @ $" + to_string(trans.precio) + 
                                          " (" + (trans.comprador == nombreCliente ? "COMPRA" : "VENTA") + 
                                          ") " + formatoFecha(trans.fecha) + "\n";
                    }
                }
            }
            send(clienteSocket, historialCliente.c_str(), historialCliente.size(), 0);
        }
    }
    
    // Eliminar cliente
    {
        lock_guard<mutex> guard(mtx);
        clientesConectados.erase(
            remove_if(clientesConectados.begin(), clientesConectados.end(),
                [clienteId](const ClienteInfo& c) { return c.id == clienteId; }),
            clientesConectados.end());
    }
    
    closesocket(clienteSocket);
    return 0;
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET servidor = socket(AF_INET, SOCK_STREAM, 0);
    
    sockaddr_in direccion;
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;
    direccion.sin_port = htons(12345);
    
    bind(servidor, (sockaddr*)&direccion, sizeof(direccion));
    listen(servidor, SOMAXCONN);
    
    cout << "Servidor iniciado. Esperando conexiones..." << endl;
    
    while(true) {
        SOCKET cliente = accept(servidor, NULL, NULL);
        if(cliente == INVALID_SOCKET) continue;
        
        CreateThread(NULL, 0, manejarCliente, (LPVOID)cliente, 0, NULL);
    }
    
    closesocket(servidor);
    WSACleanup();
    return 0;
}
