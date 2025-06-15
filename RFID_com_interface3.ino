// bibliotecas, a biblioteca MRFC522 é a da placa RFID e a SPI é para fazer a comunicação, conectar o MFRC522 a ESP 32
#include <MFRC522.h>
#include <SPI.h>


#define SS_PIN    21 // pino usado para selecionar o leitor RFID (pino SPI).
#define RST_PIN   22 // pino para resetar o leitor RFID.
#define SIZE_BUFFER     18 // Tamanho do buffer + 2 bytes do CRC. CRC é como se fosse um digito verificador, é usado para a segurança, autenticação
#define MAX_SIZE_BLOCK  16 // Máximo de bytes em um bloco

// Pinos dos leds
#define pinVerde     12 
#define pinVermelho  32

MFRC522::MIFARE_Key key; // Objeto chave que vai ser usado para autenticação, MIFARE é da biblioteca do MRFC
MFRC522::StatusCode status; // Código do status de retorno da autenticação.

MFRC522 mfrc522(SS_PIN, RST_PIN); // Inicializa os pinos

void setup() {
  Serial.begin(9600); // Inicia a serial
  SPI.begin(); // Inicia a SPI

  pinMode(pinVerde, OUTPUT);
  pinMode(pinVermelho, OUTPUT);
  
  mfrc522.PCD_Init(); // Inicia a antena MRFC522
  Serial.println("Aproxime o seu cartao do leitor..."); // Mensagem que é mostrada na serial da ide do arduino
  Serial.println();
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) return; // Aguarda a aproxima do cartão
  if (!mfrc522.PICC_ReadCardSerial()) return; // Verifica se foi lido corretamente

  int opcao = menu(); // Lê a opção enviada pela serial. Não tem muito uso pois coloquei todo esse processo de escolha no python.
  if (opcao == 0)
    leituraDados();
  else if (opcao == 1)
    gravarDados();
  else {
    Serial.println(F("Opção Incorreta!"));
    return;
  }
// Esses são métodos usado para parar a leitura do cartão
  mfrc522.PICC_HaltA(); 
  mfrc522.PCD_StopCrypto1();  
}

void leituraDados() {
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  byte buffer[SIZE_BUFFER] = {0};
  byte bloco = 1; // A operação será feita no bloco 1, visto que o bloco 0 já vem com o número do cartão
  byte tamanho = SIZE_BUFFER;

// Número do cartão - UID - Identificador único do cartão
  Serial.print("UID:");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) Serial.print("0");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

// Autenticação que é feita no cartão, onde será passado a constante que é definida internamente no bloco, o bloco, o endereço da chave, o UID
// Se a autenticação falhar, pisca o led vermelho
  if (mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, bloco, &key, &(mfrc522.uid)) != MFRC522::STATUS_OK) {
    Serial.println("Authentication failed");
    piscarLED(pinVermelho);
    return;
  }
// Faz a leitura do cartão, o bloco, o buffer e o tamanho se não der certo, pisca o led vermelho
  if (mfrc522.MIFARE_Read(bloco, buffer, &tamanho) != MFRC522::STATUS_OK) {
    Serial.println("Reading failed");
    piscarLED(pinVermelho);
    return;
  }
// Se conseguir ler os dados do cartão, pisca o led verde
  piscarLED(pinVerde);

  Serial.print("DADOS_LIDOS:"); // Envia os dados lidos para o python pela serial
  
  // Imprimi os dados lidos
  for (uint8_t i = 0; i < MAX_SIZE_BLOCK; i++) {
    if (buffer[i] >= 32 && buffer[i] <= 126) {
      Serial.print((char)buffer[i]);
    }
  }
  Serial.println();
}

// Gravar os dados no cartão
void gravarDados() {
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  byte buffer[MAX_SIZE_BLOCK] = "";
  byte bloco = 1; // A operação será feita no bloco 1
  byte tamanhoDados;

  Serial.setTimeout(30000L); // Tempo de 30 segundos para gravar os dados depois de ter escolhido a opção de gravar dados
  Serial.println(F("Insira os dados a serem gravados com o caractere '#' ao final\n[máximo de 16 caracteres]:"));

  tamanhoDados = Serial.readBytesUntil('#', (char*)buffer, MAX_SIZE_BLOCK); // Terá que colocar # no final do que quer que grave para ser gravado
  for (byte i = tamanhoDados; i < MAX_SIZE_BLOCK; i++) buffer[i] = ' ';

  String str = (char*)buffer;
  Serial.println(str);

// Autenticação, a mesma feita na autenticação de leitura, se der erro pisca o led vermelho
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A,
                                    bloco, &key, &(mfrc522.uid));

  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    piscarLED(pinVermelho);
    return;
  }

  status = mfrc522.MIFARE_Write(bloco, buffer, MAX_SIZE_BLOCK); // Tenta gravar os dados no cartão. Se der certo, pisca o led verde
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
// Função do menu, onde lê a opção que o usuário escolher
int menu() {
  Serial.println(F("\nEscolha uma opção:"));
  Serial.println(F("0 - Leitura de Dados"));
  Serial.println(F("1 - Gravação de Dados\n"));

  while (!Serial.available()) {}; // Fica aguardando enquanto o usuário não enviar algo

  int op = (int)Serial.read(); // Lê a opção escolhida
  // Remove os próximos dados, como enter ou \n
  while (Serial.available()) {
    if (Serial.read() == '\n') break;
    Serial.read();
  }
  return (op - 48); // Subtrai 48 da tabela ascii, isso por causa da posição, pois tem 48 caracteres antes da letra a.
}

// Pisca o led por 1 segundo
void piscarLED(int pino) {
  digitalWrite(pino, HIGH);
  delay(1000);
  digitalWrite(pino, LOW);
}
