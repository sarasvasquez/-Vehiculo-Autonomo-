1. Visión General del Protocolo
1.1 Propósito
El protocolo VAT permite la comunicación entre un vehículo autónomo terrestre y múltiples usuarios remotos para:

Transmitir datos de telemetría en tiempo real
Recibir y ejecutar comandos de control
Gestionar autenticación y autorización de usuarios

1.2 Modelo de Funcionamiento

Arquitectura: Cliente-Servidor
Servidor: Vehículo autónomo (simulado)
Clientes: Usuarios administradores y observadores
Capa: Aplicación (sobre TCP/IP)

1.3 Protocolos de Transporte

TCP (Puerto 5000): Para comandos de control, autenticación y gestión
UDP (Puerto 5001): Para transmisión de telemetría (broadcast)


2. Especificación del Servicio
2.1 Primitivas del Servicio
PrimitivaDescripciónTipo UsuarioTransporteCONNECTEstablecer conexión inicialTodosTCPAUTHAutenticar como administradorAdministradorTCPGET_TELEMETRYSolicitar datos actualesTodosTCPSPEED_UPAumentar velocidadAdministradorTCPSLOW_DOWNDisminuir velocidadAdministradorTCPTURN_LEFTGirar a la izquierdaAdministradorTCPTURN_RIGHTGirar a la derechaAdministradorTCPLIST_USERSListar usuarios conectadosAdministradorTCPDISCONNECTCerrar conexiónTodosTCP
2.2 Telemetría Automática

El servidor envía datos de telemetría cada 10 segundos vía UDP
Todos los clientes conectados reciben estos datos automáticamente


3. Formato de Mensajes
3.1 Estructura General
Todos los mensajes siguen el formato texto con campos separados por |:
<TIPO>|<LONGITUD>|<DATOS>\n

TIPO: Tipo de mensaje (4 caracteres)
LONGITUD: Longitud del campo DATOS en bytes (4 dígitos)
DATOS: Contenido del mensaje (variable)
\n: Terminador de mensaje

3.2 Tipos de Mensajes
A. Mensajes de Cliente → Servidor
CONNECT (Conexión)
CONN|0010|OBSERVER\n
CONN|0009|ADMIN:pwd\n

OBSERVER: Usuario observador (solo lectura)
ADMIN:pwd: Administrador con password

Respuesta:
CACK|0007|CLIENT_ID\n  (éxito)
CERR|0018|INVALID_CREDENTIALS\n  (error)
Comandos de Control
SPUP|0000|\n         (SPEED UP)
SPDN|0000|\n         (SLOW DOWN)
TNLF|0000|\n         (TURN LEFT)
TNRT|0000|\n         (TURN RIGHT)
Respuesta:
CMOK|0008|EXECUTED\n     (comando ejecutado)
CMER|0011|LOW_BATTERY\n  (error: batería baja)
CMER|0011|SPEED_LIMIT\n  (error: límite velocidad)
CMER|0010|NO_PERMISSION\n (error: sin permisos)
Listar Usuarios
LIST|0000|\n
Respuesta:
ULST|0045|3|192.168.1.5:ADMIN|192.168.1.6:OBSERVER\n
B. Mensajes de Servidor → Cliente
Telemetría (UDP cada 10s)
TELE|0050|SPEED:45.5|BATTERY:78|TEMP:35.2|DIR:NORTH\n

SPEED: Velocidad en km/h (0-100)
BATTERY: Nivel de batería en % (0-100)
TEMP: Temperatura en °C (0-100)
DIR: Dirección (NORTH, SOUTH, EAST, WEST)


4. Reglas de Procedimiento
4.1 Flujo de Conexión
Cliente                          Servidor
  |                                |
  |------- CONN|...|... --------->|
  |                                | (Validar credenciales)
  |<------ CACK|...|... ----------| (si OK)
  |                                |
  |                                | (Agregar a lista de clientes)
  |                                | (Iniciar envío UDP telemetría)
  |                                |
4.2 Flujo de Comando
Cliente Admin                   Servidor
  |                                |
  |------- SPUP|0000| ----------->|
  |                                | (Verificar permisos)
  |                                | (Verificar estado vehículo)
  |                                | (Ejecutar comando)
  |<------ CMOK|...|... ----------| (si OK)
  |                                |
4.3 Flujo de Telemetría
Servidor                    Cliente 1, 2, 3...
  |                                |
  | (cada 10 segundos)             |
  |======= TELE|...|... =========>| (UDP broadcast)
  |                                | (Actualizar interfaz)
  |                                |
4.4 Autenticación de Administrador

Password predefinido: admin123
El servidor valida credenciales al conectar
Se mantiene una sesión con identificador único
Si el admin se reconecta desde otra IP, usa el mismo password

4.5 Control de Errores

Mensaje inválido: Servidor responde CERR|0015|INVALID_MESSAGE\n
Comando sin permisos: CMER|0013|NO_PERMISSION\n
Desconexión abrupta: Servidor detecta y remueve de lista
Timeout: 30 segundos sin actividad → desconexión automática


5. Estados del Servidor
[INICIO] 
   |
   v
[LISTENING] --CONN--> [CONNECTED] --DISCONNECT--> [LISTENING]
                           |
                           |--COMANDO--> [EXECUTING] --> [CONNECTED]
                           |
                           |--10s--> [SENDING_TELEMETRY] --> [CONNECTED]

6. Códigos de Error
CódigoMensajeDescripciónINVALID_CREDENTIALSCredenciales inválidasPassword incorrectoINVALID_MESSAGEMensaje mal formadoFormato incorrectoNO_PERMISSIONSin permisosUsuario no es adminLOW_BATTERYBatería bajaBatería < 20%SPEED_LIMITLímite de velocidadVelocidad máxima alcanzadaALREADY_CONNECTEDYa conectadoCliente ya tiene sesión

7. Ejemplos de Implementación
7.1 Conexión de Observador
Cliente -> Servidor: CONN|0008|OBSERVER\n
Servidor -> Cliente: CACK|0006|CLI001\n
[Cada 10s] Servidor -> Cliente (UDP): TELE|0050|SPEED:45.5|BATTERY:78|TEMP:35.2|DIR:NORTH\n
7.2 Conexión de Admin y Comando
Cliente -> Servidor: CONN|0014|ADMIN:admin123\n
Servidor -> Cliente: CACK|0006|CLI002\n
Cliente -> Servidor: SPUP|0000|\n
Servidor -> Cliente: CMOK|0008|EXECUTED\n
[Cada 10s] Servidor -> Cliente (UDP): TELE|0050|SPEED:55.5|BATTERY:77|TEMP:35.5|DIR:NORTH\n
7.3 Error por Batería Baja
Cliente -> Servidor: SPUP|0000|\n
Servidor -> Cliente: CMER|0011|LOW_BATTERY\n

8. Consideraciones de Seguridad

Password en texto plano (para simplificar el proyecto)
En producción: usar TLS/SSL y hash de passwords
Validación de longitud de mensajes para prevenir buffer overflow
Timeout de sesiones inactivas


9. Limitaciones del Protocolo

Soporta hasta 50 clientes simultáneos
Velocidad máxima: 100 km/h
Comandos no se encolan (solo uno a la vez)
Telemetría se pierde si el cliente no está escuchando UDP