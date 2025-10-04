package clients.java;

import java.io.*;
import java.net.*;
import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class Client extends JFrame {
    
    private Socket tcpSocket;
    private DatagramSocket udpSocket;
    private BufferedReader tcpIn;
    private PrintWriter tcpOut;
    
    private boolean connected = false;
    private boolean isAdmin = false;
    private String clientId = "";
    
    // GUI Components
    private JTextField hostField, portField, passwordField;
    private JRadioButton observerRadio, adminRadio;
    private JButton connectBtn, disconnectBtn;
    private JButton speedUpBtn, slowDownBtn, leftBtn, rightBtn, listBtn;
    private JLabel speedLabel, batteryLabel, tempLabel, dirLabel;
    private JTextArea console;
    
    public Client() {
        setTitle("Cliente Vehículo Autónomo - Java");
        setSize(700, 650);
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLayout(new BorderLayout());
        
        createGUI();
        
        addWindowListener(new WindowAdapter() {
            public void windowClosing(WindowEvent e) {
                disconnect();
            }
        });
    }
    
    private void createGUI() {
        // Panel de conexión
        JPanel connPanel = new JPanel(new GridBagLayout());
        connPanel.setBorder(BorderFactory.createTitledBorder("Conexión"));
        GridBagConstraints gbc = new GridBagConstraints();
        gbc.insets = new Insets(5, 5, 5, 5);
        
        gbc.gridx = 0; gbc.gridy = 0;
        connPanel.add(new JLabel("Servidor:"), gbc);
        gbc.gridx = 1;
        hostField = new JTextField("127.0.0.1", 10);
        connPanel.add(hostField, gbc);
        
        gbc.gridx = 2;
        connPanel.add(new JLabel("Puerto:"), gbc);
        gbc.gridx = 3;
        portField = new JTextField("5000", 5);
        connPanel.add(portField, gbc);
        
        gbc.gridx = 0; gbc.gridy = 1;
        connPanel.add(new JLabel("Tipo:"), gbc);
        gbc.gridx = 1;
        observerRadio = new JRadioButton("Observador", true);
        connPanel.add(observerRadio, gbc);
        gbc.gridx = 2;
        adminRadio = new JRadioButton("Administrador");
        connPanel.add(adminRadio, gbc);
        ButtonGroup group = new ButtonGroup();
        group.add(observerRadio);
        group.add(adminRadio);
        
        gbc.gridx = 0; gbc.gridy = 2;
        connPanel.add(new JLabel("Password:"), gbc);
        gbc.gridx = 1;
        passwordField = new JPasswordField("admin123", 10);
        connPanel.add(passwordField, gbc);
        
        gbc.gridx = 2; gbc.gridwidth = 2;
        connectBtn = new JButton("Conectar");
        connectBtn.setBackground(Color.GREEN);
        connectBtn.addActionListener(e -> connect());
        connPanel.add(connectBtn, gbc);
        
        add(connPanel, BorderLayout.NORTH);
        
        // Panel de telemetría
        JPanel telemPanel = new JPanel(new GridLayout(4, 3, 10, 10));
        telemPanel.setBorder(BorderFactory.createTitledBorder("Telemetría"));
        
        telemPanel.add(new JLabel("Velocidad:", SwingConstants.RIGHT));
        speedLabel = new JLabel("0.0", SwingConstants.LEFT);
        speedLabel.setFont(new Font("Arial", Font.BOLD, 24));
        speedLabel.setForeground(Color.BLUE);
        telemPanel.add(speedLabel);
        telemPanel.add(new JLabel("km/h"));
        
        telemPanel.add(new JLabel("Batería:", SwingConstants.RIGHT));
        batteryLabel = new JLabel("0", SwingConstants.LEFT);
        batteryLabel.setFont(new Font("Arial", Font.BOLD, 24));
        batteryLabel.setForeground(Color.GREEN);
        telemPanel.add(batteryLabel);
        telemPanel.add(new JLabel("%"));
        
        telemPanel.add(new JLabel("Temperatura:", SwingConstants.RIGHT));
        tempLabel = new JLabel("0.0", SwingConstants.LEFT);
        tempLabel.setFont(new Font("Arial", Font.BOLD, 24));
        tempLabel.setForeground(Color.RED);
        telemPanel.add(tempLabel);
        telemPanel.add(new JLabel("°C"));
        
        telemPanel.add(new JLabel("Dirección:", SwingConstants.RIGHT));
        dirLabel = new JLabel("-", SwingConstants.LEFT);
        dirLabel.setFont(new Font("Arial", Font.BOLD, 24));
        dirLabel.setForeground(new Color(128, 0, 128));
        telemPanel.add(dirLabel);
        telemPanel.add(new JLabel(""));
        
        add(telemPanel, BorderLayout.CENTER);
        
        // Panel de comandos
        JPanel cmdPanel = new JPanel(new GridLayout(3, 2, 5, 5));
        cmdPanel.setBorder(BorderFactory.createTitledBorder("Comandos (Solo Admin)"));
        
        speedUpBtn = new JButton("⬆ SPEED UP");
        speedUpBtn.setEnabled(false);
        speedUpBtn.addActionListener(e -> sendCommand("SPUP"));
        cmdPanel.add(speedUpBtn);
        
        slowDownBtn = new JButton("⬇ SLOW DOWN");
        slowDownBtn.setEnabled(false);
        slowDownBtn.addActionListener(e -> sendCommand("SPDN"));
        cmdPanel.add(slowDownBtn);
        
        leftBtn = new JButton("⬅ TURN LEFT");
        leftBtn.setEnabled(false);
        leftBtn.addActionListener(e -> sendCommand("TNLF"));
        cmdPanel.add(leftBtn);
        
        rightBtn = new JButton("➡ TURN RIGHT");
        rightBtn.setEnabled(false);
        rightBtn.addActionListener(e -> sendCommand("TNRT"));
        cmdPanel.add(rightBtn);
        
        listBtn = new JButton("📋 LIST USERS");
        listBtn.setEnabled(false);
        listBtn.addActionListener(e -> sendCommand("LIST"));
        cmdPanel.add(listBtn);
        
        disconnectBtn = new JButton("❌ DISCONNECT");
        disconnectBtn.setEnabled(false);
        disconnectBtn.setBackground(Color.RED);
        disconnectBtn.setForeground(Color.WHITE);
        disconnectBtn.addActionListener(e -> disconnect());
        cmdPanel.add(disconnectBtn);
        
        JPanel bottomPanel = new JPanel(new BorderLayout());
        bottomPanel.add(cmdPanel, BorderLayout.NORTH);
        
        // Consola
        JPanel consolePanel = new JPanel(new BorderLayout());
        consolePanel.setBorder(BorderFactory.createTitledBorder("Consola"));
        console = new JTextArea(8, 50);
        console.setEditable(false);
        JScrollPane scroll = new JScrollPane(console);
        consolePanel.add(scroll, BorderLayout.CENTER);
        
        bottomPanel.add(consolePanel, BorderLayout.CENTER);
        add(bottomPanel, BorderLayout.SOUTH);
    }
    
    private void log(String message) {
        SwingUtilities.invokeLater(() -> {
            console.append("[" + java.time.LocalTime.now().toString().substring(0, 8) + "] " + message + "\n");
            console.setCaretPosition(console.getDocument().getLength());
        });
    }
    
    private void connect() {
        if (connected) {
            JOptionPane.showMessageDialog(this, "Ya estás conectado");
            return;
        }
        
        try {
            String host = hostField.getText();
            int port = Integer.parseInt(portField.getText());
            
            // Conectar TCP
            tcpSocket = new Socket(host, port);
            tcpIn = new BufferedReader(new InputStreamReader(tcpSocket.getInputStream()));
            tcpOut = new PrintWriter(tcpSocket.getOutputStream(), true);
            log("Conectado a " + host + ":" + port);
            
            // Enviar mensaje de conexión
            String connMsg;
            if (adminRadio.isSelected()) {
                String password = passwordField.getText();
                connMsg = "ADMIN:" + password;
                isAdmin = true;
            } else {
                connMsg = "OBSERVER";
                isAdmin = false;
            }
            
            String message = buildMessage("CONN", connMsg);
            tcpOut.print(message);
            tcpOut.flush();
            log("Enviado: " + message.trim());
            
            // Recibir respuesta
            String response = tcpIn.readLine();
            log("Recibido: " + response);
            
            String[] parts = parseMessage(response);
            String msgType = parts[0];
            String msgData = parts[1];
            
            if (msgType.equals("CACK")) {
                clientId = msgData;
                connected = true;
                log("✓ Conectado como " + clientId);
                
                // Iniciar receptor UDP
                startUDPReceiver();
                
                // Iniciar receptor TCP
                new Thread(this::receiveTCP).start();
                
                // Actualizar GUI
                SwingUtilities.invokeLater(() -> {
                    connectBtn.setEnabled(false);
                    disconnectBtn.setEnabled(true);
                    
                    if (isAdmin) {
                        speedUpBtn.setEnabled(true);
                        slowDownBtn.setEnabled(true);
                        leftBtn.setEnabled(true);
                        rightBtn.setEnabled(true);
                        listBtn.setEnabled(true);
                    }
                });
                
            } else if (msgType.equals("CERR")) {
                log("✗ Error: " + msgData);
                tcpSocket.close();
                JOptionPane.showMessageDialog(this, "Conexión rechazada: " + msgData, 
                                            "Error", JOptionPane.ERROR_MESSAGE);
            }
            
        } catch (Exception e) {
            log("✗ Error de conexión: " + e.getMessage());
            JOptionPane.showMessageDialog(this, "No se pudo conectar: " + e.getMessage(), 
                                        "Error", JOptionPane.ERROR_MESSAGE);
        }
    }
    
    private void startUDPReceiver() {
        try {
            udpSocket = new DatagramSocket(5001);
            new Thread(this::receiveUDP).start();
            log("Receptor UDP iniciado en puerto 5001");
        } catch (Exception e) {
            log("Error iniciando UDP: " + e.getMessage());
        }
    }
    
    private void receiveUDP() {
        byte[] buffer = new byte[1024];
        while (connected) {
            try {
                DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                udpSocket.receive(packet);
                String message = new String(packet.getData(), 0, packet.getLength());
                
                String[] parts = parseMessage(message);
                String msgType = parts[0];
                String msgData = parts[1];
                
                if (msgType.equals("TELE")) {
                    processTelemetry(msgData);
                }
                
            } catch (Exception e) {
                if (connected) {
                    log("Error UDP: " + e.getMessage());
                }
                break;
            }
        }
    }
    
    private void receiveTCP() {
        try {
            String line;
            while (connected && (line = tcpIn.readLine()) != null) {
                log("Recibido: " + line);
            }
        } catch (Exception e) {
            if (connected) {
                log("Error TCP: " + e.getMessage());
            }
        }
        
        if (connected) {
            log("Servidor cerró la conexión");
            disconnect();
        }
    }
    
    private void processTelemetry(String data) {
        // Parsear: SPEED:45.5|BATTERY:78|TEMP:35.2|DIR:NORTH
        String[] parts = data.split("\\|");
        for (String part : parts) {
            if (part.contains(":")) {
                String[] kv = part.split(":", 2);
                String key = kv[0];
                String value = kv[1];
                
                SwingUtilities.invokeLater(() -> {
                    switch (key) {
                        case "SPEED":
                            speedLabel.setText(value);
                            break;
                        case "BATTERY":
                            batteryLabel.setText(value);
                            break;
                        case "TEMP":
                            tempLabel.setText(value);
                            break;
                        case "DIR":
                            dirLabel.setText(value);
                            break;
                    }
                });
            }
        }
    }
    
    private void sendCommand(String command) {
        if (!connected) return;
        
        try {
            String message = buildMessage(command, "");
            tcpOut.print(message);
            tcpOut.flush();
            log("Comando enviado: " + command);
        } catch (Exception e) {
            log("Error enviando comando: " + e.getMessage());
        }
    }
    
    private void disconnect() {
        if (!connected) return;
        
        try {
            String message = buildMessage("DISC", "");
            tcpOut.print(message);
            tcpOut.flush();
        } catch (Exception e) {
            // Ignorar errores al desconectar
        }
        
        connected = false;
        
        try {
            if (tcpSocket != null) tcpSocket.close();
            if (udpSocket != null) udpSocket.close();
        } catch (Exception e) {
            // Ignorar
        }
        
        log("Desconectado del servidor");
        
        SwingUtilities.invokeLater(() -> {
            connectBtn.setEnabled(true);
            disconnectBtn.setEnabled(false);
            speedUpBtn.setEnabled(false);
            slowDownBtn.setEnabled(false);
            leftBtn.setEnabled(false);
            rightBtn.setEnabled(false);
            listBtn.setEnabled(false);
        });
    }
    
    private String buildMessage(String type, String data) {
        int len = data.length();
        return String.format("%s|%04d|%s\n", type, len, data);
    }
    
    private String[] parseMessage(String message) {
        String[] parts = message.trim().split("\\|", 3);
        if (parts.length >= 3) {
            return new String[] { parts[0], parts[2] };
        }
        return new String[] { "", "" };
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            Client client = new Client();
            client.setVisible(true);
        });
    }
}
