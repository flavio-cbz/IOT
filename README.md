<div align="center">

# 🌿 Station IOT — La station connectée qui veille sur vos plantes

**Parce qu'une plante ne peut pas vous dire qu'elle souffre.**

> 🎓 Projet étudiant · ADI 2 · Groupe 4 · 2026

[![Docker](https://img.shields.io/badge/Docker-Ready-blue?logo=docker)](https://www.docker.com/)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32--S3-orange?logo=espressif)](https://www.espressif.com/)
[![MQTT: Secured](https://img.shields.io/badge/MQTT-WSS%20443-brightgreen)](https://mosquitto.org/)

</div>

---

## 😔 Le constat

**En France, plus de 60% des plantes d'intérieur meurent faute d'entretien inadapté.**

Trop d'eau. Pas assez de lumière. Une chaleur excessive. Des erreurs souvent invisibles, et toujours découvertes trop tard.

Pourtant, nous aimons nos plantes. Nous les achetons, nous les décorons, nous les offrons. Mais sans les bons outils, nous sommes aveugles face à leurs besoins.

---

## ❓ La problématique

> *Comment savoir, en temps réel et sans être botaniste, si ma plante va bien ?*

Les solutions existantes sur le marché sont soit **trop chères** pour un usage domestique courant, soit **fermées** dans des écosystèmes propriétaires qui vous enfoncent dans des abonnements et des applications qui disparaissent du jour au lendemain.

Il manquait une solution **libre, transparente, auto-hébergée** — et surtout, **vraiment utile**.

---

## 💡 Notre solution : Station IOT

Notre **station de monitoring open-source** est conçue pour vous donner une vision claire et instantanée de l'état de vos plantes — construite de A à Z dans le cadre de notre projet étudiant ADI 2.

> 🌱 *Elle observe. Elle analyse. Elle vous alerte. Et elle le fait avec une interface qui donne le sourire.*

### Ce que la station surveille pour vous

| Mesure | Capteur | Utilité concrète |
|---|---|---|
| 💧 Humidité du sol | SEN0193 | Savoir quand arroser |
| 🌡️ Température ambiante | BMP280 | Détecter les coups de chaud ou de froid |
| 📊 Pression atmosphérique | BMP280 | Contexte climatique local |
| ☀️ Luminosité | BH1750 | Optimiser le placement de la plante |
| 🏔️ Altitude | BMP280 | Calibrage automatique |

### Ce que vous voyez

- **Un écran OLED embarqué** avec des animations : votre plante sourit quand elle est heureuse, elle vous supplie quand elle a soif.
- **Un dashboard web moderne**, accessible depuis n'importe quel appareil (mobile, tablette, PC), avec mode sombre et historique des données.
- **Des alertes en temps réel** — sans rafraîchissement manuel.

---

## ✨ Pourquoi PlantWatch plutôt qu'une autre solution ?

| | Solutions concurrentes | **PlantWatch** |
|---|---|---|
| 💰 Prix | 30 €–150 € + abonnement | Open-source & auto-hébergé |
| 🔒 Vie privée | Données envoyées dans le cloud | 100% chez vous |
| 🔧 Personnalisable | Non | Totalement |
| 📡 Connexion sécurisée | Variable | HTTPS + WSS (port 443) |
| 📱 Dashboard | App propriétaire | Interface web universelle |
| ♻️ Pérennité | Dépend de l'éditeur | Vous en êtes propriétaire |

---

## 🚀 Comment ça fonctionne ?

```
  [Plante] 🌿
      │
  [Station ESP32-S3]  ← Mesure en continu l'environnement
      │
      │ (Connexion chiffrée — WSS port 443)
      ▼
  [Nginx Reverse Proxy]
      │
      ├──▶  Dashboard Web  (HTTPS)
      └──▶  Broker MQTT    (WSS → /mqtt)
                │
                ▼
         [Backend Django + MongoDB]
              Stockage, analyse & diffusion temps réel
```

Tout tourne en **Docker** sur votre propre serveur ou NAS. Une seule commande suffit pour tout démarrer.

---

## ⚡ Mise en route en 3 étapes

### 1. Déployez le serveur

```bash
git clone https://github.com/votre-repo/station-iot.git
cd station-iot
docker compose up -d --build
```

> Le dashboard est immédiatement disponible sur `http://localhost:8095/`

### 2. Configurez votre accès sécurisé (optionnel mais recommandé)

Via **Nginx Proxy Manager**, créez un Proxy Host vers votre instance :

| Paramètre | Valeur |
|---|---|
| Domaine | `iot.votredomaine.com` |
| Destination | `http://<IP_SERVEUR>:8095` |
| SSL | ✅ Let's Encrypt |
| Custom Location `/mqtt` | Port `9002` — **WebSocket activé** |

### 3. Flashez votre station ESP32

```bash
cd firmware
pio run -t upload     # Compile et installe le firmware
pio device monitor    # Visualise les logs en direct
```

Au premier démarrage, la station crée un point d'accès Wi-Fi **"Station-Plante"**. Connectez-vous y depuis votre téléphone pour configurer votre réseau en 30 secondes.

---

## 🛠️ Stack technique

| Composant | Technologie |
|---|---|
| Microcontrôleur | ESP32-S3-OTG (Arduino / PlatformIO) |
| Affichage embarqué | OLED SSD1306 128×64 (I2C) |
| Communication | MQTT over WebSocket Secure (WSS) |
| Backend | Django + Django Channels (ASGI) |
| Base de données | MongoDB |
| Déploiement | Docker Compose |
| Proxy | Nginx Proxy Manager |

---

## 🩺 Diagnostic rapide

| Symptôme | Cause probable | Solution |
|---|---|---|
| ⚠️ `MQTT Disconnected` | WebSocket non activé dans Nginx | Activer "Websockets Support" pour `/mqtt` dans NPM |
| 🖤 Écran OLED éteint | Mauvais câblage I2C | Vérifier SDA=GPIO9, SCL=GPIO8 |
| 📉 Valeurs sol incohérentes | Calibration à ajuster | Modifier `SOIL_DRY` et `SOIL_WET` dans `main.cpp` |

---

## 📬 Contribuer

Ce projet est **ouvert à la communauté**. Qu'il s'agisse de corriger un bug, d'ajouter un nouveau capteur ou de proposer une amélioration d'interface — toutes les contributions sont les bienvenues.

> Ensemble, rendons les plantes immortelles. 🌍

---

<div align="center">

*Projet réalisé dans le cadre de l'ADI 2 · Groupe 4 · 2026*
*Construit avec ❤️, du café ☕ et beaucoup trop d'humidité de sol.*

</div>
