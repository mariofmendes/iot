#include <MFRC522.h>
#include <SPI.h>

#define SS_PIN    21
#define RST_PIN   22
#define SIZE_BUFFER     18
#define MAX_SIZE_BLOCK  16

#define pinVerde     12
#define pinVermelho  32

MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;

MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  SPI.begin();

  pinMode(pinVerde, OUTPUT);
  pinMode(pinVermelho, OUTPUT);
  
  mfrc522.PCD_Init(); 
  Serial.println("Aproxime o seu cartao do leitor...");
  Serial.println();
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  int opcao = menu();
  if (opcao == 0)
    leituraDados();
  else if (opcao == 1)
    gravarDados();
  else {
    Serial.println(F("Opção Incorreta!"));
    return;
  }

  mfrc522.PICC_HaltA(); 
  mfrc522.PCD_StopCrypto1();  
}

void leituraDados() {
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  byte buffer[SIZE_BUFFER] = {0};
  byte bloco = 1;
  byte tamanho = SIZE_BUFFER;

  Serial.print("UID:");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) Serial.print("0");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  if (mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, bloco, &key, &(mfrc522.uid)) != MFRC522::STATUS_OK) {
    Serial.println("Authentication failed");
    piscarLED(pinVermelho);
    return;
  }

  if (mfrc522.MIFARE_Read(bloco, buffer, &tamanho) != MFRC522::STATUS_OK) {
    Serial.println("Reading failed");
    piscarLED(pinVermelho);
    return;
  }

  piscarLED(pinVerde);

  Serial.print("DADOS_LIDOS:");
  for (uint8_t i = 0; i < MAX_SIZE_BLOCK; i++) {
    if (buffer[i] >= 32 && buffer[i] <= 126) {
      Serial.print((char)buffer[i]);
    }
  }
  Serial.println();
}

void gravarDados() {
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  byte buffer[MAX_SIZE_BLOCK] = "";
  byte bloco = 1;
  byte tamanhoDados;

  Serial.setTimeout(30000L);
  Serial.println(F("Insira os dados a serem gravados com o caractere '#' ao final\n[máximo de 16 caracteres]:"));

  tamanhoDados = Serial.readBytesUntil('#', (char*)buffer, MAX_SIZE_BLOCK);
  for (byte i = tamanhoDados; i < MAX_SIZE_BLOCK; i++) buffer[i] = ' ';

  String str = (char*)buffer;
  Serial.println(str);

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A,
                                    bloco, &key, &(mfrc522.uid));

  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    piscarLED(pinVermelho);
    return;
  }

  status = mfrc522.MIFARE_Write(bloco, buffer, MAX_SIZE_BLOCK);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Write() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    piscarLED(pinVermelho);
    return;
  } else {
    Serial.println(F("MIFARE_Write() success: "));
    piscarLED(pinVerde);
  }
}

int menu() {
  Serial.println(F("\nEscolha uma opção:"));
  Serial.println(F("0 - Leitura de Dados"));
  Serial.println(F("1 - Gravação de Dados\n"));

  while (!Serial.available()) {};

  int op = (int)Serial.read();
  while (Serial.available()) {
    if (Serial.read() == '\n') break;
    Serial.read();
  }
  return (op - 48);
}

void piscarLED(int pino) {
  digitalWrite(pino, HIGH);
  delay(1000);
  digitalWrite(pino, LOW);
}
