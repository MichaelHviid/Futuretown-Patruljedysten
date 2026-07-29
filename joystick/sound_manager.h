#ifndef SOUND_MANAGER_H
#define SOUND_MANAGER_H

#include <Arduino.h>
#include <Audio.h>
#include "LittleFS.h"

extern Audio audio;
extern byte Volume;
extern bool HasAudio;

void initAudio(byte dout, byte bclk, byte lrc ) {
    if (!HasAudio) return;
    audio.setPinout(bclk, lrc, dout);
    audio.setVolume(Volume);
    audio.setAudioTaskCore(1);
    Serial.println("Audio Manager: Initialiseret med direkte LittleFS-afspilning");
}

// Den primære afspiller, der modtager et ID og spiller filen med det samme
void playSound(int soundId, byte volume) {
    if (!HasAudio) return;

    if (volume > 21) volume = 21;
    Volume = volume;
    audio.setVolume(Volume);

    // Vi leder efter ID.mp3 (f.eks. "/8.mp3") eller ID.wav
    String pathMp3 = "/" + String(soundId) + ".mp3";
    String pathWav = "/" + String(soundId) + ".wav";
    String activePath = "";

    if (LittleFS.exists(pathMp3)) activePath = pathMp3;
    else if (LittleFS.exists(pathWav)) activePath = pathWav;

    if (activePath != "") {
        Serial.printf("Audio: Spiller direkte fil -> %s\n", activePath.c_str());
        audio.connecttoFS(LittleFS, activePath.c_str());
    } else {
        Serial.printf("Audio Fejl: Kunne ikke finde fil for ID %d\n", soundId);
    }
}

// Bruges til webinterfacets preview via direkte filnavn
void playSoundDirect(String filNavn) {
    if (!HasAudio) return;

    audio.setVolume(Volume); 
    String activePath = filNavn.startsWith("/") ? filNavn : "/" + filNavn;

    if (LittleFS.exists(activePath)) {
        Serial.printf("Web Preview: Spiller %s\n", activePath.c_str());
        audio.connecttoFS(LittleFS, activePath.c_str());
    } else {
        Serial.printf("Web Preview Fejl: %s findes ikke i LittleFS\n", activePath.c_str());
    }
}

#endif
