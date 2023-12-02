# IMP Projekt

S - ARM@FITkit3, ESP32 či jiný HW: Demo aplikace strojového učení (pomocí ArmNN, TinyML, TensorFlow Lite apod.)

Autor: Josef Kuchař (xkucha28)

## Úvod do problému

Cílem toho projektu je automatický odečet hodnot z plynoměru. Kontrolní odečty se hodí při změně smlouvy, zafixování cen a podobně. Pro uživatele je jednodušší, když je tato činnost automatizovaná a nemusí tak na ní myslet.

Automatický odečet lze u plynoměru dělat dvěma způsoby:

1. Opticky, například kamerou
2. Snímáním magnetických impulzů (1 puls = 0,01 m³) například pomocí hallové sondy

## Popis řešení

## Struktura projektu

Popis zajímavých částí struktury projektu

- `src/` Zdrojové kódy části běžící na ESP32
  - `camera_config.h` Nastavení kamery
  - `image_manipulation.{h|cpp}` Implementace bilinární interpolace
  - `model_data.{h|cpp}` Převedený uint8 model
  - `main.cpp` Hlavní kód aplikace
- `train/` Python notebook s kódem pro natrénování modelu
- `web/` Zdrojové kódy webového rozhraní
  - `src/App.svelte` Hlavní kód webového rozhraní

## Zhodnocení

### Místo pro zlepšení

Hlavním místem pro zlepšení by byla augmentace datové sady například rotací číslic, přidáním umělých nečistot a podobně. Tím by se nejspíše zlepšila přesnost modelu v reálných podmínkách. Toto zlepšení nebylo implementováno.

## Využité knihovny

### Embedded část

#### TensorFlow Lite for Microcontrollers

[Dokumentace](https://www.tensorflow.org/lite/microcontrollers)
[Platformio knihovna](<https://registry.platformio.org/libraries/trylaarsdam/Tensorflow%20Lite%20for%20Microcontrollers%20(WCL)>)

Implementace knihovny Tensorflow Lite for Microcontrollers pro PlatformIO. Slouží ke spouštění natrénovaného modelu.

#### ESP32 Camera Driver

[Platformio knihovna](https://registry.platformio.org/libraries/espressif/esp32-camera)

Driver kamery připojené k ESP32. Umožňuje z kamery získat obrázky v různých rozlišeních a formátech.

#### ESPAsyncWebServer

[Platformio knihovna](https://registry.platformio.org/libraries/ottowinter/ESPAsyncWebServer-esphome)

Asynchronní implementace webového serveru. Webový server zpřístupňuje jednoduché webové stránky, které slouží pro konfiguraci chodu celé aplikace.

#### ArduinoJson

[Platformio knihovna](https://registry.platformio.org/libraries/bblanchon/ArduinoJson)

Knihovna pro serializaci a deserializaci JSON formátu. Využitá pro parsování konfigurace poslané z webového rozhraní.

#### NtpClient

[Platformio knihovna](https://github.com/taranais/NTPClient)

Slouží k získání reálného času z internetu pomocí NTC protokolu. Získaný datum a čas se zapíše do logů společně s načerpanými daty.

### Webová část

#### Svelte

[Webové stránky](https://svelte.dev/)

Jednoduchý framework pro vytváření interaktivních webových aplikací.

#### TailwindCSS

[Webové stránky](https://tailwindcss.com/)

CSS framework sloužící ke snadnému stylování pomocí předpřipravených CSS tříd.

#### DaisyUI

[Webové stránky](https://daisyui.com/)

Soubor hotových komponent (tlačítek a podobně) postavený na TailwindCSS frameworkem.

#### Vite

[Webové stránky](https://vitejs.dev/)

Rychlý místní vývojový server, bundler.

#### Vite plugin compression 2

[Github repozitář](https://github.com/nonzzz/vite-plugin-compression)

Plugin do Vite, který komprimuje výsledné soubory do formátu gzip pro úsporu místa na mikrokontroleru.

## Další použité zdroje

### Kód bilineární interpolace

https://rosettacode.org/wiki/Bilinear_interpolation#C

Původní kód pro bilinerání interpolaci sloužící k zvětšování a zmenšování obrázků do požadovaného rozlišení 28x28 byl převzat ze stránky rossetacode.org.

Byly provedeny úpravy - práce pouze s černobílým 8bit obrázkem, oprava chyby, získávání pouze části původního obrázku definované souřadnicemi.

### Datová sada TMIST

https://www.kaggle.com/datasets/nimishmagre/tmnist-typeface-mnist

![TMNIST](https://storage.googleapis.com/kaggle-datasets-images/1552696/2558856/e9c50da141c1525b4ff74a03883ed1d7/dataset-cover.jpg?t=2021-08-26-01-32-54)
Ukázkový obrázek TMNIST datasetu, získaný z odkazu výše.

Tato datová sada byla použita k natrénovaní konvoluční neuronové sítě použité pro rozpoznávání čísel.

### Python notebook pro natrénování modelu

https://www.kaggle.com/code/stpeteishii/tmnist-conv2d/notebook

Tento Python notebook je použit k natrénování použité konvoluční neuronové sítě.

Je přidána konverze do formátu TFLite, následná quantizace do 8bit pro rychlejší provádění modelu a úsporu místa.

## Autoevaluace

14; Mám pocit, že jsem k projektu přistupoval zodpovědně, jedná se o poměrně rozsáhlé kvalitní funkční řešení.
