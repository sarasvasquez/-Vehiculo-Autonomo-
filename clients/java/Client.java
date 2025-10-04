package clients.java;

import java.io.*;
import java.net.*;
import java.util.Random;

import javax.swing.*;
import java.awt.*;
import java.awt.event.*;


public class Client {

    private Socket socket;
    private BufferedReader entrada;
    private PrintWriter salida;

    // Interfaz gráfica
    private JFrame frame;
    private JTextArea areaDatos;
    private JTextField campoComando;
    private JButton botonEnviar;

    // Labels para mostrar telemetría
    private JLabel lblSpeed, lblBatt, lblDir;

    // Modo demo (sin servidor)
    private boolean demoMode = false;
    private Random rand = new Random();

    public Client(String host, int puerto) {
        try {
            // Conexión al servidor
            socket = new Socket(host, puerto);
            entrada = new BufferedReader(new InputStreamReader(socket.getInputStream()));
            salida = new PrintWriter(socket.getOutputStream(), true);
            System.out.println("Conectado al servidor en " + host + ":" + puerto);

        } catch (IOException e) {
            System.out.println("No se pudo conectar al servidor, iniciando en modo DEMO.");
            demoMode = true;
        }

        // Crear la GUI
        frame = new JFrame("Cliente Vehículo Autónomo - Java");
        frame.setSize(400, 300);
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        frame.setLayout(new BorderLayout());

        // Panel superior con telemetría
        JPanel panelDatos = new JPanel(new GridLayout(3, 2, 10, 10));
        panelDatos.setBorder(BorderFactory.createTitledBorder("Telemetría"));

        panelDatos.add(new JLabel("Velocidad:"));
        lblSpeed = new JLabel("0 km/h");
        lblSpeed.setFont(new Font("Arial", Font.BOLD, 20));
        panelDatos.add(lblSpeed);

        panelDatos.add(new JLabel("Batería:"));
        lblBatt = new JLabel("0 %");
        lblBatt.setFont(new Font("Arial", Font.BOLD, 20));
        panelDatos.add(lblBatt);

        panelDatos.add(new JLabel("Dirección:"));
        lblDir = new JLabel("-");
        lblDir.setFont(new Font("Arial", Font.BOLD, 20));
        panelDatos.add(lblDir);

        // Área de mensajes
        areaDatos = new JTextArea();
        areaDatos.setEditable(false);
        JScrollPane scroll = new JScrollPane(areaDatos);

        // Panel inferior con campo de comandos
        campoComando = new JTextField();
        botonEnviar = new JButton("Enviar Comando");

        // Acción al enviar un comando: construimos el mensaje según el protocolo CMND|LLLL|DATA
        ActionListener enviarAccion = new ActionListener() {
            public void actionPerformed(ActionEvent e) {
                String raw = campoComando.getText();
                if (raw == null) return;
                String texto = raw.trim();
                if (texto.isEmpty()) return;

                // Normalizar entrada
                String normalized = texto.toUpperCase().replaceAll("\\s+", " ");

                // Traducir comandos legibles a códigos del servidor
                String codigo = normalized;
                switch (normalized) {
                    case "SPEED UP":
                    case "SPEEDUP":
                        codigo = "SPUP";
                        break;
                    case "SLOW DOWN":
                    case "SLOWDOWN":
                        codigo = "SPDN";
                        break;
                    case "TURN LEFT":
                    case "TURNLEFT":
                        codigo = "TNLF";
                        break;
                    case "TURN RIGHT":
                    case "TURNRIGHT":
                        codigo = "TNRT";
                        break;
                    // Si el usuario ya ingresó el código corto, lo dejamos tal cual:
                    case "SPUP":
                    case "SPDN":
                    case "TNLF":
                    case "TNRT":
                        codigo = normalized;
                        break;
                    default:
                        // Enviar tal cual lo ingresado (el servidor puede ignorarlo si no lo reconoce)
                        codigo = normalized;
                        break;
                }

                if (!demoMode && salida != null) {
                    // Construir mensaje con tipo CMND, longitud de 4 dígitos y data = codigo
                    String toSend = String.format("CMND|%04d|%s", codigo.length(), codigo);
                    salida.println(toSend);  // Envía al servidor con \n
                    areaDatos.append("Comando enviado: " + texto + " -> " + codigo + "\n");
                    areaDatos.append("Enviado raw: " + toSend + "\n");
                    System.out.println("ENVIADO RAW: [" + toSend + "]");
                } else {
                    areaDatos.append("Demo> Comando simulado: " + texto + " -> " + codigo + "\n");
                }
                campoComando.setText("");
            }
        };

        botonEnviar.addActionListener(enviarAccion);
        // Permitir enviar al presionar Enter en el campo de texto
        campoComando.addActionListener(enviarAccion);

        JPanel panelInferior = new JPanel(new BorderLayout());
        panelInferior.add(campoComando, BorderLayout.CENTER);
        panelInferior.add(botonEnviar, BorderLayout.EAST);

        // Agregar todo al frame
        frame.add(panelDatos, BorderLayout.NORTH);
        frame.add(scroll, BorderLayout.CENTER);
        frame.add(panelInferior, BorderLayout.SOUTH);
        frame.setVisible(true);

        // Si hay servidor, escuchar mensajes
        if (!demoMode) {
            new Thread(() -> {
                try {
                    String linea;
                    while ((linea = entrada.readLine()) != null) {
                        // Mostrar mensaje crudo para depuración
                        final String raw = linea;
                        System.out.println("RAW FROM SERVER: [" + raw + "]");
                        
                        SwingUtilities.invokeLater(() -> areaDatos.append("Servidor: " + raw + "\n"));
                        procesarMensaje(raw);
                    }
                } catch (IOException ex) {
                    SwingUtilities.invokeLater(() -> areaDatos.append("Conexión cerrada.\n"));
                }

            }).start();
        } else {
            // Si no hay servidor, iniciar simulación
            new Timer(2000, e -> generarDemo()).start();
        }
    }

    // Procesa mensajes reales del servidor (ejemplo: TELE|... o CMOK/CMER/CERR)
    private void procesarMensaje(String msg) {
        if (msg == null) return;
        String m = msg.trim();
        System.out.println("Procesando mensaje: [" + m + "]");

        // Primeramente detectar respuestas de comando: CMOK, CMER, CERR
        if (m.startsWith("CMOK|") || m.startsWith("CMER|") || m.startsWith("CERR|")) {
            String[] parts = m.split("\\|", 3);
            String tipo = parts.length > 0 ? parts[0] : "";
            String data = parts.length > 2 ? parts[2] : (parts.length > 1 ? parts[1] : "");
            final String linea = tipo + " -> " + data;
            SwingUtilities.invokeLater(() -> areaDatos.append("Servidor: " + linea + "\n"));
            return;
        }

        // Normalizar separadores: convertir '|' y ';' a espacios para manejar varios formatos
        String norm = m.replace('|', ' ').replace(';', ' ').replace(',', ' ');
        String[] tokens = norm.split("\\s+");

        // Variables temporales
        String speedVal = null, battVal = null, dirVal = null, tempVal = null;

        for (String t : tokens) {
            String tt = t.toUpperCase();
            if (tt.startsWith("SPEED=") || tt.startsWith("SPEED:")) {
                String[] kv = t.split("[=:]");
                if (kv.length >= 2) speedVal = kv[1];
            } else if (tt.startsWith("BATTERY=") || tt.startsWith("BATTERY:")) {
                String[] kv = t.split("[=:]");
                if (kv.length >= 2) battVal = kv[1];
            } else if (tt.startsWith("DIR=") || tt.startsWith("DIR:") || tt.startsWith("DIRECTION=")) {
                String[] kv = t.split("[=:]");
                if (kv.length >= 2) dirVal = kv[1];
            } else if (tt.startsWith("TEMP=") || tt.startsWith("TEMP:") || tt.startsWith("TEMPERATURE=")) {
                String[] kv = t.split("[=:]");
                if (kv.length >= 2) tempVal = kv[1];
            }
        }

        // Hacer copias final para capturar en la lambda
        final String fSpeed = speedVal;
        final String fBatt  = battVal;
        final String fDir   = dirVal;
        final String fTemp  = tempVal;

        // Actualizar GUI en EDT
        SwingUtilities.invokeLater(() -> {
            if (fSpeed != null) lblSpeed.setText(fSpeed + " km/h");
            if (fBatt  != null) lblBatt.setText(fBatt + " %");
            if (fDir   != null) lblDir.setText(fDir);
            if (fTemp  != null) areaDatos.append("Temp: " + fTemp + " °C\n");
        });
    }

    // Genera datos simulados en modo demo
    private void generarDemo() {
        int speed = rand.nextInt(61); // 0-60
        int batt = rand.nextInt(101); // 0-100
        String[] dirs = {"LEFT", "RIGHT", "FORWARD"};
        String dir = dirs[rand.nextInt(dirs.length)];

        lblSpeed.setText(speed + " km/h");
        lblBatt.setText(batt + " %");
        lblDir.setText(dir);

        areaDatos.append("Demo> TELEMETRY SPEED=" + speed + " BATTERY=" + batt + " DIR=" + dir + "\n");
    }

    public static void main(String[] args) {
        // Cambia host y puerto según tu servidor
        new Client("127.0.0.1", 5000);
    }
}
