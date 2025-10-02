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

            // Acción al enviar un comando
            botonEnviar.addActionListener(new ActionListener() {
                public void actionPerformed(ActionEvent e) {
                    String comando = campoComando.getText();
                    if (!comando.isEmpty()) {
                        if (!demoMode && salida != null) {
                        salida.println(comando);  // Envía al servidor
                        areaDatos.append("Comando enviado: " + comando + "\n");
                        } else {
                            areaDatos.append("Demo> Comando simulado: " + comando + "\n");
                        }
                        campoComando.setText("");
                    }
                }
            });

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
                new Thread(new Runnable() {
                    public void run() {
                        try {
                            String linea;
                            while ((linea = entrada.readLine()) != null) {
                                areaDatos.append("Servidor: " + linea + "\n");
                                procesarMensaje(linea);
                            }
                        } catch (IOException ex) {
                            areaDatos.append("Conexión cerrada.\n");
                        }
                    }
                }).start();
            } else {
                // Si no hay servidor, iniciar simulación
                new Timer(2000, e -> generarDemo()).start();
            }
        }

        // Procesa mensajes reales del servidor (ejemplo: TELEMETRY SPEED=30 BATTERY=80 DIR=LEFT)
        private void procesarMensaje(String msg) {
            if (msg.startsWith("TELEMETRY")) {
                String[] partes = msg.split(" ");
                for (String p : partes) {
                    if (p.startsWith("SPEED=")) lblSpeed.setText(p.split("=")[1] + " km/h");
                    if (p.startsWith("BATTERY=")) lblBatt.setText(p.split("=")[1] + " %");
                    if (p.startsWith("DIR=")) lblDir.setText(p.split("=")[1]);
                }
            }
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
        new Client("127.0.0.1", 8080);
    }
}