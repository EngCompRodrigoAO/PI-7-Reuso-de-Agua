//-----------------------------------------------------------------------------------------------------------  BIBLIOTECAS
#include <WiFi.h>
#include <SPI.h>
#include <Wire.h>
#include "ThingSpeak.h"
#include <LiquidCrystal_I2C.h>
#include <MCP23017.h>
#include <ArduinoUniqueID.h>
#include <arduino.h>
#include <time.h>

//-----------------------------------------------------------------------------------------------------------   CONSTANTES
const char *apiWriteKey = ""; // Chave de gravação do ThingSpeak
const char *apiReadKey = "";  // Chave de leitura do ThingSpeak
const char *ssid = "";        // Nome da rede wifi ssid
const char *pass = "";        // Senha da rede wifi
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = -3600 * 3;
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
long DECORRIDOMILLIS = 0;
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
int accumulatorLevel = 0, reservoirLevel = 0, DESCARTE_MAQUINA = 0, CONTADOR_ATUALIZACAO_SERVER = 0, MES_ATUAL, MES_ANTERIOR;
boolean bombPower = 0;
boolean statusLed = 0;
String NUMERO_SERIE = "";
String NIVEL_ACUMULADOR = "NUL", NIVEL_REUSO = "NUL", BOMBA_STATUS = "NULL";
String NIVEL_1 = "", NIVEL_2 = "", NIVEL_3 = "", NIVEL_4 = "", NIVEL_5 = "", NIVEL_6 = "", SOLENOIDE_1 = "", SOLENOIDE_2 = "", BOMBA = "";

// SIMBOLOS ESPECIAIS LCD LOGO UNIVESP
byte SIMB1[8] = {B11100, B01110, B01110, B00111, B00111, B00111, B00011, B00011};
byte SIMB2[8] = {B01111, B00111, B00011, B00011, B00011, B00001, B00001, B00001};
byte SIMB3[8] = {B11110, B11100, B11000, B11000, B11000, B10000, B10000, B10000};
byte SIMB4[8] = {B00111, B01110, B01110, B11100, B11100, B11100, B11000, B11000};
byte SIMB5[8] = {B00011, B00001, B00001, B00000, B00000, B00000, B00000, B00000};
byte SIMB6[8] = {B10000, B10000, B10000, B10000, B10000, B11100, B01111, B01111};
byte SIMB7[8] = {B00001, B00001, B00011, B00011, B00011, B00111, B11110, B11110};
byte SIMB8[8] = {B11000, B10000, B10000, B00000, B00000, B00000, B00000, B00000};

WiFiClient client;
LiquidCrystal_I2C lcd(LCD_ADDRESS, 20, 4); // Seta o endereço do LCD em 0x27 e configura o LCD de 20 colunas e 4 linhas
MCP23017 myMCP = MCP23017(MCP_ADDRESS);

//-----------------------------------------------------------------------------------------------------------       VOIDS
void IRAM_ATTR pulseCounter()
{
    pulseCount++;
}

// acionamento da bomba 1
void BOMBA_ACIONAMENTO(boolean STATUS_BOMBA_1)
{
    if (STATUS_BOMBA_1 == 1 && bombStatus == 0)
    {
        digitalWrite(BOMBA_1, HIGH);
        bombPower = 1;
        bombStatus = 1;
        BOMBA_STATUS = "LIGA";
    }
    else
    {
        digitalWrite(BOMBA_1, LOW);
        bombPower = 0;
        bombStatus = 0;
        BOMBA_STATUS = "DESL";
    }
}

// Acionamento solenoide da agua potável
void SOLOENOIDE_POTAVEL_ACIONAMENTO(boolean STATUS_SOLENOIDE_1)
{
    if (STATUS_SOLENOIDE_1 == 1)
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
    if (STATUS_SOLENOIDE_2 == 1)
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
    Serial.println("SERVIDOR ZERADO.");
}

void PEGAR_HORA()
{
    String MES_TEMP = "";
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Falha ao obter hora");
        return;
    }
    Serial.println(&timeinfo, "%A, %d, %B, %Y, %H, %m, %S");
    Serial.println(&timeinfo, "%d/%m/%Y");
    MES_TEMP = (&timeinfo, "%m");
    MES_ATUAL = MES_TEMP.toInt();
    if (MES_ANTERIOR < MES_ATUAL)
    {
        Serial.println("RESETANDO SERVIDOR PARA O PROXIMO MES");
        RESETA_SERVER();
        MES_ANTERIOR = MES_ATUAL;
        Serial.println("SERVIDOR FOI RESETADO.");
    }
}
//------------------------------------------------------------------------------------------------- CARREGA CONFIGURAÇÕES
void setup()
{
    Serial.begin(9600); // Inicia a comunicação serial
    // UniqueIDdump(Serial);
    NUMERO_SERIE = "MCUDEVICE-";
    // Serial.print("UniqueID: ");
    for (size_t i = 0; i < UniqueIDsize; i++)
    {
        if (UniqueID[i] < 0x10)
            Serial.print("0");

        // Serial.print(UniqueID[i], HEX);
        NUMERO_SERIE += String(UniqueID[i], HEX);
        // Serial.print("");
    }
    Serial.println();

    NUMERO_SERIE.toUpperCase();
    Serial.println(NUMERO_SERIE);
    WiFi.mode(WIFI_STA);      // Coloca o WIFI do ESP32 em modo estação
    ThingSpeak.begin(client); // Inicializa a comunicação com o ThingSpeak

    lcd.init();      // Incializa o LCD
    lcd.backlight(); // Seta a Iluminação do LCD

    // CRIA SIMBOLOS NO LCD
    lcd.createChar(1, SIMB1);
    lcd.createChar(2, SIMB2);
    lcd.createChar(3, SIMB3);
    lcd.createChar(4, SIMB4);
    lcd.createChar(5, SIMB5);
    lcd.createChar(6, SIMB6);
    lcd.createChar(7, SIMB7);
    lcd.createChar(8, SIMB8);

    byte Count = 1;
    lcd.clear();

    // MOSTRA NO LCD OS BYTES PARA SIMBOLOS SIMB1, SIMB2, SIMB3...
    for (byte y = 1; y < 3; y++)
    {
        for (byte x = 8; x < 12; x++)
        {
            lcd.setCursor(x, y);
            lcd.write(Count);
            Count++;
        }
    }

    lcd.setCursor(0, 0);
    lcd.print("UNIVESP");
    lcd.setCursor(14, 3);
    lcd.print("4N88");
    delay(1000);

    myMCP.Init();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" A REUTILIZACAO DA ");
    lcd.setCursor(0, 1);
    lcd.print(" AGUA DE MAQUINA DE ");
    lcd.setCursor(0, 2);
    lcd.print("    LAVAR ROUPAS    ");
    lcd.setCursor(0, 3);
    lcd.print("CARREGANDO");
    delay(3000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" PROJETO INTEGRADOR ");
    lcd.setCursor(0, 1);
    lcd.print("");
    lcd.setCursor(0, 2);
    lcd.print("     GRUPO 4N88     ");
    lcd.setCursor(0, 3);
    lcd.print("CARREGANDO");
    lcd.setCursor(10, 3);
    lcd.write(255);
    delay(3000);

    // lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("");
    lcd.setCursor(0, 1);
    lcd.print("     INTEGRANTES    ");
    lcd.setCursor(0, 2);
    lcd.print("");
    lcd.setCursor(0, 3);
    lcd.print("CARREGANDO");
    lcd.setCursor(11, 3);
    lcd.write(255);
    delay(3000);

    // lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  ANDRE LUIZ PRADO  ");
    lcd.setCursor(0, 1);
    lcd.print("     DOS SANTOS     ");
    lcd.setCursor(0, 2);
    lcd.print("     RA: 1822375    ");
    lcd.setCursor(0, 3);
    lcd.print("CARREGANDO");
    lcd.setCursor(12, 3);
    lcd.write(255);
    delay(3000);

    // lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("     DIEGO ALVES    ");
    lcd.setCursor(0, 1);
    lcd.print("     DOS SANTOS     ");
    lcd.setCursor(0, 2);
    lcd.print("     RA: 1831936    ");
    lcd.setCursor(0, 3);
    lcd.print("CARREGANDO");
    lcd.setCursor(13, 3);
    lcd.write(255);
    delay(3000);

    // lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  ELTON SOLIGUETO   ");
    lcd.setCursor(0, 1);
    lcd.print("");
    lcd.setCursor(0, 2);
    lcd.print("     RA: 1836172    ");
    lcd.setCursor(0, 3);
    lcd.print("CARREGANDO");
    lcd.setCursor(14, 3);
    lcd.write(255);
    delay(3000);

    // lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("   LILIAN ADRIANA   ");
    lcd.setCursor(0, 1);
    lcd.print("  GONZALEZ TENORIO  ");
    lcd.setCursor(0, 2);
    lcd.print("     RA: 1835883    ");
    lcd.setCursor(0, 3);
    lcd.print("CARREGANDO");
    lcd.setCursor(15, 3);
    lcd.write(255);
    delay(3000);

    // lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  RODRIGO DE AVILA  ");
    lcd.setCursor(0, 1);
    lcd.print("      OLIVEIRA      ");
    lcd.setCursor(0, 2);
    lcd.print("     RA: 1826340    ");
    lcd.setCursor(0, 3);
    lcd.print("CARREGANDO");
    lcd.setCursor(16, 3);
    lcd.write(255);
    delay(3000);

    // lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SERGIO LUIZ BARBOSA");
    lcd.setCursor(0, 1);
    lcd.print("");
    lcd.setCursor(0, 2);
    lcd.print("     RA: 1826279    ");
    lcd.setCursor(0, 3);
    lcd.print("CARREGANDO");
    lcd.setCursor(17, 3);
    lcd.write(255);
    delay(3000);

    // lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("   VICTOR MAKTURA   ");
    lcd.setCursor(0, 1);
    lcd.print("");
    lcd.setCursor(0, 2);
    lcd.print("     RA: 1827787    ");
    lcd.setCursor(0, 3);
    lcd.print("CARREGANDO");
    lcd.setCursor(18, 3);
    lcd.write(255);
    delay(3000);

    // lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("     ORIENTADORA    ");
    lcd.setCursor(0, 1);
    lcd.print("    THAIS PEREIRA   ");
    lcd.setCursor(0, 2);
    lcd.print("      DA SILVA      ");
    lcd.setCursor(0, 3);
    lcd.print("CARREGANDO");
    lcd.setCursor(19, 3);
    lcd.write(255);
    delay(3000);
    lcd.clear();

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
    previousMillis = 0;

    /* FAZ A VERIFICAÇÃO SE TEM CONECTIVIDADE COM WIFI CASO TENHA PASSA A FRENTE CASO NÃO FAZ A CONEXÃO COM A REDE
     WIFI PRÉ ESTABELECIDA NA CONSTANTE SSID E PASS.*/
    if (WiFi.status() != WL_CONNECTED)
    {
        lcd.clear();
        lcd.print("AGUARDE CONECTANDO..");
        Serial.println("AGUARDE CONECTANDO.");
        digitalWrite(LED_WIFI, LOW);
        while (WiFi.status() != WL_CONNECTED)
        {
            WiFi.begin(ssid, pass);
            delay(5000);
        }
        lcd.clear();
        lcd.print("Conectado.");
        Serial.println("Conectado.");
        digitalWrite(LED_WIFI, HIGH);
    }
    // CARREGA A INFORMAÇÃO DO SERVIDOR O TOTAL ACUMULADOR DE MILILITROS.
    totalMilliLitres = ThingSpeak.readFloatField(1753394, 2, apiReadKey);
    // CARREGA A INFORMAÇÃO DO SERVIDOR O TOTAL ACUMULADOR DE  LITROS.
    totalLitres = ThingSpeak.readFloatField(1753394, 3, apiReadKey);
    // RESETA_SERVER();

    // Cria uma interrupção para a porta do microcontrolador para o sensor de fluxo que funciona em modo HALL
    attachInterrupt(digitalPinToInterrupt(SENSOR_FLUXO), pulseCounter, FALLING);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); // pegar hora certa do servidor ntp
    PEGAR_HORA();
    delay(2000);
}

void LCD_TESTANDO() // ROTINA ESCREVE NO LCD TESTANDO
{
    Serial.println("INICIO TEXTO TESTANDO");
    for (int x = 0; x <= 5; x++)
    {
        lcd.setCursor(0, 3);
        lcd.print(F("AGUARDE TESTANDO    "));
        delay(200);
        lcd.setCursor(0, 3);
        lcd.print(F("AGUARDE TESTANDO.   "));
        delay(200);
        lcd.setCursor(0, 3);
        lcd.print(F("AGUARDE TESTANDO..  "));
        delay(200);
        lcd.setCursor(0, 3);
        lcd.print(F("AGUARDE TESTANDO... "));
        delay(200);
        lcd.setCursor(0, 3);
        lcd.print(F("AGUARDE TESTANDO...."));
        delay(200);
        Serial.println(x);
    }
    Serial.println("FIM TEXTO TESTANDO");
}
void TESTE_SENSORES() // ROTINA PARA TESTAR SENSORES
{
    if (SENSOR_NIVEL_3 == 0 || SENSOR_NIVEL_3 == 1 && SENSOR_NIVEL_2 == 0) // TESTA SENSORES NIVEL ACUMULADOR
    {
        Serial.println("ERRO: P1 - INICIO");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("! TESTE ACUMULADOR !"));
        lcd.setCursor(0, 1);
        lcd.print(F("SENSOR DE NIVEL MAX"));
        lcd.setCursor(0, 2);
        lcd.print(F("COD.: #ERRO_SN_3_000"));
        lcd.setCursor(0, 3);
        lcd.print(F("!!!! VERIFICAR !!!!"));
        delay(10000);
        // TESTE_SENSORES();
        Serial.println("ERRO: P1 - FIM");
    }
    else
    {
        Serial.println("ERRO: P2 - INICIO");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("! TESTE ACUMULADOR !"));
        lcd.setCursor(0, 1);
        lcd.print(F("  NIVEIS ACUMULADOR "));
        lcd.setCursor(0, 2);
        lcd.print(F("  SENSORES DE NIVEL "));
        LCD_TESTANDO();
        lcd.setCursor(0, 3);
        lcd.print(F("      SENSOR OK     "));
        delay(3000);
        Serial.println("ERRO: P2 - FIM");
        loop();
    }

    if (SENSOR_NIVEL_6 == 1 && SENSOR_NIVEL_5 == 0 && SENSOR_NIVEL_4 == 0) // TESTA SENSORES NIVEL RESERVATÓRIO  NIVEL MÁXIMO COM PROBLEMA
    {
        Serial.println("ERRO: P3 - INICIO");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F(" TESTE RESERVATORIO "));
        lcd.setCursor(0, 1);
        lcd.print(F("SENSOR DE NIVEL MAX"));
        lcd.setCursor(0, 2);
        lcd.print(F("COD.: #ERRO_SN_6_000"));
        lcd.setCursor(0, 3);
        lcd.print(F("!!!! VERIFICAR !!!!"));
        delay(10000);
        // TESTE_SENSORES();
        Serial.println("ERRO: P3 - FIM");
    }
    else
    {
        if (SENSOR_NIVEL_6 == 0 && SENSOR_NIVEL_5 == 1 && SENSOR_NIVEL_4 == 0) // TESTA SENSORES NIVEL RESERVATÓRIO NIVEL MÉDIO COM PROBLEMA
        {
            Serial.println("ERRO: P4 - INICIO");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(F(" TESTE RESERVATORIO "));
            lcd.setCursor(0, 1);
            lcd.print(F("SENSOR DE NIVEL MED"));
            lcd.setCursor(0, 2);
            lcd.print(F("COD.: #ERRO_SN_5_000"));
            lcd.setCursor(0, 3);
            lcd.print(F("!!!! VERIFICAR !!!!"));
            delay(10000);
            // TESTE_SENSORES();
            Serial.println("ERRO: P4 - FIM");
        }
        else
        {
            if (SENSOR_NIVEL_6 == 1 && SENSOR_NIVEL_5 == 1 && SENSOR_NIVEL_4 == 0) // TESTA SENSORES NIVEL RESERVATÓRIO NIVEL MINIMO COM PROBLEMA
            {
                Serial.println("ERRO: P5 - INICIO");
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print(F(" TESTE RESERVATORIO "));
                lcd.setCursor(0, 1);
                lcd.print(F("SENSOR DE NIVEL MIN"));
                lcd.setCursor(0, 2);
                lcd.print(F("COD.: #ERRO_SN_4_000"));
                lcd.setCursor(0, 3);
                lcd.print(F("!!!! VERIFICAR !!!!"));
                delay(10000);
                // TESTE_SENSORES();
                Serial.println("ERRO: P5 - FIM");
            }
            else
            {
                if (SENSOR_NIVEL_6 == 1 && SENSOR_NIVEL_5 == 0 && SENSOR_NIVEL_4 == 1) // TESTA SENSORES NIVEL RESERVATÓRIO TODOS OS NIVEIS COM PROBLEMA
                {
                    Serial.println("ERRO: P6 - INICIO");
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print(F(" TESTE RESERVATORIO "));
                    lcd.setCursor(0, 1);
                    lcd.print(F("SENSOR DE NIVEL ***"));
                    lcd.setCursor(0, 2);
                    lcd.print(F("COD.: #ERRO_SN_#_000"));
                    lcd.setCursor(0, 3);
                    lcd.print(F("!!!! VERIFICAR !!!!"));
                    delay(10000);
                    // TESTE_SENSORES();
                    Serial.println("ERRO: P6 - FIM");
                }
                else
                {
                    if (SENSOR_NIVEL_6 != 1 || SENSOR_NIVEL_6 != 0 || SENSOR_NIVEL_5 != 1 || SENSOR_NIVEL_5 != 0 || SENSOR_NIVEL_4 != 1 || SENSOR_NIVEL_4 != 0) // TESTA SENSORES NIVEL RESERVATÓRIO
                    {
                        Serial.println("ERRO: P7 - INICIO");
                        lcd.clear();
                        lcd.setCursor(0, 0);
                        lcd.print(F(" TESTE RESERVATORIO "));
                        lcd.setCursor(0, 1);
                        lcd.print(F("SENSOR DE NIVEL ***"));
                        lcd.setCursor(0, 2);
                        lcd.print(F("COD.: #ERRO_SN_#_000"));
                        lcd.setCursor(0, 3);
                        lcd.print(F("!!!! VERIFICAR !!!!"));
                        delay(10000);
                        // TESTE_SENSORES();
                        Serial.println("ERRO: P7 - FIM");
                    }
                    else
                    {
                        Serial.println("ERRO: P8 - INICIO");
                        lcd.clear();
                        lcd.setCursor(0, 0);
                        lcd.print(F(" TESTE RESERVATORIO "));
                        lcd.setCursor(0, 1);
                        lcd.print(F("NIVEIS RESERVATORIO"));
                        lcd.setCursor(0, 2);
                        lcd.print(F(" SENSORES DE NIVEL "));
                        LCD_TESTANDO();
                        lcd.setCursor(0, 3);
                        lcd.print(F("      SENSOR OK     "));
                        delay(3000);
                        Serial.println("ERRO: 8 - FIM");
                        loop();
                    }
                }
            }
        }
    }
}
void ERRO_SISTEMA(int SITUACAO_SISTEMA, String ERRO_LOCAL, String ERRO_DESCRICAO) // ACIONA EM CASO DE ALGUM ERRO PARA PROTEGER O SISTEMA
{
    BOMBA_ACIONAMENTO(0);
    SOLOENOIDE_POTAVEL_ACIONAMENTO(1);
    SOLOENOIDE_REUSO_ACIONAMENTO(0);
    ThingSpeak.setStatus("ERRO: " + ERRO_LOCAL + " - VERIFICAR: " + ERRO_DESCRICAO);
    ThingSpeak.writeFields(1, apiWriteKey); // Envia o lote de informações para o servidor
    lcd.clear();
    while (SITUACAO_SISTEMA == 1)
    {
        lcd.setCursor(0, 0);
        lcd.print(F("!!! ERRO !! ERRO !!!"));
        lcd.setCursor(0, 1);
        lcd.print(ERRO_LOCAL);
        lcd.setCursor(0, 2);
        lcd.print(F(" REINICIAR SISTEMA! "));
        lcd.setCursor(0, 3);
        lcd.print(ERRO_DESCRICAO);
        delay(10000);
        for (int x = 0; x <= 5; x++)
        {
            lcd.setCursor(0, 3);
            lcd.print(F("                    "));
            delay(1000);
            lcd.setCursor(0, 3);
            lcd.print(ERRO_DESCRICAO);
            delay(500);
            lcd.noBacklight();
            delay(500);
            lcd.backlight();
            delay(500);
        }
        TESTE_SENSORES();
    }
}

//------------------------------------------------------------------------------------- Loop infinito do microcontrolador
void loop()
{
    DECORRIDOMILLIS = millis();
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
        Serial.print(bombStatus); // Envia para Serial Monitor Status Bomba

        // ACIONAMENTO DESCARTE DE AGUA
        if (levelSensor_1 == 1 && DESCARTE_MAQUINA == 0)
        {
            SOLOENOIDE_REUSO_ACIONAMENTO(0);
            DESCARTE_MAQUINA++;
        }
        else
        {
            if (levelSensor_1 == 0 && DESCARTE_MAQUINA == 0)
            {
                SOLOENOIDE_REUSO_ACIONAMENTO(0);
                // DESCARTE_MAQUINA++;
            }
            else
            {
                if (levelSensor_1 == 1 && DESCARTE_MAQUINA == 1)
                {
                    SOLOENOIDE_REUSO_ACIONAMENTO(1);
                    DESCARTE_MAQUINA++;
                }
                else
                {
                    if (levelSensor_1 == 0 && DESCARTE_MAQUINA == 1)
                    {
                        SOLOENOIDE_REUSO_ACIONAMENTO(0);
                        DESCARTE_MAQUINA++;
                    }
                    else
                    {
                        if (levelSensor_1 == 0 && DESCARTE_MAQUINA == -1)
                        {
                            DESCARTE_MAQUINA = 1;
                        }
                    }
                }
            }
        }

        // NIVEL DO ACUMULADOR
        if (levelSensor_2 == 1 && levelSensor_3 == 1)
        {
            bombStatus = 0;
            NIVEL_ACUMULADOR = "MAX";
            accumulatorLevel = 100;
            if (reservoirLevel == 50 || reservoirLevel == 10 || reservoirLevel == 0)
            {
                BOMBA_ACIONAMENTO(1);
            }
            else
            {
                BOMBA_ACIONAMENTO(0);
            }
        }
        else
        {
            if (levelSensor_2 == 1 && levelSensor_3 == 0)
            {
                bombStatus = 1;
                NIVEL_ACUMULADOR = "MIN";
                accumulatorLevel = 10;
            }
            else
            {
                if (levelSensor_2 == 0 && levelSensor_3 == 0)
                {
                    bombStatus = 0;
                    NIVEL_ACUMULADOR = "---";
                    accumulatorLevel = 0;
                    BOMBA_ACIONAMENTO(0);
                }
                else
                {
                    if (levelSensor_2 == 0 || levelSensor_2 == 1 && levelSensor_3 == 0 || levelSensor_3 == 1)
                    {
                        bombStatus = 0;
                        NIVEL_ACUMULADOR = "---";
                        accumulatorLevel = 0;
                        BOMBA_ACIONAMENTO(0);
                    }
                    else
                    {
                        NIVEL_ACUMULADOR = "ERR";
                        accumulatorLevel = 0;
                        ERRO_SISTEMA(1, "VERIFICAR ACUMULADOR", "SENSORES DE NIVEL");
                    }
                }
            }
        }

        // NIVEL DO RESERVATORIO DE REUSO
        if (levelSensor_4 == 1 && levelSensor_5 == 0 && levelSensor_6 == 0)
        {
            bombStatus = 0;
            NIVEL_REUSO = "MIM";
            reservoirLevel = 10;
            SOLOENOIDE_POTAVEL_ACIONAMENTO(0);
            if (accumulatorLevel == 10 || accumulatorLevel == 0)
            {
                BOMBA_ACIONAMENTO(0);
            }
            else
            {
                BOMBA_ACIONAMENTO(1);
            }
        }
        else
        {
            if (levelSensor_4 == 1 && levelSensor_5 == 1 && levelSensor_6 == 0)
            {
                bombStatus = 0;
                NIVEL_REUSO = "MED";
                reservoirLevel = 50;
                SOLOENOIDE_POTAVEL_ACIONAMENTO(0);
                if (accumulatorLevel == 10 || accumulatorLevel == 0)
                {
                    BOMBA_ACIONAMENTO(0);
                }
                else
                {
                    BOMBA_ACIONAMENTO(1);
                }
            }
            else
            {
                if (levelSensor_4 == 1 && levelSensor_5 == 1 && levelSensor_6 == 1)
                {
                    bombStatus = 1;
                    NIVEL_REUSO = "MAX";
                    reservoirLevel = 100;
                    SOLOENOIDE_POTAVEL_ACIONAMENTO(0);
                    BOMBA_ACIONAMENTO(0);
                }
                else
                {
                    if (levelSensor_4 == 0 && levelSensor_5 == 0 && levelSensor_6 == 0)
                    {
                        bombStatus = 0;
                        SOLOENOIDE_POTAVEL_ACIONAMENTO(1);
                        NIVEL_REUSO = "---";
                        reservoirLevel = 0;
                        if (accumulatorLevel == 10 || accumulatorLevel == 0)
                        {
                            BOMBA_ACIONAMENTO(0);
                        }
                        else
                        {
                            BOMBA_ACIONAMENTO(1);
                        }
                    }
                    else
                    {
                        if (levelSensor_4 == 0 || levelSensor_4 == 1 && levelSensor_5 == 0 || levelSensor_5 == 1 && levelSensor_6 == 0 || levelSensor_6 == 1)
                        {
                            bombStatus = 0;
                            SOLOENOIDE_POTAVEL_ACIONAMENTO(1);
                            NIVEL_REUSO = "---";
                            reservoirLevel = 0;
                            BOMBA_ACIONAMENTO(1);
                        }
                        else
                        {
                            bombStatus = 1;
                            SOLOENOIDE_POTAVEL_ACIONAMENTO(1);
                            BOMBA_ACIONAMENTO(0);
                            NIVEL_REUSO = "ERR";
                            reservoirLevel = 0;
                            ERRO_SISTEMA(1, "VERIFICAR RESERVATORIO", "SENSORES DE NIVEL");
                        }
                    }
                }
            }
        }

        // Escreve no Display LCD
        lcd.clear(); // Limpa o LCD
        lcd.setCursor(0, 0);
        lcd.print(F("FLUXO"));
        lcd.setCursor(5, 0);
        lcd.print(F("|"));
        lcd.setCursor(6, 0);
        lcd.print(F("TOTAL"));
        lcd.setCursor(11, 0);
        lcd.print(F("|"));
        lcd.setCursor(12, 0);
        lcd.print(F("ACU"));
        lcd.setCursor(15, 0);
        lcd.print(F("|"));
        lcd.setCursor(16, 0);
        lcd.print(F("REU"));

        lcd.setCursor(0, 1);
        lcd.print(int(flowRate));
        lcd.setCursor(2, 1);
        lcd.print(F("L/m"));
        lcd.setCursor(5, 1);
        lcd.print(F("|"));
        lcd.setCursor(6, 1);
        lcd.print(int(totalLitres));
        lcd.setCursor(10, 1);
        lcd.print(F("L"));
        lcd.setCursor(11, 1);
        lcd.print(F("|"));
        lcd.setCursor(12, 1);
        lcd.print(NIVEL_ACUMULADOR);
        lcd.setCursor(15, 1);
        lcd.print(F("|"));
        lcd.setCursor(16, 1);
        lcd.print(NIVEL_REUSO);

        lcd.setCursor(0, 2);
        lcd.print(F("B1"));
        lcd.setCursor(2, 2);
        if (levelSensor_1 == 1)
        {
            lcd.write(255);
        }
        else
        {
            lcd.write(219);
        }
        lcd.setCursor(3, 2);
        lcd.print("|");
        lcd.setCursor(4, 2);
        lcd.print(F("B3"));
        lcd.setCursor(6, 2);
        if (levelSensor_3 == 1)
        {
            lcd.write(255);
        }
        else
        {
            lcd.write(219);
        }
        lcd.setCursor(7, 2);
        lcd.print("|");
        lcd.setCursor(8, 2);
        lcd.print(F("B5"));
        lcd.setCursor(10, 2);
        if (levelSensor_5 == 1)
        {
            lcd.write(255);
        }
        else
        {
            lcd.write(219);
        }
        lcd.setCursor(11, 2);
        lcd.print("|");
        lcd.setCursor(12, 2);
        lcd.print(F("S1"));
        lcd.setCursor(14, 2);
        if (discardSolenoidStatus == 1)
        {
            lcd.write(255);
        }
        else
        {
            lcd.write(219);
        }
        lcd.setCursor(15, 2);
        lcd.print("|");
        lcd.setCursor(16, 2);
        lcd.print(F("BOMB"));

        lcd.setCursor(0, 3);
        lcd.print(F("B2"));
        lcd.setCursor(2, 3);
        if (levelSensor_2 == 1)
        {
            lcd.write(255);
        }
        else
        {
            lcd.write(219);
        }
        lcd.setCursor(3, 3);
        lcd.print("|");
        lcd.setCursor(4, 3);
        lcd.print(F("B4"));
        lcd.setCursor(6, 3);
        if (levelSensor_4 == 1)
        {
            lcd.write(255);
        }
        else
        {
            lcd.write(219);
        }
        lcd.setCursor(7, 3);
        lcd.print("|");
        lcd.setCursor(8, 3);
        lcd.print(F("B6"));
        lcd.setCursor(10, 3);
        if (levelSensor_6 == 1)
        {
            lcd.write(255);
        }
        else
        {
            lcd.write(219);
        }
        lcd.setCursor(11, 3);
        lcd.print("|");
        lcd.setCursor(12, 3);
        lcd.print(F("S2"));
        lcd.setCursor(14, 3);
        if (potableSolenoidStatus == 1)
        {
            lcd.write(255);
        }
        else
        {
            lcd.write(219);
        }
        lcd.setCursor(15, 3);
        lcd.print("|");
        lcd.setCursor(16, 3);
        lcd.print(BOMBA_STATUS);

        if (CONTADOR_ATUALIZACAO_SERVER == 192)
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
            CONTADOR_ATUALIZACAO_SERVER = 0;
        }
        /*else
        {
            // Carrega a informação do servidor o total acumulada de mililitros
            totalMilliLitres = ThingSpeak.readFloatField(1753394, 2, apiReadKey);
            // Carrega a informação do servidor o total acumulado de litros.
            totalLitres = ThingSpeak.readFloatField(1753394, 3, apiReadKey);
        }*/
        CONTADOR_ATUALIZACAO_SERVER++;
        digitalWrite(LED_STATUS, HIGH);
        Serial.print(" | "); // Envia para Serial Monitor a separação
        Serial.print("TEMPO: ");
        Serial.print((millis() - DECORRIDOMILLIS)); // Envia para Serial Monitor Tempo decorrido em ms
        Serial.println("ms");
        delay(500);
    }
}