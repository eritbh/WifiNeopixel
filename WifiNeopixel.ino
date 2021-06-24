#include <Adafruit_NeoPixel.h>
#include <ESP8266WebServerSecure.h>
#include <ESP8266WiFi.h>
#include "CredentialManager.h"

// SSL certs and WiFi info and stuff
#include "Secrets.h"

// LED strip configuration
#define LED_DATA_PIN 0
#define NUMPIXELS 180

ESP8266WebServerSecure server(443);

// Set the credential filename and realm name, as well as default credentials
CredentialManager cm("/credentials.txt", "global", DEFAULT_LOGIN, DEFAULT_PASSWORD);

// Configure our LED strip
Adafruit_NeoPixel pixels(NUMPIXELS, LED_DATA_PIN, NEO_GRB + NEO_KHZ800);

// The current color of the strip
uint32_t currentColor = pixels.Color(0, 0, 0);

void setup()
{
  Serial.begin(115200);

  // Set up LED strip
  pixels.begin();

  //Initialize wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("WiFi Connect Failed! Rebooting...");
    delay(1000);
    ESP.restart();
  }

  // Set up the web server

  // Add HTTPS cert
  server.getServer().setRSACert(new BearSSL::X509List(serverCert), new BearSSL::PrivateKey(serverKey));

  // Register route handlers
  server.on("/", HTTP_GET, showHomepage);
  server.on("/changecreds", HTTP_GET, showcredentialpage);      //for this simple example, just show a simple page for changing credentials at the root
  server.on("/changecreds", HTTP_POST, handlecredentialchange); //handles submission of credentials from the client
  server.on("/color", HTTP_GET, GET_color);
  server.on("/color", HTTP_POST, POST_color);
  // server.on("/nuke", HTTP_GET, nuke);
  server.onNotFound(redirect);

  // Start listening
  server.begin();

  Serial.print("Open https://");
  Serial.print(WiFi.localIP());
  Serial.println("/ in your browser to see it working");
}

unsigned long lastRun = millis();
int currentLed = 0;
#define PERIOD 500;
void loop()
{
  server.handleClient();
  yield();
  for (int i = 0; i < NUMPIXELS; i += 1)
  {
    pixels.setPixelColor(i, currentColor);
  }
  pixels.show();
}

void nuke()
{
  cm.purge();
  server.send(200, "text/plain", "yeet");
  ESP.restart();
}

void showHomepage()
{
  server.send(200, "text/html", R"HTML(
    <html>
      <a href="/changecreds">Change credentials</a>
    </html>
  )HTML");
}

//This function redirects home
void redirect()
{
  String url = "https://" + WiFi.localIP().toString();
  Serial.println("Redirect called. Redirecting to " + url);
  server.sendHeader("Location", url, true);
  server.send(302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
  server.client().stop();             // Stop is needed because we sent no content length
}

//This function checks whether the current session has been authenticated. If not, a request for credentials is sent.
bool session_authenticated()
{
  Serial.println("Checking authentication.");
  if (server.authenticateDigest(cm.login, cm.credentialHash))
  {
    Serial.println("Authentication confirmed.");
    return true;
  }
  else
  {
    Serial.println("Not authenticated. Requesting credentials.");
    server.requestAuthentication(DIGEST_AUTH, cm.salt.c_str(), "Authentication failed");
    redirect();
    return false;
  }
}

//This function sends a simple webpage for changing login credentials to the client
// Handler for /
void showcredentialpage()
{
  Serial.println("Show credential page called.");
  if (!session_authenticated())
  {
    return;
  }
  server.send(200, "text/html", R"HTML(
    <html>
      <h1>Login Credentials</h1>
      <form action="changecreds" method="post">
        <p>
          <label for="login">Login</label>
          <input type="text" name="login">
        </p>
        <p>
          <label for="password">New password</label>
          <input type="password" name="password">
        </p>
        <p>
          <label for="password_duplicate">New password (again):</label>
          <input type="password" name="password_duplicate">
        </p>
        <p>
          <button type="submit" name="newcredentials">Change</button>
        </p>
      </form>
    </html>
  )HTML");
}

//This function handles a credential change from a client.
// Handler for /changecreds
void handlecredentialchange()
{
  if (!session_authenticated())
  {
    return;
  }

  String login = server.arg("login");
  String pw1 = server.arg("password");
  String pw2 = server.arg("password_duplicate");

  if (login != "" && pw1 != "" && pw1 == pw2)
  {

    cm.edit(login, pw1);
    server.send(200, "text/plain", "Credentials updated");
    redirect();
  }
  else
  {
    server.send(200, "text/plain", "Malformed credentials");
    redirect();
  }
}

void GET_color()
{
  if (!session_authenticated())
  {
    return;
  }

  server.send(200, "text/html", R"HTML(
    <html>
      <h1>Change Color</h1>
      <form action="color" method="post">
        <p>
          <label for="r">R</label>
          <input type="range" name="r" min="0" max="255">
        </p>
        <p>
          <label for="g">G</label>
          <input type="range" name="g" min="0" max="255">
        </p>
        <p>
          <label for="b">B</label>
          <input type="range" name="b" min="0" max="255">
        </p>
        <p>
          <button type="submit">Change color</button>
        </p>
      </form>
    </html>
  )HTML");
}

void POST_color()
{
  if (!session_authenticated())
  {
    return;
  }

  int r = server.arg("r").toInt();
  int g = server.arg("g").toInt();
  int b = server.arg("b").toInt();
  currentColor = pixels.Color(r, g, b);
  server.send(204);
}
