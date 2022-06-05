//-----------------------------------------------------------------------------------------------------------  BIBLIOTECAS
#include <WiFi.h>
#include <SPI.h>
#include <Wire.h>
#include "ThingSpeak.h"
#include <LiquidCrystal_I2C.h>

//-----------------------------------------------------------------------------------------------------------   CONSTANTES
const char *apiWriteKey = "78HR62O527P424BP"; // Chave de gravação do ThingSpeak
const char *apiReadKey = "BNZX34W2JPUZHRWO";  // Chave de leitura do ThingSpeak
const char *ssid = "FALANGE_SUPREMA";         // Nome da rede wifi ssid
const char *pass = "#kinecs#";                // Senha da rede wifi
//-----------------------------------------------------------------------------------------------------------  DEFINIÇÕES
#define SENSOR_FLUXO 27
#define SENSOR_NIVEL_1 12
#define SENSOR_NIVEL_2 13
#define SENSOR_NIVEL_3 14
#define SENSOR_NIVEL_4 25
#define SENSOR_NIVEL_5 26
#define BOMBA_1 32
#define SOLENOIDE_POTAVEL 34
#define SOLENOIDE_DESCARTE 35
#define LED_STATUS 2

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
boolean levelSensor_1 = 0, levelSensor_2 = 0, levelSensor_3 = 0, levelSensor_4 = 0, levelSensor_5 = 0;
int accumulatorLevel = 0;
int reservoirLevel = 0;
boolean bombPower = 0;
boolean statusLed = 0;

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
void SLOENOIDE_POTAVEL_ACIONAMENTO(boolean STATUS_SOLENOIDE_1)
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
void SLOENOIDE_REUSO_ACIONAMENTO(boolean STATUS_SOLENOIDE_2)
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



WiFiClient client;
LiquidCrystal_I2C lcd(0x27, 20, 4); // Seta o endereço do LCD em 0x27 e configura o LCD de 20 colunas e 4 linhas
//------------------------------------------------------------------------------------------------- CARREGA CONFIGURAÇÕES
void setup()
{
    Serial.begin(9600);       // Inicia a comunicação serial
    WiFi.mode(WIFI_STA);      // Coloca o WIFI do ESP32 em modo estação
    ThingSpeak.begin(client); // Inicializa a comunicação com o ThingSpeak
    lcd.init();               // Incializa o LCD
    lcd.backlight();          // Seta a Iluminação do LCD

    /* Faz a verificação se tem conectividade wifi caso tenha passa a frente caso não faz a conexão com a rede
     WIFI pré estabelecida na cosntante ssid e pass.*/
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("AGUARDE CONECTANDO...");
        while (WiFi.status() != WL_CONNECTED)
        {
            WiFi.begin(ssid, pass);
            delay(5000);
        }
        Serial.println("Conectado.");
    }
    // configurar as portas do microcontrolador como entradas de nivel alto
    pinMode(SENSOR_FLUXO, INPUT_PULLUP);   // Seta o comportamento da porta do microcontrolador
    pinMode(SENSOR_NIVEL_1, INPUT_PULLUP); // Seta a porta 12 do microcontrolador para sensor nivel 1 como entrada
    pinMode(SENSOR_NIVEL_2, INPUT_PULLUP); // Seta a porta 13 do microcontrolador para sensor nivel 1 como entrada
    pinMode(SENSOR_NIVEL_3, INPUT_PULLUP); // Seta a porta 14 do microcontrolador para sensor nivel 1 como entrada
    pinMode(SENSOR_NIVEL_4, INPUT_PULLUP); // Seta a porta 25 do microcontrolador para sensor nivel 1 como entrada
    pinMode(SENSOR_NIVEL_5, INPUT_PULLUP); // Seta a porta 26 do microcontrolador para sensor nivel 1 como entrada

    // configurar as portas do microcontrolador como saidas
    pinMode(LED_STATUS, OUTPUT);         // Seta a porta 2 do microcontrolador para led de status como saida
    pinMode(SOLENOIDE_POTAVEL, OUTPUT);  // Seta a porta 34 do microcontrolador para solenoide potavel como saida
    pinMode(SOLENOIDE_DESCARTE, OUTPUT); // Seta a porta 35 do microcontrolador para solenoide descarte como saida

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
            while (WiFi.status() != WL_CONNECTED)
            {
                WiFi.begin(ssid, pass);
                delay(5000);
            }
            Serial.println("Conectado.");
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

        // Imprima a vazão para este segundo em litros/minuto
        Serial.print("FLUXO: ");
        Serial.print(float(flowRate)); // Envia para Serial Monitor o FLUXO
        Serial.print("L/min");
        Serial.print("\t"); // Envia para Serial Monitor a tabula
        Serial.print("QUANTIDADE DE LIQUIDO: ");
        Serial.print(totalMilliLitres); // Envia para Serial Monitor o total em mililitros
        Serial.print("mL / ");
        Serial.print(totalLitres); // Envia para Serial Monitor o total em litros
        Serial.println("L");
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
            // Escreve no Display LCD
            lcd.clear(); // Limpa o LCD
            lcd.setCursor(1, 0);
            lcd.write(float(flowRate)); // Escreve no LCD o FLUXO
            lcd.setCursor(1, 0);
            lcd.write(totalMilliLitres); // Escreve no LCD o total em mililitros
            lcd.setCursor(1, 0);
            lcd.write(totalLitres); // Escreve no LCD o total em litros

            if (totalMilliLitres > tempflowMilliLitres)
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
                ThingSpeak.writeFields(1, apiWriteKey);          // Envia o lote de informações para o servidor
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
}