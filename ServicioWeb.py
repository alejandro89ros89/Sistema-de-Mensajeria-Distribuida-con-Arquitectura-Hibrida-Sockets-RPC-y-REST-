from http.server import BaseHTTPRequestHandler, HTTPServer
import urllib.parse

class ManejadorNormalizacion(BaseHTTPRequestHandler):
    def do_POST(self):
        # 1. Leer cuanto mide el mensaje que nos envian
        Longitud = int(self.headers['Content-Length'])
        DatosSucios = self.rfile.read(Longitud).decode('utf-8')
        
        # 2. Limpiar el mensaje: split() quita todos los espacios sobrantes
        # y join() los vuelve a unir con un solo espacio entre palabras
        Palabras = DatosSucios.split()
        MensajeLimpio = " ".join(Palabras)
        
        # 3. Enviar la respuesta al cliente
        self.send_response(200)
        self.send_header('Content-type', 'text/plain')
        self.end_headers()
        self.wfile.write(MensajeLimpio.encode('utf-8'))

def IniciarServidor():
    # Se despliega en el puerto 8000 de la maquina local
    Puerto = 8000
    ServidorWeb = HTTPServer(('', Puerto), ManejadorNormalizacion)
    print(f"Servicio Web de Normalizacion activo en el puerto {Puerto}...")
    ServidorWeb.serve_forever()

if __name__ == "__main__":
    IniciarServidor()