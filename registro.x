
struct DatosOperacion {
    string nombre_usuario<256>;
    string operacion<256>;
};

program REGISTRO_PROG {
    version REGISTRO_VERS {
        int IMPRIMIR_LOG(DatosOperacion) = 1;
    } = 1;
} = 100522369; 