#ifndef IMAGES_H
#define IMAGES_H

#include <Arduino.h>

// Icone Goutte (Humidité) - 8x8
const unsigned char PROGMEM icon_droplet[] = {0x00, 0x18, 0x3C, 0x7E,
                                              0x7E, 0x7E, 0x3C, 0x00};

// Icone Thermometre (Température) - 8x8
const unsigned char PROGMEM icon_thermometer[] = {0x0C, 0x0C, 0x0C, 0x1E,
                                                  0x3E, 0x3E, 0x1E, 0x00};

// Icone Soleil (Lumière) - 8x8
const unsigned char PROGMEM icon_sun[] = {0x00, 0x24, 0x18, 0x7E,
                                          0x7E, 0x18, 0x24, 0x00};

// Icone WiFi - 8x8
const unsigned char PROGMEM icon_wifi[] = {0x00, 0x3C, 0x42, 0x18,
                                           0x24, 0x00, 0x18, 0x18};

// Icone Horloge (Uptime) - 8x8
const unsigned char PROGMEM icon_clock[] = {0x18, 0x24, 0x42, 0x42,
                                            0x42, 0x24, 0x18, 0x00};

#endif
