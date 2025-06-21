# Aqualung / Shearwater transmitter decoder with ESP32-S3
This project is inspired by the work done during several months by many passionated people - see : [Scubaboard forum](https://scubaboard.com/community/threads/reading-wireless-air-transmitter-using-arduino.601083/)

The synthesis of the protocol is located in this github : [Github - rg422](https://github.com/rg422/PPS-MH8A-Transmitter)

## What do you need
You need :
- an ESP32-S3 devkit board (esp32-s3-devkitc-1) : [Amazon - no affiliation](https://www.amazon.fr/sspa/click?ie=UTF8&spc=MTo0NjY3MzU2MjYyNDY4NzU3OjE3NDg4MDA0NzU6c3BfYXRmOjMwMDEyMzA1MTM4NzIzMjo6MDo6&url=%2FESP32-S3-DevKitC-1-d%25C3%25A9veloppement-RUIZHI-ESP32-S3-WROOM-1-N16R8-Compatible%2Fdp%2FB0CPY56822%2Fref%3Dsr_1_1_sspa%3F__mk_fr_FR%3D%25C3%2585M%25C3%2585%25C5%25BD%25C3%2595%25C3%2591%26crid%3D2RLZNUFKS4N21%26dib%3DeyJ2IjoiMSJ9.YbOPs5gGHpnKMpsODreQ9JX5HxYccIQ8MpKxOvNwiV88Q9XB7p4uV82D3jJ8gEuhMm8nBnOhAmM_MM0B8MZneQepHkhB0fMz_srhLt7Ld7M5L-DpW6DEaPSTnS1C54wOEC9actJpDo4N_sKzRVzlKNieIkAGpKtShFJTFXYxEJrAoB0dZqkXWUSw60J0od2ARF_0LZCHDY_blImdHTzNP9FiL-Bh3iIDUBB3dO89iNIlXK3K8Jri54VTHarlghNNqPZYbHL5oNtUGYSDwZNd_Ax8pywbY6qjkU6Bc1EhFr4.HeSgEcWuVNLpo9pQNJ1JdSDwmvVu-mvt7z6sSouZVMQ%26dib_tag%3Dse%26keywords%3Desp32-s3-devkitc-1%26qid%3D1748800475%26s%3Delectronics%26sprefix%3Desp32-s3-devkitc-1%252Celectronics%252C69%26sr%3D1-1-spons%26sp_csd%3Dd2lkZ2V0TmFtZT1zcF9hdGY%26psc%3D1)
- a 8ohm / 2W cheap speaker : [Amazon - no affiliation](https://www.amazon.fr/dp/B00O9YGQ42?ref=ppx_yo2ov_dt_b_fed_asin_title)
- few wires and a soldering iron

## Important :
If you are just using an ESP32-S3 without operational amplifier to amplify the speaker signal -> you need to use the v2.0 release.
From v3.0 release, the SW is using interrupt reading for more acccuracy, and it needs to get an amplified signal from the speaker with an AOP (for instance TLV2371DBV)

## Wirings
![image](https://github.com/user-attachments/assets/dbb9efec-de91-46df-9ee4-e331d59c5bc5)

Connect the + of the speaker on the GPIO4 of the ESP32-S3

Connect the - of the speaker on one of the GND pins of the ESP32-S3

Connect your computer to the ESP32-S3 on the UART USB socket

## Software
You'll need Visual Studio Code + Platformio

Then just download my code and upload it to your ESP32-S3

## Physical setup

![image](https://github.com/user-attachments/assets/66615d05-e8f4-48cf-8a8e-36d89d0bf244)

Put the speaker on the aqualung / shearwater transmitter

Read the values on the console

## Result

![image](https://github.com/user-attachments/assets/74292ec7-edca-479f-89b8-f87916f0eca0)

In this example, I closed the tank valve and I released a little bit of pressure several times to see the impact on the values
