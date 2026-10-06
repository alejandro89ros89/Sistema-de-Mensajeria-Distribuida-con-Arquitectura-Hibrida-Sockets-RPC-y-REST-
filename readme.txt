# Para ejecutar la practica haremos lo siguiente: 
make clean (para borrar posibles archivos antiguos)
make 
# 1. Ejecutamos el servidor 
./registro_server
# 2. Ejecutamos el servicio web 
python3 ServicioWeb.py
# 3. Ejecutamos el servidor RPC 
export LOG_RPC_IP=127.0.0.1
./servidor -p 8080
# 4. Ejecutamos los clientes que queramos 
python3 Cliente/Cliente.py -s 127.0.0.1 -p 8080
    