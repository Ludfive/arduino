#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>
#include "DHT.h"
#include <Wire.h>
#include <RTClib.h>
#define DHTPIN A1 // pino que estamos conectado
#define DHTTYPE DHT11 // DHT 11

RTC_DS1307 rtc;
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
SoftwareSerial mySerial(52, 50); // RX, TX
const int sensorPin = A0;

String stringRecebida = "";
String horaDesligou = "";

int avisoValidade = 0;
int pos = 1;
bool achou = false;
bool ligado = false;

int temp() {
  return dht.readTemperature();
}

int umid() {
  return dht.readHumidity();
}

String obterData() {
  DateTime now = rtc.now();

  String string = String(now.day()) + "/" + String(now.month()) + "/" + String(now.year());
  return string;
}

String obterHorario() {
  DateTime now = rtc.now();
  String horario = String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());
  return horario;
}

void atualizarBlocoRelatorio() {
  int temperatura = temp();
  int umidade = umid();
  String stringAtual = obterData();
  String horarioAtual = obterHorario();
  
  String stringRelatorio = "Temperatura: " + String(temperatura) + "\nUmidade: " + String(umidade) +
                      "\nData: " + stringAtual + " Hora: " + horarioAtual;

  escreveStringEEPROM(1, stringRelatorio);
}


void relatorio() {
  int qtde = EEPROM.read(0);
  String msg = (leStringEEPROM(1)) + "\n";
  for (int i = 1; i <= qtde; ++i) {
    String stringArmazenada = leStringEEPROM(i * 100);
    int qtdeEspaco = stringArmazenada.lastIndexOf(" ");
    if (qtdeEspaco != -1) {
      String nomeProduto = stringArmazenada.substring(0, qtdeEspaco);
      String diasVencimentoStr = stringArmazenada.substring(qtdeEspaco + 1);
      DateTime stringVencimento = DateTime(
          diasVencimentoStr.substring(6).toInt(),
          diasVencimentoStr.substring(3, 5).toInt(),
          diasVencimentoStr.substring(0, 2).toInt(),
          0, 0, 0
      );
      int diasVencimento = (stringVencimento - rtc.now()).days();
      if (diasVencimento < 10) {
        msg += String(i) + " - ";
        msg += nomeProduto;
        if (diasVencimento == 0) {
          msg += " expira hoje!\n";
        }
        else if (diasVencimento < 0) {
          msg += " já expirou!\n";
        }
        else {
          msg += " expira em ";
          msg += diasVencimento;
          msg += " dias.\n";
        }
      }
    } else {
      Serial.println("Erro: Formato de dados do produto inválido.");
    }
  }
  if(horaDesligou.length() > 2)
    msg += "\n\nUltimo relatório antes de desligar:\n" + horaDesligou;
  mySerial.println(msg);
}

void escreveStringEEPROM(int endereco, const String &string) {
  EEPROM.write(endereco, string.length());
  endereco++;
  for (int i = 0; i < string.length(); ++i) {
    EEPROM.write(endereco + i, string[i]);
  }
  EEPROM.write(endereco + string.length(), '\0');
}

String leStringEEPROM(int endereco) {
  String string = "";
  int length = EEPROM.read(endereco);
  endereco++;
  for (int i = 0; i < length; ++i) {
    string += char(EEPROM.read(endereco + i));
  }
  
  return string;
}

void listaStringsArmazenadas() {
  String msg = "Alimentos\n";
  int qtde = EEPROM.read(0);

  for (int i = 1,  j = 1; i <= qtde; ++i, j++) {
    String string = leStringEEPROM(i * 100);
    msg += String(j) + " - " + string + "\n";
  }

  // imprimir no Serial
  Serial.println(msg);

  // enviar via Bluetooth
  mySerial.println(msg);
}


void limpaEEPROM() {
  Serial.println("Excluindo todas as strings armazenadas na EEPROM...");
  EEPROM.write(0, 0);
  for (int i = 2; i <= EEPROM.read(0) * 100; ++i) {
    EEPROM.write(i, 0);
  }
  
  Serial.println("Tudo foi excluído!");
}

void deletaString(int index) {
  int qtde = EEPROM.read(0);

  if (index > 0 && index <= qtde) {
    Serial.print("Excluindo string no índice ");
    Serial.println(index);

    int enderecoParaDeletar = index * 100;

    int lengthDeletar = EEPROM.read(enderecoParaDeletar);
    for (int i = index + 1; i <= qtde; ++i) {
      String proxString = leStringEEPROM(i * 100);
      escreveStringEEPROM((i - 1) * 100, proxString);
    }

    qtde--;
    EEPROM.write(0, qtde);

    
    for (int i = (qtde + 1) * 100; i < (qtde + 2) * 100; ++i) { // evitar duplicar
      EEPROM.write(i, 0);
    }

    Serial.println("String excluída com sucesso!");
  } else {
    Serial.println("Índice inválido. Nenhuma string excluída.");
  }
}

void processarComando(const String &comando) {
  if (comando.startsWith("OK") || comando.startsWith("+DISC:SUCCESS")) {
    Serial.println("String ignorada: " + comando);
  } else if(comando.equals("consultar")){
    listaStringsArmazenadas();
  } else if (comando.equals("excluir")) {
    limpaEEPROM();
  } else if (comando.equals("relatorio")) {
    relatorio();
  } else if (comando.startsWith("excluir ")) {
    int index = comando.substring(8).toInt();
    deletaString(index);
  } else {
    // entra somente alimentos
    int qtde = EEPROM.read(0);
    qtde++;
    EEPROM.write(0, qtde);
    escreveStringEEPROM(qtde * 100, comando);
    Serial.print("Quantidade de strings armazenadas: ");
    Serial.println(qtde);
  }
}

String luz(){
  int valorLuz = analogRead(sensorPin);
  if (valorLuz > 500) {
    return "Geladeira fechada";
  }
  else {
    return "Geladeira aberta";
  }
}

void mostrarlcd()
{
  String msg = "";
  int qtde = EEPROM.read(0);
  lcd.clear();
  for (;pos <= qtde && !achou; pos++) {
    String stringArmazenada = leStringEEPROM(pos * 100);
    int qtdeEspaco = stringArmazenada.lastIndexOf(" ");
    if (qtdeEspaco != -1) {
      String nomeProduto = stringArmazenada.substring(0, qtdeEspaco);
      String diasVencimentoStr = stringArmazenada.substring(qtdeEspaco + 1);
      DateTime stringVencimento = DateTime(
          diasVencimentoStr.substring(6).toInt(),
          diasVencimentoStr.substring(3, 5).toInt(),
          diasVencimentoStr.substring(0, 2).toInt(),
          0, 0, 0
      );
      int diasVencimento = (stringVencimento - rtc.now()).days();
      if (diasVencimento <= 10) {
        achou = true;
        avisoValidade++;
        msg += nomeProduto;
        if (diasVencimento == 0) {
          msg += " vence hoje!";
        }
        else if (diasVencimento < 0) {
          msg += " venceu!";
        }
        else {
          msg += " ";
          msg += diasVencimento;
          msg += " dias.";
        }
        lcd.setCursor(0, 3);
        lcd.print(msg);
      }
    } else {
      Serial.println("Erro: Formato de dados do produto inválido.");
    }
  }
  if(pos > qtde)
    pos = 0;
  achou = false;
  lcd.setCursor(0, 0);
  lcd.print("Temperatura: " + String(temp()) + "C");
  lcd.setCursor(0, 1);
  lcd.print("Umidade: " + String(umid())) + "%";
  lcd.setCursor(0, 2);
  lcd.print(luz());
}

void setup() {
  Serial.begin(9600);
  dht.begin();
  mySerial.begin(9600);
  rtc.begin();
  // rtc.adjust(DateTime(2023, 11, 21, 15, 38, 00));
  lcd.begin(20, 4);

  String stringArmazenada = leStringEEPROM(1);
  Serial.println("Dados Armazenados: \n" + stringArmazenada);
  horaDesligou = stringArmazenada;
  
  int posData = stringArmazenada.indexOf("Data:");
  int posHora = stringArmazenada.indexOf("Hora:");
  String dataString = stringArmazenada.substring(posData + 6, posHora - 1);
  String horaString = stringArmazenada.substring(posHora + 6);

  dataString.trim();
  horaString.trim();

  DateTime dataArmazenada = DateTime(
    dataString.substring(6, 10).toInt(), // Ano
    dataString.substring(3, 5).toInt(),  // Mês
    dataString.substring(0, 2).toInt(),  // Dia
    horaString.substring(0, 2).toInt(),  // Hora
    horaString.substring(3, 5).toInt(),  // Minuto
    horaString.substring(6, 8).toInt()   // Segundo
  );

  DateTime dataAtual = rtc.now();

  int diferencaEmMinutos = (dataAtual - dataArmazenada).totalseconds();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tempo desligado");
  lcd.setCursor(0, 1);
  lcd.print(String(diferencaEmMinutos) + " segundos");
  delay(3000);

  Serial.println("Diferença em segundos: " + String(diferencaEmMinutos));
}

String diasDaSemana(int dia) {
  switch (dia) {
    case 1:
      return "Domingo";
    case 2:
      return "Segunda-feira";
    case 3:
      return "Terça-feira";
    case 4:
      return "Quarta-feira";
    case 5:
      return "Quinta-feira";
    case 6:
      return "Sexta-feira";
    case 7:
      return "Sábado";
    default:
      return "Desconhecido";
  }
}

void limparEEPROM() {
  Serial.println("limpando...");
  int tamanhoEEPROM = EEPROM.length();

  for (int i = 0; i < tamanhoEEPROM; ++i) {
    EEPROM.write(i, 0xFF);
  }
  Serial.println("limpo!");
}

void loop() {
  static unsigned long ultimaAtualizacao = 0;
  unsigned long tempoLigado = millis();
  
  if (tempoLigado - ultimaAtualizacao >= 5000) {
    atualizarBlocoRelatorio();
    ultimaAtualizacao = tempoLigado;
    mostrarlcd();
    if(avisoValidade == 5) {
      mySerial.println("");
      avisoValidade = 0;
    }
  }
  
  if (mySerial.available()) {
    char c = mySerial.read();
    stringRecebida += c; // Adicionar o caractere à string recebida
    if (stringRecebida.indexOf("X0") != -1) {
      Serial.print("String recebida: ");
      Serial.println(stringRecebida);
      stringRecebida.replace("X0", "");

      if(stringRecebida.length() > 4)
        processarComando(stringRecebida);

      stringRecebida = "";
    }
  }
  
  if (Serial.available()) {
    char c = Serial.read();
    mySerial.write(c);
  }
}
