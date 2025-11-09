#include <TJpg_Decoder.h>
#include <TFT_eSPI.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <SpotifyArduino.h>
#include <SpotifyArduinoCert.h>

#include <ArduinoJson.h>

#include "List_SPIFFS.h"
#include "Web_Fetch.h"

// Spiffs Setup
#define FS_NO_GLOBALS
#include <FS.h>

#include <SPIFFS.h>
char ssid[] = "YOURSSID";                 // your network SSID (name)
char password[] = "YOURPASSWORD";         // your network password
char clientId[] = "YOURCLIENTID";         // Your client ID of your spotify api app
char clientSecret[] = "YOURCLIENTSECRET"; // Your client Secret of your spotify api app (Do Not share this!)

#define SPOTIFY_MARKET "US"
#define SPOTIFY_REFRESH_TOKEN "YOURREFRESHTOKEN" // Your Spotify Refresh Token

WiFiClientSecure client;
SpotifyArduino spotify(client, clientId, clientSecret, SPOTIFY_REFRESH_TOKEN);

unsigned long delayBetweenRequests = 5000; // Time between requests (1 minute)
unsigned long requestDueTime;              // time when request due

TFT_eSPI tft = TFT_eSPI(); // Create an instance of the TFT_eSPI class
TFT_eSprite img = TFT_eSprite(&tft);

String lastTrackName = "";

// ---- Function to draw decoded JPEG blocks into sprite ----
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{

    if (y >= img.height())
        return 0;
    img.pushImage(x, y, w, h, bitmap);
    return 1; // Continue decoding
}

void setup()
{

    Serial.begin(115200);

    // Initialize SPIFFS
    if (!SPIFFS.begin())
    {
        Serial.println("SPIFFS initialization failed!");
        while (1)
            yield();
    }
    Serial.println("SPIFFS ready.");

    tft.init();                // Initialize the TFT display
    tft.setRotation(1);        // Set the display rotation (0-3)
    tft.fillScreen(TFT_BLACK); // Fill Screen with background color
    tft.setTextSize(2);

    // Setup TJpg decoder
    TJpgDec.setJpgScale(2);     // No scaling (decode full resolution)
    TJpgDec.setSwapBytes(true); // Required for TFT_eSPI
    TJpgDec.setCallback(tft_output);

    WiFi.mode(WIFI_STA); // Connect To Wifi
    WiFi.begin(ssid, password);
    Serial.println("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");

    // Handle HTTPS Verification
    client.setCACert(spotify_server_cert); // Sometimes this doesn't work, if it doesn't try uncommenting the line below.
    // client.setInsecure(); // Accept any certificate (simplest, not secure)
    client.setTimeout(10000); // Allow up to 10s for full JSON response, This is not strictly neccesssary but fixes "Error -1", I found that this error is fairly common, so enabled by default

    // Center pivot point for later rotations (optional)
    tft.setPivot(tft.width() / 2, tft.height() / 2);

    // Create a 200x200 sprite
    if (img.createSprite(150, 150))
    {
        img.fillSprite(TFT_BLACK);
    }
    else
    {
        tft.fillScreen(TFT_RED);
        tft.drawString("Failed to create sprite!", 10, 10);
    }

    // For testing: remove old file
    if (SPIFFS.exists("/album.jpg"))
    {
        Serial.println("Removing old /M81.jpg file");
        SPIFFS.remove("/album.jpg");
    }

    Serial.println("Refreshing Access Tokens");
    if (!spotify.refreshAccessToken())
    {
        Serial.println("Failed to get access tokens");
    }
}

void printCurrentlyPlayingToSerial(CurrentlyPlaying currentlyPlaying)
{
    // Check if it's a new track
    if (String(currentlyPlaying.trackName) == lastTrackName)
    {
        Serial.println("Same track as before — skipping update.");
        return; // Exit early, no need to redraw or reprint
    }

    if (SPIFFS.exists("/album.jpg"))
    {
        Serial.println("Removing old album art");
        SPIFFS.remove("/album.jpg");
    }

    // Update stored track name
    lastTrackName = String(currentlyPlaying.trackName);

    Serial.println("--------- Currently Playing ---------");

    Serial.print("Is Playing: ");
    if (currentlyPlaying.isPlaying)
    {
        Serial.println("Yes");
    }
    else
    {
        Serial.println("No");
    }

    Serial.print("Track: ");
    Serial.println(currentlyPlaying.trackName);

    tft.fillScreen(TFT_BLACK); // Clear screen for new drawing

    tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set the text color to white with black background

    // Draw a string at position (100, 100) on the screen
    tft.drawString(currentlyPlaying.trackName, 20, 180);

    String combinedArtists = "";

    Serial.println("Artists: ");
    for (int i = 0; i < currentlyPlaying.numArtists; i++)
    {
        Serial.print("Name: ");
        Serial.println(currentlyPlaying.artists[i].artistName);
        Serial.println();
        combinedArtists += currentlyPlaying.artists[i].artistName; // Add artist name
        if (i < currentlyPlaying.numArtists - 1)
        {
            combinedArtists += ", "; // Add comma except after last artist
        }
    }

    Serial.println(combinedArtists);
    tft.drawString(combinedArtists, 20, 210);
    Serial.print("Album: ");
    Serial.println(currentlyPlaying.albumName);
    Serial.println();

    if (currentlyPlaying.contextUri != NULL)
    {
        Serial.print("Context URI: ");
        Serial.println(currentlyPlaying.contextUri);
        Serial.println();
    }

    // will be in order of widest to narrowest
    // currentlyPlaying.numImages is the number of images that
    // are stored

    String albumUrl = currentlyPlaying.albumImages[1].url;

    Serial.print("Album Image: ");
    Serial.print(albumUrl);
    Serial.println();
    Serial.println("------------------------");

    tft.fillRect(300, 0, 20, 240, TFT_BLACK);

    uint32_t t = millis();

    // Download file to SPIFFS
    bool loaded_ok = getFile(albumUrl, "/album.jpg");

    listSPIFFS();

    t = millis() - t;
    if (loaded_ok)
    {
        Serial.printf("Download took %u ms\n", t);
    }
    else
    {
        Serial.println("Download failed!");
        delay(5000);
        return;
    }

    // Decode JPEG directly into sprite
    img.fillSprite(TFT_BLACK);
    TJpgDec.drawFsJpg(0, 0, "/album.jpg");

    img.pushSprite(20, 20);

    Serial.println("Image rendered at 200x200");

    listSPIFFS();
}

void loop()
{
    if (millis() > requestDueTime)
    {
        Serial.println("getting currently playing song:");
        // Market can be excluded if you want e.g. spotify.getCurrentlyPlaying()
        int status = spotify.getCurrentlyPlaying(printCurrentlyPlayingToSerial, SPOTIFY_MARKET);
        if (status == 200)
        {
            Serial.println("Successfully got currently playing");
        }
        else if (status == 204)
        {
            Serial.println("Doesn't seem to be anything playing");
            tft.fillScreen(TFT_BLACK);
            tft.drawString("No Track Playing", 20, 115);
        }
        else
        {
            Serial.print("Error: ");
            Serial.println(status);
        }
        requestDueTime = millis() + delayBetweenRequests;
    }
}