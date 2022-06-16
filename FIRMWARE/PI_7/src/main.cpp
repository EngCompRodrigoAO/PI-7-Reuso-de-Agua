//-----------------------------------------------------------------------------------------------------------  BIBLIOTECAS
#include <WiFi.h>
#include <SPI.h>
#include <Wire.h>
#include "ThingSpeak.h"
#include <LiquidCrystal_I2C.h>
#include <MCP23017.h>
#include <ArduinoUniqueID.h>

//-----------------------------------------------------------------------------------------------------------   CONSTANTES
const char *apiWriteKey = "78HR62O527P424BP"; // Chave de gravação do ThingSpeak
const char *apiReadKey = "BNZX34W2JPUZHRWO";  // Chave de leitura do ThingSpeak
const char *ssid = "FALANGE_SUPREMA";         // Nome da rede wifi ssid
const char *pass = "#kinecs#";                // Senha da rede wifi
// const char *ssid = "Hotel Exclusivo";         // Nome da rede wifi ssid
// const char *pass = "rwhe2008";                // Senha da rede wifi
//-----------------------------------------------------------------------------------------------------------  DEFINIÇÕES
#define SENSOR_FLUXO 27
#define SENSOR_NIVEL_1 33
#define SENSOR_NIVEL_2 26
#define SENSOR_NIVEL_3 25
#define SENSOR_NIVEL_4 14
#define SENSOR_NIVEL_5 13
#define SENSOR_NIVEL_6 12
#define BOMBA_1 32
#define SOLENOIDE_POTAVEL 5
#define SOLENOIDE_DESCARTE 4
#define LED_STATUS 15
#define MCP23017_PORT_A 39
#define MCP23017_PORT_B 36
#define MCP_ADDRESS 0x20 // (A2/A1/A0 = LOW) configura o endereço do MCP23017 na rede I2C
#define LCD_ADDRESS 0x38 //(A1/A2/A0 = HIGH) configura o endereço do LCD na rede I2C
#define RESET_PIN 4
#define LED_WIFI 2

//----------------------------------------------------------------------------------------------------- VARIAVEIS GLOBAIS
long currentMillis = 0;
long previousMillis = 0;
int interval = 1000; // Define o intervalo de leitura em milisegundos
float calibrationFactor = 4.5;
volatile byte pulseCount;
byte pulse1Sec = 0;
float flowRate;
unsigned long flowMilliLitres;
unsigned long tempflowMilliLitres;
unsigned int totalMilliLitres;
float flowLitres;
float totalLitres;
boolean bombStatus = 0;
boolean potableSolenoidStatus = 0;
boolean discardSolenoidStatus = 0;
boolean levelSensor_1 = 0, levelSensor_2 = 0, levelSensor_3 = 0, levelSensor_4 = 0, levelSensor_5 = 0, levelSensor_6 = 0;
int accumulatorLevel = 0;
int reservoirLevel = 0;
boolean bombPower = 0;
boolean statusLed = 0;
String NUMERO_SERIE;

//-----------------------------------------------------------------------------------------------------------       VOIDS
void IRAM_ATTR pulseCounter()
{
    pulseCount++;
}

// acionamento da bomba 1
void BOMBA_ACIONAMENTO(boolean STATUS_BOMBA_1)
{
    if (STATUS_BOMBA_1 != 0)
    {
        digitalWrite(BOMBA_1, HIGH);
        bombPower = 1;
    }
    else
    {
        digitalWrite(BOMBA_1, LOW);
        bombPower = 0;
    }
}

// Acionamento solenoide da agua potável
void SOLOENOIDE_POTAVEL_ACIONAMENTO(boolean STATUS_SOLENOIDE_1)
{
    if (STATUS_SOLENOIDE_1 != 0)
    {
        digitalWrite(SOLENOIDE_POTAVEL, HIGH);
        potableSolenoidStatus = 1;
    }
    else
    {
        digitalWrite(SOLENOIDE_POTAVEL, LOW);
        potableSolenoidStatus = 0;
    }
}

// Acionamento solenoide da agua descarte
void SOLOENOIDE_REUSO_ACIONAMENTO(boolean STATUS_SOLENOIDE_2)
{
    if (STATUS_SOLENOIDE_2 != 0)
    {
        digitalWrite(SOLENOIDE_DESCARTE, HIGH);
        discardSolenoidStatus = 1;
    }
    else
    {
        digitalWrite(SOLENOIDE_DESCARTE, LOW);
        discardSolenoidStatus = 0;
    }
}

void RESETA_SERVER()
{
    // Carrega as informações para serem enviadas em um lote somente para o servidor
    ThingSpeak.setField(1, 0); // Fluxo corrente
    ThingSpeak.setField(2, 0); // Total em mililitros
    ThingSpeak.setField(3, 0); // Total em litros
    ThingSpeak.setField(4, 0); // Nivel reservatorio
    ThingSpeak.setField(5, 0); // Nivel acumulador
    ThingSpeak.setField(6, 0); // Status da Bomba
    ThingSpeak.setField(7, 0); // Status Solenoide Potavel
    ThingSpeak.setField(8, 0); // Status Solenoide Descarte
    ThingSpeak.setStatus("ZERA SERVIDOR");
    ThingSpeak.writeFields(1, apiWriteKey); // Envia o lote de informações para o servidor
}

WiFiClient client;
LiquidCrystal_I2C lcd(LCD_ADDRESS, 20, 4); // Seta o endereço do LCD em 0x27 e configura o LCD de 20 colunas e 4 linhas
MCP23017 myMCP = MCP23017(MCP_ADDRESS);
//------------------------------------------------------------------------------------------------- CARREGA CONFIGURAÇÕES
void setup()
{
    Serial.begin(9600); // Inicia a comunicação serial
    UniqueIDdump(Serial);
    NUMERO_SERIE = "MCUDEVICE-";
    Serial.print("UniqueID: ");
    for (size_t i = 0; i < UniqueIDsize; i++)
    {
        if (UniqueID[i] < 0x10)
            Serial.print("0");

        Serial.print(UniqueID[i], HEX);
        NUMERO_SERIE += String(UniqueID[i], HEX);
        Serial.print("");
    }
    Serial.println();
    NUMERO_SERIE.toUpperCase();
    Serial.println(NUMERO_SERIE);
    WiFi.mode(WIFI_STA);      // Coloca o WIFI do ESP32 em modo estação
    ThingSpeak.begin(client); // Inicializa a comunicação com o ThingSpeak
    lcd.init();               // Incializa o LCD
    lcd.backlight();          // Seta a Iluminação do LCD
    myMCP.Init();
    /*
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("      UNIVESP       ");
        lcd.setCursor(0, 1);
        lcd.print(" PROJETO INTEGRADOR ");
        lcd.setCursor(0, 2);
        lcd.print("   REUSO DA AGUA    ");
        lcd.setCursor(0, 3);
        lcd.print("    GRUPO: 4N88     ");
        delay(5000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("     INTEGRANTES    ");
        delay(3000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("ANDRE LUIZ PRADO DOS SANTOS, 1822375");
        delay(3000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("DIEGO ALVES DOS SANTOS, 1831936");
        delay(3000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("ELTON SOLIGUETO, 1836172");
        delay(3000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("LILIAN ADRIANA GONZALEZ TENORIO, 1835883");
        delay(3000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("RODRIGO DE AVILA OLIVEIRA, 1826340");
        delay(3000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("SERGIO LUIZ BARBOSA, 1826279");
        delay(3000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("VICTOR MAKTURA, 1827787");
        delay(3000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("ORIENTADORA: THAIS PEREIRA DA SILVA");
        delay(5000);
        lcd.clear();
    */
    /* Faz a verificação se tem conectividade wifi caso tenha passa a frente caso não faz a conexão com a rede
     WIFI pré estabelecida na cosntante ssid e pass.*/
    if (WiFi.status() != WL_CONNECTED)
    {
        lcd.clear();
        lcd.print("AGUARDE CONECTANDO...");
        Serial.println("AGUARDE CONECTANDO.");
        while (WiFi.status() != WL_CONNECTED)
        {
            WiFi.begin(ssid, pass);
            delay(5000);
        }
        lcd.clear();
        lcd.print("Conectado.");
        Serial.println("Conectado.");
    }
    // RESETA_SERVER();
    delay(2000);

    // configurar as portas do microcontrolador como entradas de nivel alto
    Serial.println("SENSOR_FLUXO");
    pinMode(SENSOR_FLUXO, INPUT_PULLUP); // Seta o comportamento da porta do microcontrolador
    Serial.println("SENSOR_NIVEL_1");
    pinMode(SENSOR_NIVEL_1, INPUT_PULLUP); // Seta a porta 33 do microcontrolador para sensor nivel 1 como entrada
    Serial.println("SENSOR_NIVEL_2");
    pinMode(SENSOR_NIVEL_2, INPUT_PULLUP); // Seta a porta 26 do microcontrolador para sensor nivel 2 como entrada
    Serial.println("SENSOR_NIVEL_3");
    pinMode(SENSOR_NIVEL_3, INPUT_PULLUP); // Seta a porta 25 do microcontrolador para sensor nivel 3 como entrada
    Serial.println("SENSOR_NIVEL_4");
    pinMode(SENSOR_NIVEL_4, INPUT_PULLUP); // Seta a porta 14 do microcontrolador para sensor nivel 4 como entrada
    Serial.println("SENSOR_NIVEL_5");
    pinMode(SENSOR_NIVEL_5, INPUT_PULLUP); // Seta a porta 13 do microcontrolador para sensor nivel 5 como entrada
    Serial.println("SENSOR_NIVEL_6");
    pinMode(SENSOR_NIVEL_6, INPUT_PULLUP); // Seta a porta 12 do microcontrolador para sensor nivel 6 como entrada

    // configurar as portas do microcontrolador como saidas
    Serial.println("LED_STATUS");
    pinMode(LED_STATUS, OUTPUT); // Seta a porta 15 do microcontrolador para led de status como saida
    Serial.println("LED_WIFI");
    pinMode(LED_WIFI, OUTPUT); // Seta a porta 2 do microcontrolador para led de status como saida
    Serial.println("SOLENOIDE_POTAVEL");
    pinMode(SOLENOIDE_POTAVEL, OUTPUT); // Seta a porta 5 do microcontrolador para solenoide potavel como saida
    Serial.println("SOLENOIDE_DESCARTE");
    pinMode(SOLENOIDE_DESCARTE, OUTPUT); // Seta a porta 4 do microcontrolador para solenoide descarte como saida
    Serial.println("BOMBA_1");
    pinMode(BOMBA_1, OUTPUT); // Seta a porta 32 do microcontrolador para bomba como saida
    // Configura as portas do MCP23017
    Serial.println("MCP23017");
    myMCP.setAllPins(A, OFF);         // Porta A: todos pinos em LOW
    myMCP.setAllPins(B, OFF);         // Porta B: todos pinos em LOW
    myMCP.setPortMode(0b11111111, A); // Porta A: todos pinos como OUTPUT
    myMCP.setPortMode(0b11110000, B); // Porta B: B0 - B3 pinos como OUTPUT, B4-B7 pinos como INPUT

    pulseCount = 0;
    flowRate = 0;
    flowMilliLitres = 0;

    // Carrega a informação do servidor o total acumulada de mililitros
    totalMilliLitres = ThingSpeak.readFloatField(1753394, 2, apiReadKey);
    // Carrega a informação do servidor o total acumulado de litros.
    totalLitres = ThingSpeak.readFloatField(1753394, 3, apiReadKey);

    previousMillis = 0;

    // Cria uma interrupção para a porta do microcontrolador para o sensor de fluxo que funciona em modo HALL
    attachInterrupt(digitalPinToInterrupt(SENSOR_FLUXO), pulseCounter, FALLING);
}
//------------------------------------------------------------------------------------- Loop infinito do microcontrolador
void loop()
{
    digitalWrite(LED_STATUS, LOW);
    currentMillis = millis();
    if (currentMillis - previousMillis > interval)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("AGUARDE CONECTANDO...");
            digitalWrite(LED_WIFI, LOW);
            while (WiFi.status() != WL_CONNECTED)
            {
                WiFi.begin(ssid, pass);
                delay(5000);
            }
            Serial.println("Conectado.");
            digitalWrite(LED_WIFI, HIGH);
        }
        pulse1Sec = pulseCount; // Recebe a informação do sensor de fluxo
        pulseCount = 0;         // Limpa a variavel para nova varredura
        // calcula a fluxo de passagem
        flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
        previousMillis = millis();

        /* Divida a vazão em litros/minuto por 60 para determinar quantos litros
        passou pelo sensor neste intervalo de 1 segundo, então multiplique por 1000 para
        converte para mililitros.*/
        flowMilliLitres = (flowRate / 60) * 1000;
        flowLitres = (flowRate / 60);

        totalMilliLitres += flowMilliLitres;
        totalLitres += flowLitres;
        levelSensor_1 = digitalRead(SENSOR_NIVEL_1);
        levelSensor_2 = digitalRead(SENSOR_NIVEL_2);
        levelSensor_3 = digitalRead(SENSOR_NIVEL_3);
        levelSensor_4 = digitalRead(SENSOR_NIVEL_4);
        levelSensor_5 = digitalRead(SENSOR_NIVEL_5);
        levelSensor_6 = digitalRead(SENSOR_NIVEL_6);
        potableSolenoidStatus = digitalRead(SOLENOIDE_POTAVEL);
        discardSolenoidStatus = digitalRead(SOLENOIDE_DESCARTE);
        bombStatus = digitalRead(BOMBA_1);

        // Imprima a vazão para este segundo em litros/minuto
        Serial.print("FLUXO: ");
        Serial.print(float(flowRate)); // Envia para Serial Monitor o FLUXO
        Serial.print("L/min");
        // Serial.print("\t");  // Envia para Serial Monitor a tabula
        Serial.print(" | "); // Envia para Serial Monitor a separação
        Serial.print("QUANTIDADE: ");
        Serial.print(totalMilliLitres); // Envia para Serial Monitor o total em mililitros
        Serial.print("mL / ");
        Serial.print(totalLitres); // Envia para Serial Monitor o total em litros
        Serial.print("L");
        Serial.print(" | "); // Envia para Serial Monitor a separação
        Serial.print("N1: ");
        Serial.print(levelSensor_1); // Envia para Serial Monitor Status Sensor nivel 1
        Serial.print(" | ");         // Envia para Serial Monitor a separação
        Serial.print("N2: ");
        Serial.print(levelSensor_2); // Envia para Serial Monitor Status Sensor nivel 2
        Serial.print(" | ");         // Envia para Serial Monitor a separação
        Serial.print("N3: ");
        Serial.print(levelSensor_3); // Envia para Serial Monitor Status Sensor nivel 3
        Serial.print(" | ");         // Envia para Serial Monitor a separação
        Serial.print("N4: ");
        Serial.print(levelSensor_4); // Envia para Serial Monitor Status Sensor nivel 4
        Serial.print(" | ");         // Envia para Serial Monitor a separação
        Serial.print("N5: ");
        Serial.print(levelSensor_5); // Envia para Serial Monitor Status Sensor nivel 5
        Serial.print(" | ");         // Envia para Serial Monitor a separação
        Serial.print("N6: ");
        Serial.print(levelSensor_6); // Envia para Serial Monitor Status Sensor nivel 6
        Serial.print(" | ");         // Envia para Serial Monitor a separação
        Serial.print("SR: ");
        Serial.print(discardSolenoidStatus); // Envia para Serial Monitor Status Solenoide Descarte
        Serial.print(" | ");                 // Envia para Serial Monitor a separação
        Serial.print("SP: ");
        Serial.print(potableSolenoidStatus); // Envia para Serial Monitor Status Solenoide Potavel
        Serial.print(" | ");                 // Envia para Serial Monitor a separação
        Serial.print("SP: ");
        Serial.println(bombStatus); // Envia para Serial Monitor Status Bomba

        if (levelSensor_1 == 1 && levelSensor_2 == 1 && levelSensor_3 == 1)
        {
            reservoirLevel = 100;
            BOMBA_ACIONAMENTO(1);
        }
        else
        {
            if (levelSensor_1 == 1 && levelSensor_2 == 1 && levelSensor_3 == 0)
            {
                reservoirLevel = 50;
            }
            else
            {
                if (levelSensor_1 == 1 && levelSensor_2 == 0 && levelSensor_3 == 0)
                {
                    reservoirLevel = 10;
                }
                if (levelSensor_1 == 0 && levelSensor_2 == 0 && levelSensor_3 == 0)
                {
                    BOMBA_ACIONAMENTO(0);
                }
                else
                {
                    if (levelSensor_4 == 1 && levelSensor_5 == 1)
                    {
                        SOLOENOIDE_POTAVEL_ACIONAMENTO(0); // Desliga solenoide Potavel
                        SOLOENOIDE_REUSO_ACIONAMENTO(1);   // Aciona solenoide Descarte
                    }
                    else
                    {
                        SOLOENOIDE_REUSO_ACIONAMENTO(0);   // Desliga Solenoide Descarte
                        SOLOENOIDE_POTAVEL_ACIONAMENTO(1); // Aciona solenoide Potavel
                    }
                }
            }
        }
        /*
                // verificar este switch ele esta travando o sistema todo
                switch (levelSensor_2)
                {
                case 1:
                    accumulatorLevel = 100;
                    if (bombStatus != 1)
                    {
                        bombStatus = 1;
                    }
                    break;

                case 0:
                    switch (levelSensor_1)
                    {
                    case 0:
                        if (bombStatus != 0)
                        {
                            bombStatus = 0;
                            accumulatorLevel = 10;
                        }
                        break;
                    case 1:
                        bombStatus = 1;
                        break;
                    }
                } */
        // Escreve no Display LCD
        lcd.clear(); // Limpa o LCD
        lcd.setCursor(0, 0);
        lcd.print(F("FLUXO: "));
        lcd.setCursor(7, 0);
        lcd.print(float(flowRate)); // Escreve no LCD o FLUXO
        lcd.setCursor(13, 0);
        lcd.print(F("L/Min"));
        lcd.setCursor(0, 1);
        lcd.print(F("TOTAL: "));
        lcd.setCursor(7, 1);
        lcd.print(totalMilliLitres); // Escreve no LCD o total em mililitros
        lcd.setCursor((sizeof(totalMilliLitres) + 8), 1);
        lcd.print(F(" ml"));
        lcd.setCursor(0, 2);
        lcd.print(F("TOTAL: "));
        lcd.setCursor(7, 2);
        lcd.print(totalLitres); // Escreve no LCD o total em litros
        lcd.setCursor((sizeof(totalLitres) + 8), 2);
        lcd.print(F(" L"));

        lcd.setCursor(0, 3);
        lcd.print("|");
        lcd.setCursor(1, 3);
        lcd.print(levelSensor_1);

        lcd.setCursor(2, 3);
        lcd.print("|");
        lcd.setCursor(3, 3);
        lcd.print(levelSensor_2);

        lcd.setCursor(4, 3);
        lcd.print("|");
        lcd.setCursor(5, 3);
        lcd.print(levelSensor_3);

        lcd.setCursor(6, 3);
        lcd.print("|");
        lcd.setCursor(7, 3);
        lcd.print(levelSensor_4);

        lcd.setCursor(8, 3);
        lcd.print("|");
        lcd.setCursor(9, 3);
        lcd.print(levelSensor_5);

        lcd.setCursor(10, 3);
        lcd.print("|");
        lcd.setCursor(11, 3);
        lcd.print(levelSensor_6);

        lcd.setCursor(12, 3);
        lcd.print("|");
        lcd.setCursor(13, 3);
        lcd.print(potableSolenoidStatus);

        lcd.setCursor(14, 3);
        lcd.print("|");
        lcd.setCursor(15, 3);
        lcd.print(discardSolenoidStatus);

        lcd.setCursor(16, 3);
        lcd.print("|");
        lcd.setCursor(17, 3);
        lcd.print(bombStatus);

        lcd.setCursor(18, 3);
        lcd.print("|");

        if (float(flowRate) > !0)
        {
            // Carrega as informações para serem enviadas em um lote somente para o servidor
            ThingSpeak.setField(1, float(flowRate));         // Fluxo corrente
            ThingSpeak.setField(2, float(totalMilliLitres)); // Total em mililitros
            ThingSpeak.setField(3, float(totalLitres));      // Total em litros
            ThingSpeak.setField(4, reservoirLevel);          // Nivel reservatorio
            ThingSpeak.setField(5, accumulatorLevel);        // Nivel acumulador
            ThingSpeak.setField(6, bombStatus);              // Status da Bomba
            ThingSpeak.setField(7, potableSolenoidStatus);   // Status Solenoide Potavel
            ThingSpeak.setField(8, discardSolenoidStatus);   // Status Solenoide Descarte
            ThingSpeak.setStatus(NUMERO_SERIE);
            ThingSpeak.writeFields(1, apiWriteKey); // Envia o lote de informações para o servidor
        }
        else
        {
            // Carrega a informação do servidor o total acumulada de mililitros
            totalMilliLitres = ThingSpeak.readFloatField(1753394, 2, apiReadKey);
            // Carrega a informação do servidor o total acumulado de litros.
            totalLitres = ThingSpeak.readFloatField(1753394, 3, apiReadKey);
        }
    }
    digitalWrite(LED_STATUS, HIGH);
    delay(500);
}