#pragma once

#include <FS.h>
#include <LittleFS.h>
#include <ESP8266WebServer.h>

class CredentialManager
{
public:
  const char *filename;
  String salt;
  String login;
  String credentialHash;

  CredentialManager(const char *filename, String salt, String defaultLogin, String defaultPlainTextCredential)
  {
    //Initialize LittleFS to save credentials
    if (!LittleFS.begin())
    {
      Serial.println("LittleFS initialization error, programmer flash configured?");
      ESP.restart();
    }

    this->filename = filename;
    this->salt = salt;

    File f = LittleFS.open(filename, "r");
    if (f)
    {
      String contents = f.readString();
      int a = contents.indexOf('\n', 0);
      int b = contents.indexOf('\n', a + 1);
      login = contents.substring(0, a - 1);
      credentialHash = contents.substring(a + 1, b - 1);
      f.close();
    }
    else
    {
      Serial.println("No credential found, resetting to default credentials.");
      Serial.println("user:" + defaultLogin);
      Serial.println("password:" + defaultPlainTextCredential);
      edit(defaultLogin, defaultPlainTextCredential);
    }
  }

  String createCredentialHash(String login, String plainTextPassword)
  {
    return ESP8266WebServer::credentialHash(login, salt, plainTextPassword);
  }

  void edit(String newLogin, String newPassword)
  {
    //Set global variables to new values
    login = newLogin;
    credentialHash = createCredentialHash(newLogin, newPassword);

    //Save new values to LittleFS for loading on next reboot
    File f = LittleFS.open(filename, "w"); // "W" mode discards old contents
    if (f)
    {
      f.println(login);
      f.println(credentialHash);
      f.close();
    }
  }

  void purge()
  {
    LittleFS.remove(filename);
  }
};
