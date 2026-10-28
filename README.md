# Smart Home STM32

Maison connectée miniature pilotée par une carte **NUCLEO-F411RE** (STM32F411RE, Cortex-M4 à 100 MHz), programmée en C avec la HAL sous STM32CubeIDE.

Mini-projet de l'unité *Architectures Numériques Embarquées*, 4e année (E4SE), ESIEE Paris, 2026.
Réalisé par **David Ihou Wateba** et **Antoine Perfettini**.

<!-- Photo de la maquette ici : ![Maquette](docs/maquette.jpg) -->

## Fonctionnalités

- **Modes HOME / AWAY** avec le bouton utilisateur de la carte
- **Alarme intrusion** : en AWAY, le capteur PIR déclenche le buzzer et la LED d'alarme
- **Alarme gaz** : surveillée en permanence, seuils avec hystérésis
- **Fenêtre motorisée** : servo commandé par un bouton, fermeture automatique à la pluie, bloquée en AWAY
- **Température** : capteur TMP102 en I²C, driver écrit à partir de la datasheet
- **Supervision** : tous les événements et toutes les mesures envoyés au PC par liaison série

## Matériel

- Carte NUCLEO-F411RE
- Modules du kit Keyestudio KS5009 : PIR, buzzer passif, LED, capteur de gaz MQ, capteur de pluie, bouton, servo
- Capteur de température TMP102

## Brochage

| Fonction | Broche | Périphérique STM32 |
|---|---|---|
| Bouton HOME / AWAY (B1) | PC13 | EXTI13, front descendant |
| Capteur PIR | PB5 (D4) | EXTI5, front montant |
| Bouton fenêtre | PA10 (D2) | EXTI10, front montant |
| LED d'alarme | PA8 (D7) | Sortie GPIO |
| Buzzer passif | PB4 (D5) | TIM3_CH1, PWM 2 kHz |
| Servo fenêtre | PB6 (D10) | TIM4_CH1, PWM 50 Hz |
| Capteur de gaz | PA0 (A0) | ADC1_IN0, via pont diviseur 22 kΩ / 10 kΩ |
| Capteur de pluie | PA1 (A1) | ADC1_IN1 |
| TMP102 SDA / SCL | PB9 (D14) / PB8 (D15) | I2C1, 100 kHz |
| Liaison PC | PA2 / PA3 | USART2 (port COM virtuel du ST-LINK), 115 200 bauds 8N1 |

Bases de temps : **TIM11** toutes les 3 s (mesures gaz, pluie, température) et **TIM10** toutes les 250 ms (clignotement de la LED).

## Architecture du code

Tout le code utilisateur est dans [`Core/Src/main.c`](Core/Src/main.c), entre les balises `USER CODE` générées par STM32CubeMX.

- Les **interruptions** (timers, boutons, PIR) ne font que lever un drapeau `volatile`.
- La **boucle principale** teste les drapeaux et fait le vrai travail : lecture ADC, échanges I²C, messages UART, décisions.
- Résultat : aucune interruption ne bloque les autres, et pas de `HAL_Delay` dans une interruption.

## Compiler le projet

1. Cloner le dépôt.
2. Dans STM32CubeIDE : *File → Import → General → Existing Projects into Workspace*, puis sélectionner le dossier cloné.
3. *Project → Build Project* (Ctrl+B).
4. Flasher la carte et ouvrir un terminal série à 115 200 bauds sur le port COM du ST-LINK.

Au démarrage, le terminal affiche :

```
TMP102 OK

=== Smart Home STM32 - Bienvenue ===
Mode HOME
```

## Documentation

- [Rapport du projet (PDF)](docs/Rapport_SmartHome_STM32_WATEBA_PERFETTINI.pdf)
- RM0383 — Reference Manual STM32F411
- UM1724 — Manuel des cartes STM32 Nucleo-64
- Datasheet TMP102 (Texas Instruments, SBOS397)

## Licence

Le code dans `Core/Src/main.c` est celui des auteurs. Les dossiers `Drivers/` (HAL et CMSIS) et les fichiers générés par STM32CubeMX restent sous la licence de STMicroelectronics et d'ARM indiquée dans chacun d'eux.
