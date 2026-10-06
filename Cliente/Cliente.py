from enum import Enum
import argparse
import sys
import socket
import threading

#OJO IMPORTANTE, TENGO QUE AÑADIR EL QUIT HAY QUE ACORDARSE
class client :

    # ******************** TYPES *********************
    # *
    # * @brief Return codes for the protocol methods
    class RC(Enum) :
        OK = 0
        ERROR = 1
        USER_ERROR = 2
    
        

    # ****************** ATTRIBUTES ******************
    _server = None
    _port = -1
    # Añadimos atributo
    _user = None         # Nombre del usuario actual
    _connected = False   # Estado de la conexión
    #Parte2: tenemos que tener un diccionario de los usuarios conectados 
    _users = {}

    # ******************** METHODS *******************
    # *


    # Funcion usada en viugilante para recibir cadenas y refactorizar el codigo

    #OJO, devuelve strings 
    @staticmethod
    def RecibirCadena(socket_con):
        cadena = ""
        a=True
        while a==True:
            char_byte = socket_con.recv(1)
            # Si el byte es nulo o no hay más datos, paramos
            if not char_byte or char_byte == b'\x00':
                 #a=False
                 break
                 #Si ponemos false la el \x00 entra dento de la cadena en la linea de abajo 
            cadena += char_byte.decode()  # LO PASAMOS A STRING 
        return cadena



# Funcion que esta pendiente de la escucha siempre activa en el thread
    @staticmethod
    def Vigilante(socket_escucha):
        while True:
            try:
                # El hilo se queda "durmiendo" aqui hasta que llega una peticion del servidor
                Conexion, Direccion = socket_escucha.accept()
                
                # Leemos la operacion que nos ha pasado el servidor
                Operacion = client.RecibirCadena(Conexion)
                
                if Operacion == "SEND_MESSAGE":
                    # Ya no hay bucle for ni num_mensajes. Leemos directamente:
                    Remitente = client.RecibirCadena(Conexion)
                    IdMensaje = client.RecibirCadena(Conexion)
                    Contenido = client.RecibirCadena(Conexion)
                    # Formato exacto que pide la Parte 1
                    print(f"\ns> MESSAGE {IdMensaje} FROM {Remitente}")
                    print(f"{Contenido}")
                    print("END\nc> ", end="") # Ponemos el c> para que la consola no quede fea
                    
                elif Operacion == "SEND_MESSAGE_ATTACH":
                    # Leemos los datos igual, pero anadiendo el nombre del fichero al final
                    Remitente = client.RecibirCadena(Conexion)
                    IdMensaje = client.RecibirCadena(Conexion)
                    Contenido = client.RecibirCadena(Conexion)
                    NombreFichero = client.RecibirCadena(Conexion)
                    
                    # Formato exacto que pide la Parte 2 para ficheros
                    print(f"\ns> MESSAGE {IdMensaje} FROM {Remitente}")
                    print(f"{Contenido}")
                    print("END")
                    print(f"FILE {NombreFichero}\nc> ", end="")
                #Gestion de los ACK
                elif Operacion == "SEND_MESS_ACK":
                    IdMensaje = client.RecibirCadena(Conexion)
                    print(f"\nc> SEND MESSAGE {IdMensaje} OK")
                    print("c> ", end="", flush=True)

                elif Operacion == "SEND_MESS_ATTACH_ACK":
                    IdMensaje = client.RecibirCadena(Conexion)
                    NombreFichero = client.RecibirCadena(Conexion)
                    print(f"\nc> SENDATTACH MESSAGE {IdMensaje} {NombreFichero} OK")
                    print("c> ", end="", flush=True)
                elif Operacion == "GET FILE":
                    # Leemos quien nos lo pide y que archivo quiere
                    Remitente = client.RecibirCadena(Conexion)
                    NombreFichero = client.RecibirCadena(Conexion)
                    
                    try:
                        # Abrimos el archivo de nuestro ordenador en modo lectura de bytes ('rb')
                        with open(NombreFichero, 'rb') as f:
                            while True:
                                # Leemos un trozo del archivo
                                BytesLeidos = f.read(1024)
                                if not BytesLeidos:
                                    break # Ya no hay mas archivo
                                # Se lo enviamos al compañero
                                Conexion.sendall(BytesLeidos)
                    except Exception as e:
                        print("\nc> FILE TRANSFER FAILED\nc> ", end="")
                
                # Cerramos la conexion de este mensaje en concreto
                Conexion.close()
                
            except Exception as e:
                # Si hay un error o cerramos el socket, el hilo termina
                socket_escucha.close()
                break

    # * @param user - User name to register in the system
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user is already registered
    # * @return ERROR if another error occurred
    @staticmethod
    def  register(user) :
        try:
            # 1. Creamos el socket TCP
            # AF_INET es para IPv4 y SOCK_STREAM para TCP
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            
            # 2. Conectamos al servidor (usando los datos que guardamos en el main)
            s.connect((client._server, client._port))
            
            # 3. Enviamos la operacion, en el caso de Python no añadira caracter nulo al final, lo metemos nosotros 
            operation = "REGISTER\0"
            s.sendall(operation.encode())
            
            # 4. Enviamos el nombre de usuario
            s.sendall(f"{user}\0".encode())
            
            # 5. Recibimos 1 solo bye y por lo tanto ponemos recv(1) 
            # En un futuro cuando enviemos y recibamos elementos mas grandes tenemos que variar ese numero 
            ans = s.recv(1)
            
            if not ans:
                return client.RC.ERROR
            # Recibimos la informacion y la manejamos
           # texto = ans.decode()  # La pasamos a string
            #res_code = int(texto) # Lo pasamos a int 
            #OJO; lo que haciamos antes estaba mal porque ans es un byte y te da el valor int directamente 
            res_code = ans[0]
                        
            # 6. Interpretamos según tu protocolo en C
            if res_code == 0:
                print("c> REGISTER OK")
                return client.RC.OK
            elif res_code == 1:
                print("c> USERNAME IN USE")
                return client.RC.USER_ERROR
            else:
                print("c> REGISTER FAIL")
                return client.RC.ERROR

        except Exception as e:
            print("c> REGISTER FAIL")
            return client.RC.ERROR
        finally:
            #Cerramos siempre el socket
            s.close()

    # *
    # 	 * @param user - User name to unregister from the system
    # 	 * 
    # 	 * @return OK if successful
    # 	 * @return USER_ERROR if the user does not exist
    # 	 * @return ERROR if another error occurred
    @staticmethod
    def  unregister(user) :
       # Ahora me da igual si el usuario esta o no activo  o no?
        s =None
        try:   
            #1. Creamos el server en TCP 
            s=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))
            #Enviamos los mensajes 
            s.sendall("UNREGISTER\0".encode())
            s.sendall(f"{user}\0".encode())

            # Recibimos el codigo de respuesta 
            ans = s.recv(1)
            
            if not ans:
                return client.RC.ERROR
            # Recibimos la informacion y la manejamos
            res_code=ans[0]
            #texto = ans.decode()  # La pasamos a string
            #res_code = int(texto) # Lo pasamos a int 
                        
            # 6. Interpretamos según tu protocolo en C
            if res_code == 0:
                print("c> UNREGISTER OK")
                return client.RC.OK
            elif res_code == 1:
                print("c> USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            else:
                print("c> UNREGISTER FAIL")
                return client.RC.ERROR

        except Exception as e:
            print("c> UNREGISTER FAIL")
            return client.RC.ERROR
        finally:
            #Cerramos siempre el socket
            s.close()


    # *
    # * @param user - User name to connect to the system
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred
   # *
    # * @param user - User name to connect to the system
    # * # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred
    @staticmethod
    def  connect(user) :
        try: 
            #Creeamos nuestropropio de escucha 
            s_escucha= socket.socket(socket.AF_INET, socket.SOCK_STREAM) 
            #Python pide elq ue este libre con el 0 
            s_escucha.bind(("",0))
            s_escucha.listen(1)
            
            #Cogemos que puerto nos ha pasado el sistema 
            mi_puerto=s_escucha.getsockname()[1]

            # MODIFICAICON LANZAMOS EL HILO AQUÍ PARA EVITAR CONNECTION REFUSED 
            # LANZAMOS EL HILO AQUII 
            hilo = threading.Thread(target=client.Vigilante, args=(s_escucha,))
            # daemon=True: si cerramos el programa, el hilo se cierra solo
            hilo.daemon = True 
            hilo.start()
            
            #Reabrimos el socket de escucha del server y enviamos la informacion
            s_principal=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s_principal.connect((client._server, client._port))

            # Enviamos 
            s_principal.sendall("CONNECT\0".encode())
            s_principal.sendall(f"{user}\0".encode())
            s_principal.sendall(f"{mi_puerto}\0".encode())

            # Recibimos la respuesta 
            ans = s_principal.recv(1) 
            if not ans:
                s_escucha.close()
                s_principal.close()
                return client.RC.ERROR
            # ans es un byte ya, entonces no hay que transformarlo ya 
            # Recibimos la informacion y la manejamos
            #texto = ans.decode()  # La pasamos a string
            #res_code = int(texto) # Lo pasamos a int 
            #OTRA VEZ ESTE PROBLEMA 
            res_code = ans[0]
            if res_code == 0:
                print("c> CONNECT OK")
                # Aquí es donde más adelante lanzaremos el hilo con s_escucha
                client._connected = True
                client._user = user
                s_principal.close()
                return client.RC.OK
            elif res_code == 1:
                print("c> CONNECT FAIL, USER DOES NOT EXIST")
                s_escucha.close()
                s_principal.close()
                return client.RC.USER_ERROR
            elif res_code == 2:
                print("c> USER ALREADY CONNECTED")
                s_escucha.close()
                s_principal.close()
                return client.RC.USER_ERROR
            else:
                print("c> CONNECT FAIL")
                s_escucha.close()
                s_principal.close()
                return client.RC.USER_ERROR
        except Exception as e:
            print("c> CONNECT FAIL")
            return client.RC.ERROR

        # OJO, hay que pasar el perto que estaescuchando 
        #cuando mande el mensaje tenemso que tener un hilo que escuche 
        #  Write your code here
        return client.RC.ERROR
    # *
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred
    @staticmethod
    def  users() :
        # Comprobamos si nuestro cliente esta conectado 
        if not client._connected:
            print("c> CONNECTED USERS FAIL, USER IS NOT CONNECTED")
            return client.RC.USER_ERROR
        s = None 
        try: 
            #Lo primero, creo el socket tcp 
            s=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))
            #Enviamos la informacion 
            s.sendall("USERS\0".encode())
            #  El servidor espera un nombre para su readLine inicial
            s.sendall(f"{client._user}\0".encode()) 
            #Recibimos la informacion
            ans = s.recv(1)
            if not ans:
                return client.RC.ERROR

            # Recibimos la informacion y la manejamos
            #texto = ans.decode()  # La pasamos a string
            #res_code = int(texto) # Lo pasamos a int 
            res_code = ans[0]

            if res_code == 0:
                #Recibimos los parametrros 
                # Como son cadenas largas en lugar de un byte usamos nuestra funcion
                num_usuarios_str = client.RecibirCadena(s)
                num_usuarios = int(num_usuarios_str)
                print(f"c> CONNECTED USERS ({num_usuarios} users connected) OK")
                # Bucle para recibir los nombres
                for i in range(num_usuarios):
                    CadenaInfo = client.RecibirCadena(s)
                    PartesInfo = CadenaInfo.split(":")
                    
                    if len(PartesInfo) >= 1:
                        user = PartesInfo[0]
                        print(f"{user}")  # Solo imprimimos el nombre
                        
                        # Guardamos en el diccionario si viene completo
                        if len(PartesInfo) == 3:
                            ip = PartesInfo[1]
                            puerto = int(PartesInfo[2])
                            client._users[user] = {"ip": ip, "puerto": puerto}
                return client.RC.OK
            else:
                print("c> CONNECTED USERS FAIL")
                return client.RC.USER_ERROR
        except Exception as e:
            print("c> CONNECTED USERS FAIL")
            return client.RC.ERROR
        finally: 
            if s is not None:
                s.close()
        



    # *
    # * @param user - User name to disconnect from the system
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist
    # * @return ERROR if another error occurred
    @staticmethod
    def  disconnect(user) :
        s= None
        try: 
            #Creamos els ocket y nos conectamso 
            s=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))
            # Enviamos la inforamcion 
            s.sendall("DISCONNECT\0".encode())
            s.sendall(f"{user}\0".encode())
            #Recibimos la informacion 
            ans = s.recv(1)
            if not ans:
                return client.RC.ERROR
            # Recibimos la informacion y la manejamos
            #texto = ans.decode()  # La pasamos a string
            #res_code = int(texto) # Lo pasamos a int 
            res_code=ans[0]
            if res_code == 0:
                print("c> DISCONNECT OK")
                client._connected = False # Actualizamos el estado interno
                return client.RC.OK
            elif res_code == 1:
                print("c> DISCONNECT FAIL, USER DOES NOT EXIST") 
                return client.RC.USER_ERROR
            elif res_code == 2:
                print("c> DISCONNECT FAIL, USER NOT CONNECTED") 
                return client.RC.USER_ERROR
            else:
                print("c> DISCONNECT FAIL")
                return client.RC.USER_ERROR
        except Exception as e:
            print("c> DISCONNECT FAIL")
            return client.RC.ERROR
        finally: 
            if s is not None:
                s.close()

    # *
    # * @param user    - Receiver user name
    # * @param message - Message to be sent
    # * 
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def  send(user,  message) :
        #Lo primero comprobamos si el cliente esta conectado 
        if not client._connected:
            print("c> SEND FAIL")
            return client.RC.USER_ERROR
        MensajeLimpio = client.LlamarServicioWeb(message)
        s=None
        try: 
            #Lo primero, creo el socket tcp 
            s=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            # Creamos el socket creado al de escucha del server 
            s.connect((client._server, client._port))
            #Enviamos la operacion 
            operation="SEND\0"  
            #Codificamos a ASCII
            s.sendall(operation.encode())
            #Enviamos mi nombre 
            s.sendall(f"{client._user}\0".encode())
            #Enviamos el nombre del receptor   
            s.sendall(f"{user}\0".encode())
            #Enviamos el mensaje 
            s.sendall(f"{MensajeLimpio}\0".encode())
            #Recibimos la respuesta 
            #Podriamos usar la funcion que henmos usado de RecibirCadena 
            #Pero para un byte lo vemos algo inutil 
            ans = s.recv(1)
            if not ans:
                s.close()
                return client.RC.ERROR
            # Recibimos la informacion y la manejamos
            #texto = ans.decode()  # La pasamos a string
            #res_code = int(texto) # Lo pasamos a int 
            res_code = ans[0]
            if res_code == 0:
                # Leemos el ID que manda el servidor
                id_mensaje = client.RecibirCadena(s)
                print(f"c> SEND OK - MESSAGE {id_mensaje}")
                s.close()
                return client.RC.OK
            else:
                print("c> SEND FAIL")
                s.close()
                return client.RC.USER_ERROR
        except Exception as e:
            print("c> SEND FAIL")
            #Como no se si se ha llegado ac rear el socket 
            if not s is None:
                s.close()
            return client.RC.ERROR



    # *
    # * @param user    - Receiver user name
    # * @param file    - file  to be sent
    # * @param message - Message to be sent
    # * 
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def SendAttach(user, file, message):
        # Comprobamos si el cliente esta conectado
        if not client._connected:
            print("c> SENDATTACH FAIL")
            return client.RC.USER_ERROR
        MensajeLimpio = client.LlamarServicioWeb(message)
        SocketEnvio = None
        try:
            # Creamos el socket TCP y nos conectamos
            SocketEnvio = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            SocketEnvio.connect((client._server, client._port))
            
            # 1. Enviamos la operacion
            Operacion = "SENDATTACH\0"
            SocketEnvio.sendall(Operacion.encode())
            # 2. Enviamos nuestro nombre (remitente)
            SocketEnvio.sendall(f"{client._user}\0".encode())
            
            # 3. Enviamos el destinatario
            SocketEnvio.sendall(f"{user}\0".encode())
            
            # 4. Enviamos el mensaje
            SocketEnvio.sendall(f"{MensajeLimpio}\0".encode())
            
            # 5. Enviamos el nombre del fichero adjunto
            SocketEnvio.sendall(f"{file}\0".encode())
            
            # 6. Recibimos la respuesta usando la funcion auxiliar
            ans = SocketEnvio.recv(1)
            CodigoRespuesta = ans[0]
            
            # 7. Manejamos los resultados
            if CodigoRespuesta == 0:
                # Si es un 0, el servidor nos manda tambien el ID del mensaje
                IdMensaje = client.RecibirCadena(SocketEnvio)
                print(f"c> SENDATTACH OK - MESSAGE {IdMensaje}")
                return client.RC.OK

            elif CodigoRespuesta == 1:
                print("c> SENDATTACH FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
                
            else:
                print("c> SENDATTACH FAIL")
                return client.RC.ERROR
                
        except Exception as e:
            print("c> SENDATTACH FAIL")
            return client.RC.ERROR
            
        finally:
            # Cerramos siempre el socket al terminar
            if SocketEnvio is not None:
                SocketEnvio.close()
    # *
    # **
    # * @brief Command interpreter for the client. It calls the protocol functions.
    @staticmethod
    def shell():

        while (True) :
            try :
                command = input("c> ")
                line = command.split(" ")
                if (len(line) > 0):

                    line[0] = line[0].upper()

                    if (line[0]=="REGISTER") :
                        if (len(line) == 2) :
                            client.register(line[1])
                        else :
                            print("Syntax error. Usage: REGISTER <userName>")

                    elif(line[0]=="UNREGISTER") :
                        if (len(line) == 2) :
                            client.unregister(line[1])
                        else :
                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif(line[0]=="CONNECT") :
                        if (len(line) == 2) :
                            client.connect(line[1])
                        else :
                            print("Syntax error. Usage: CONNECT <userName>")

                    elif(line[0]=="DISCONNECT") :
                        if (len(line) == 2) :
                            client.disconnect(line[1])
                        else :
                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif(line[0]=="USERS") :
                        if (len(line) == 1) :
                            client.users()
                        else :
                            print("Syntax error. Usage: CONNECTED_USERS <userName>")

                    elif(line[0]=="SEND") :
                        if (len(line) >= 3) :
                            #  Remove first two words
                            message = ' '.join(line[2:])
                            client.send(line[1], message)
                        else :
                            print("Syntax error. Usage: SEND <userName> <message>")

                    elif(line[0]=="SENDATTACH") :
                        if (len(line) >= 4) :
                            # El fichero siempre será la ÚLTIMA palabra
                            fichero = line[-1]
                            # El mensaje será todo lo que hay entre el usuario y el fichero
                            message = ' '.join(line[2:-1])
                            client.SendAttach(line[1], fichero, message)
                        else :
                            print("Syntax error. Usage: SENDATTACH <userName> <message> <filename>")
                    #Propio de la part 2, para descargar archivos
                    elif(line[0]=="GETFILE"):
                        # Comando: GETFILE <userName> <fileName> <localFileName>
                        if (len(line) >= 4):
                            UsuarioDestino = line[1]
                            ArchivoRemoto = line[2]
                            # Como el nombre local podria tener espacios, unimos el resto de la linea
                            ArchivoLocal = ' '.join(line[3:])
                            client.GetFile(UsuarioDestino, ArchivoRemoto, ArchivoLocal)
                        else:
                            print("Syntax error. Usage: GETFILE <userName> <fileName> <localFileName>")
                            
                    elif(line[0]=="QUIT") :
                        if (len(line) == 1) :
                            # MODIFICADO PARA DESCONECTARNOS
                            if client._connected:
                                client.disconnect(client._user)
                            break
                        else :
                            print("Syntax error. Use: QUIT")
                    else :
                        print("Error: command " + line[0] + " not valid.")
            except Exception as e:
                print("Exception: " + str(e))

    # *
    # * @brief Prints program usage
    @staticmethod
    def usage() :
        print("Usage: python3 client.py -s <server> -p <port>")


    # *
    # * @brief Parses program execution arguments
    @staticmethod
    def  parseArguments(argv) :
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        args = parser.parse_args()

        # GUARDAR EN LA CLASE (Usa client. delante)
        client._server = args.s
        client._port = args.p

        if ((client._port < 1024) or (client._port > 65535)):
            print("Error: Port must be in the range 1024 <= port <= 65535")
            return False
        
        return True

    #FUNCION DE LA PARTE 2
    @staticmethod
    def GetFile(user, remote_file, local_file):
        # 1. Comprobamos si tenemos la IP y puerto del usuario en el diccionario
        if user not in client._users:
            print("c> FILE TRANSFER FAILED, user not connected.") # Mensaje exacto que pide el PDF
            return client.RC.ERROR
            
        IpDestino = client._users[user]["ip"]
        PuertoDestino = client._users[user]["puerto"]
        
        SocketP2P = None
        try:
            # 2. Nos conectamos DIRECTAMENTE al otro cliente Python
            SocketP2P = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            SocketP2P.connect((IpDestino, PuertoDestino))
            
            # 3. Enviamos la operacion "GET FILE"
            SocketP2P.sendall("GET FILE\0".encode())
            
            # 4. Enviamos nuestro nombre
            SocketP2P.sendall(f"{client._user}\0".encode())
            
            # 5. Enviamos el nombre del fichero que queremos
            SocketP2P.sendall(f"{remote_file}\0".encode())
            
            # 6. Recibimos los bytes del archivo y los guardamos
            with open(local_file, 'wb') as f:
                while True:
                    # Leemos en bloques de 1024 bytes
                    Datos = SocketP2P.recv(1024)
                    if not Datos:
                        break # Si no hay mas datos, el otro cerro la conexion (fin del archivo)
                    f.write(Datos)
                    
            print(f"Descarga de {remote_file} completada con exito en {local_file}")
            return client.RC.OK
            
        except Exception as e:
            print("c> FILE TRANSFER FAILED")
            return client.RC.ERROR
            
        finally:
            if SocketP2P is not None:
                SocketP2P.close()

    @staticmethod
    def LlamarServicioWeb(mensaje):
        try:
            import urllib.request
            # La URL donde escucha nuestro script de arriba
            Url = "http://localhost:8000"
            Datos = mensaje.encode('utf-8')
            
            # Preparamos la peticion POST
            Peticion = urllib.request.Request(Url, data=Datos, method='POST')
            
            # Hacemos la llamada y leemos el resultado limpio
            with urllib.request.urlopen(Peticion) as Respuesta:
                return Respuesta.read().decode('utf-8')
        except Exception:
            # Si el servicio web no esta encendido, devolvemos el original
            # para que la practica no se pare, aunque no limpiara espacios
            return mensaje

    # ******************** MAIN *********************
    @staticmethod
    def main(argv) :
        if (not client.parseArguments(argv)) :
            client.usage()
            return
        #Prrimero extraemos de la linea de comando el IP y el puerto usando la funcion parseArguments 
        #Ademas, esta linea ya tratara bien los valores pàsados 
        if client._server is None or client._port is None:
            client.usage()
            return

        client.shell()
        print("+++ FINISHED +++")
    
    

if __name__=="__main__":
    client.main(sys.argv[1:]) #He modificado esto para hacer el sys.