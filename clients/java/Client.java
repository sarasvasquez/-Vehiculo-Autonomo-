package clients.java;

import java.io.*;
import java.net.*;
import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class Client extends JFrame {

    // Sockets
    private Socket tcpSocket;
    private DatagramSocket udpSocket;
    private BufferedReader tcpIn;
    private PrintWriter tcpOut;

    // Estado de conexión
    private boolean connected = false;
    private boolean isAdmin = false;
    private String clientId = null;

    // Componentes de la interfaz
    private JLabel lblSpeed, lblBatt, lblTemp, lblDir;
    private JTextArea console;
    private JButton btnConnect, btnDisconnect;
    private JButton btnSpeedUp, btnSlowDown, btnTurnLeft, btnTurnRight;
    private JTextField txtHost, txtPort, txtPassword;
    private JRadioButton rbObserver, rbAdmin;

    public Client() {
        super("Cliente Vehículo Autónomo - Java");
        setSize(900, 700);
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLayout(new BorderLayout(10, 10));

        createConnectionPanel();
        createTelemetryPanel();
        createControlPanel();
        createConsolePanel();

        setVisible(true);
        log(">> Cliente Java iniciado. Esperando conexión...");
    }

    private void createConnectionPanel() {
        JPanel panel = new JPanel(new GridBagLayout());
        panel.setBorder(BorderFactory.createTitledBorder("Conexión al Servidor"));
        GridBagConstraints gbc = new GridBagConstraints();
        gbc.insets = new Insets(5, 5, 5, 5);
        gbc.fill = GridBagConstraints.HORIZONTAL;

        // Host
        gbc.gridx = 0; gbc.gridy = 0;
        panel.add(new JLabel("Host:"), gbc);
        gbc.gridx = 1;
        txtHost = new JTextField("127.0.0.1", 15);
        panel.add(txtHost, gbc);

        // Puerto
        gbc.gridx = 2;
        panel.add(new JLabel("Puerto TCP:"), gbc);
        gbc.gridx = 3;
        txtPort = new JTextField("5000", 8);
        panel.add(txtPort, gbc);

        // Tipo de usuario
        gbc.gridx = 0; gbc.gridy = 1;
        panel.add(new JLabel("Usuario:"), gbc);

        ButtonGroup userGroup = new ButtonGroup();
        rbObserver = new JRadioButton("Observador", true);
        rbAdmin = new JRadioButton("Administrador");
        userGroup.add(rbObserver);
        userGroup.add(rbAdmin);

        gbc.gridx = 1;
        panel.add(rbObserver, gbc);
        gbc.gridx = 2;
        panel.add(rbAdmin, gbc);

        // Password
        gbc.gridx = 0; gbc.gridy = 2;
        panel.add(new JLabel("Password:"), gbc);
        gbc.gridx = 1;
        txtPassword = new JPasswordField("admin123", 15);
        panel.add(txtPassword, gbc);

        // Botones
        gbc.gridx = 2;
        btnConnect = new JButton("Conectar");
        btnConnect.setBackground(new Color(34, 139, 34));
        btnConnect.setForeground(Color.WHITE);
        btnConnect.addActionListener(e -> connectToServer());
        panel.add(btnConnect, gbc);

        gbc.gridx = 3;
        btnDisconnect = new JButton("Desconectar");
        btnDisconnect.setBackground(new Color(178, 34, 34));
        btnDisconnect.setForeground(Color.WHITE);
        btnDisconnect.setEnabled(false);
        btnDisconnect.addActionListener(e -> disconnect());
        panel.add(btnDisconnect, gbc);

        add(panel, BorderLayout.NORTH);
    }

    private void createTelemetryPanel() {
        JPanel panel = new JPanel(new GridLayout(4, 3, 15, 15));
        panel.setBorder(BorderFactory.createTitledBorder("Telemetría en Tiempo Real"));
        panel.setPreferredSize(new Dimension(0, 300));

        Font labelFont = new Font("Arial", Font.PLAIN, 16);
        Font valueFont = new Font("Arial", Font.BOLD, 36);

        // Velocidad
        panel.add(createLabel("Velocidad:", labelFont));
        lblSpeed = createLabel("0", valueFont, Color.BLUE);
        panel.add(lblSpeed);
        panel.add(createLabel("km/h", labelFont));

        // Batería
        panel.add(createLabel("Batería:", labelFont));
        lblBatt = createLabel("0", valueFont, Color.GREEN);
        panel.add(lblBatt);
        panel.add(createLabel("%", labelFont));

        // Temperatura
        panel.add(createLabel("Temperatura:", labelFont));
        lblTemp = createLabel("0", valueFont, Color.RED);
        panel.add(lblTemp);
        panel.add(createLabel("°C", labelFont));

        // Dirección
        panel.add(createLabel("Dirección:", labelFont));
        lblDir = createLabel("-", valueFont, new Color(128, 0, 128));
        panel.add(lblDir);
        panel.add(new JLabel());

        add(panel, BorderLayout.CENTER);
    }

    private void createControlPanel() {
        JPanel panel = new JPanel(new GridBagLayout());
        panel.setBorder(BorderFactory.createTitledBorder("Comandos de Control"));
        GridBagConstraints gbc = new GridBagConstraints();
        gbc.insets = new Insets(10, 10, 10, 10);
        gbc.fill = GridBagConstraints.BOTH;

        // Acelerar (arriba)
        gbc.gridx = 1; gbc.gridy = 0;
        btnSpeedUp = createControlButton("⬆ ACELERAR", "SPUP");
        panel.add(btnSpeedUp, gbc);

        // Girar izquierda
        gbc.gridx = 0; gbc.gridy = 1;
        btnTurnLeft = createControlButton("⬅ GIRAR IZQ", "TNLF");
        panel.add(btnTurnLeft, gbc);

        // Girar derecha
        gbc.gridx = 2; gbc.gridy = 1;
        btnTurnRight = createControlButton("➡ GIRAR DER", "TNRT");
        panel.add(btnTurnRight, gbc);

        // Frenar (abajo)
        gbc.gridx = 1; gbc.gridy = 2;
        btnSlowDown = createControlButton("⬇ FRENAR", "SPDN");
        panel.add(btnSlowDown, gbc);

        add(panel, BorderLayout.SOUTH);
    }

    private void createConsolePanel() {
        JPanel panel = new JPanel(new BorderLayout());
        panel.setBorder(BorderFactory.createTitledBorder("Log de Eventos"));
        panel.setPreferredSize(new Dimension(0, 150));

        console = new JTextArea();
        console.setEditable(false);
        console.setFont(new Font("Monospaced", Font.PLAIN, 12));
        JScrollPane scroll = new JScrollPane(console);
        panel.add(scroll, BorderLayout.CENTER);

        add(panel, BorderLayout.EAST);
    }

    private JLabel createLabel(String text, Font font) {
        return createLabel(text, font, Color.BLACK);
    }

    private JLabel createLabel(String text, Font font, Color color) {
        JLabel label = new JLabel(text, SwingConstants.CENTER);
        label.setFont(font);
        label.setForeground(color);
        return label;
    }

    private JButton createControlButton(String text, String command) {
        JButton btn = new JButton(text);
        btn.setPreferredSize(new Dimension(150, 50));
        btn.setEnabled(false);
        btn.addActionListener(e -> sendCommand(command));
        return btn;
    }

    private void log(String message) {
        String timestamp = java.time.LocalTime.now().format(
            java.time.format.DateTimeFormatter.ofPattern("HH:mm:ss"));
        console.append("[" + timestamp + "] " + message + "\n");
        console.setCaretPosition(console.getDocument().getLength());
    }

    // ----------------- MÉTODO DE CONEXIÓN -----------------
    private void connectToServer() {
        if (connected) {
            JOptionPane.showMessageDialog(this, "Ya estás conectado");
            return;
        }

        String host = txtHost.getText();
        int tcpPort = Integer.parseInt(txtPort.getText());

        try {
            tcpSocket = new Socket(host, tcpPort);
            tcpIn = new BufferedReader(new InputStreamReader(tcpSocket.getInputStream()));
            tcpOut = new PrintWriter(tcpSocket.getOutputStream(), true);
            log("✓ Conectado a " + host + ":" + tcpPort + " (TCP)");

            boolean isServerCloud = host.contains("cloud") || tcpPort == 5000;
            int udpPort = 5002;
            String connMsg;

            if (rbAdmin.isSelected()) {
                String password = txtPassword.getText();

                if (isServerCloud) {
                    connMsg = String.format("CONN|%04d|ADMIN:%s\n", ("ADMIN:" + password).length(), password);
                } else {
                    connMsg = String.format("CONN|%04d|ADMIN:%s:%d\n", ("ADMIN:" + password + ":" + udpPort).length(), password, udpPort);
                }
                isAdmin = true;

            } else {
                if (isServerCloud) {
                    connMsg = String.format("CONN|%04d|OBSERVER\n", "OBSERVER".length());
                } else {
                    connMsg = String.format("CONN|%04d|OBSERVER:%d\n", ("OBSERVER:" + udpPort).length(), udpPort);
                }
                isAdmin = false;
            }

            tcpOut.print(connMsg);
            tcpOut.flush();
            log("→ Enviado: " + connMsg.trim());

            String response = tcpIn.readLine();
            log("← Recibido: " + response);

            String[] parts = response.split("\\|");
            if (parts[0].equals("CACK")) {
                clientId = parts[2];
                connected = true;

                btnConnect.setEnabled(false);
                btnDisconnect.setEnabled(true);

                if (isAdmin) {
                    btnSpeedUp.setEnabled(true);
                    btnSlowDown.setEnabled(true);
                    btnTurnLeft.setEnabled(true);
                    btnTurnRight.setEnabled(true);
                }

                log("✓ Autenticado como " + clientId);

                int udpPortToListen = isServerCloud ? 5001 : udpPort;
                startUdpListener(udpPortToListen);

            } else {
                log("✗ Error de autenticación: " + response);
                tcpSocket.close();
                JOptionPane.showMessageDialog(this, "Autenticación fallida", "Error", JOptionPane.ERROR_MESSAGE);
            }

        } catch (Exception e) {
            log("✗ Error de conexión: " + e.getMessage());
            JOptionPane.showMessageDialog(this, "No se pudo conectar: " + e.getMessage(), "Error", JOptionPane.ERROR_MESSAGE);
        }
    }

    private void startUdpListener(int udpPort) {
        try {
            udpSocket = new DatagramSocket(udpPort);
            log("✓ Escuchando telemetría en puerto " + udpPort + " (UDP)");

            Thread udpThread = new Thread(() -> {
                byte[] buffer = new byte[1024];
                while (connected) {
                    try {
                        DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                        udpSocket.receive(packet);
                        String message = new String(packet.getData(), 0, packet.getLength()).trim();
                        log("📡 Telemetría: " + message);
                        parseTelemetry(message);
                    } catch (Exception e) {
                        if (connected) log("✗ Error UDP: " + e.getMessage());
                        break;
                    }
                }
            });
            udpThread.setDaemon(true);
            udpThread.start();

        } catch (Exception e) {
            log("✗ Error al iniciar UDP: " + e.getMessage());
        }
    }

    private void parseTelemetry(String message) {
        try {
            String[] parts = message.split("\\|");
            if (parts.length >= 3 && parts[0].equals("TELE")) {
                String[] fields = message.substring(message.indexOf(parts[2])).split("\\|");

                for (String field : fields) {
                    if (field.contains(":")) {
                        String[] kv = field.split(":", 2);
                        String key = kv[0], value = kv[1];

                        SwingUtilities.invokeLater(() -> {
                            switch (key) {
                                case "SPEED": lblSpeed.setText(value); break;
                                case "BATTERY": lblBatt.setText(value); break;
                                case "TEMP": lblTemp.setText(value); break;
                                case "DIR": lblDir.setText(value); break;
                            }
                        });
                    }
                }
            }
        } catch (Exception e) {
            log("✗ Error parseando telemetría: " + e.getMessage());
        }
    }

    private void sendCommand(String command) {
        if (!connected || !isAdmin) {
            JOptionPane.showMessageDialog(this, "Debes estar conectado como administrador", "Advertencia", JOptionPane.WARNING_MESSAGE);
            return;
        }

        try {
            String msg = command + "|0000|\n";
            tcpOut.print(msg);
            tcpOut.flush();
            log("→ Comando: " + msg.trim());

            String response = tcpIn.readLine();
            log("← Respuesta: " + response);

            String[] parts = response.split("\\|");
            if (parts[0].equals("CMOK")) {
                JOptionPane.showMessageDialog(this, "Comando ejecutado correctamente", "Éxito", JOptionPane.INFORMATION_MESSAGE);
            } else {
                JOptionPane.showMessageDialog(this, "Comando rechazado: " + parts[2], "Error", JOptionPane.ERROR_MESSAGE);
            }

        } catch (Exception e) {
            log("✗ Error enviando comando: " + e.getMessage());
            JOptionPane.showMessageDialog(this, "Error de comunicación: " + e.getMessage(), "Error", JOptionPane.ERROR_MESSAGE);
        }
    }

    private void disconnect() {
        if (!connected) return;

        try {
            tcpOut.print("DISC|0000|\n");
            tcpOut.flush();
            log("→ Desconectando...");

            connected = false;

            if (tcpSocket != null && !tcpSocket.isClosed()) tcpSocket.close();
            if (udpSocket != null && !udpSocket.isClosed()) udpSocket.close();

            btnConnect.setEnabled(true);
            btnDisconnect.setEnabled(false);
            btnSpeedUp.setEnabled(false);
            btnSlowDown.setEnabled(false);
            btnTurnLeft.setEnabled(false);
            btnTurnRight.setEnabled(false);

            log("✓ Desconectado");

        } catch (Exception e) {
            log("✗ Error al desconectar: " + e.getMessage());
        }
    }

    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> new Client());
    }
}
