#define BLYNK_PRINT Serial // กำหนดให้แสดงข้อความจาก Blynk ไปยังพอร์ตเชื่อมต่ออนุกรม (Serial)
#define BLYNK_TEMPLATE_ID "TMPL6yOrdALQ6" // กำหนดรหัสแม่แบบของโปรเจ็คใน Blynk
#define BLYNK_TEMPLATE_NAME "Mobile" // กำหนดชื่อแม่แบบ
#define BLYNK_AUTH_TOKEN "0AvpMzNSmsGQw-zMOWmqO7Uq3erjALMG" // โทเค็นสำหรับการตรวจสอบสิทธิ์กับ Blynk

#include <ESP8266WiFi.h> // รวมไลบรารีสำหรับการเชื่อมต่อ WiFi ของ ESP8266
#include <BlynkSimpleEsp8266.h> // รวมไลบรารี Blynk สำหรับ ESP8266

char auth[] = BLYNK_AUTH_TOKEN; // กำหนดตัวแปรสำหรับเก็บโทเค็นการตรวจสอบสิทธิ์
char ssid[] = "MT-WF";  // ชื่อ WiFi 
char pass[] = "cintapadamu";  // รหัสผ่าน WiFi 

void setup() {
  Serial.begin(9600); // เริ่มการสื่อสารที่ baud rate 9600
  Blynk.begin(auth, ssid, pass); // เริ่มการเชื่อมต่อ Blynk ด้วยโทเค็น, SSID และรหัสผ่าน WiFi
}

// ฟังก์ชันที่จะถูกเรียกเมื่อมีการเปลี่ยนแปลงข้อมูลที่ Virtual Pin
BLYNK_WRITE(V0) {  
   Serial.println(1);  
}

BLYNK_WRITE(V1) {
  Serial.println(2);
}

BLYNK_WRITE(V2) {
  Serial.println(3);
}

BLYNK_WRITE(V3) {
  Serial.println(4);
}

BLYNK_WRITE(V4) {
  Serial.println(5);
}

BLYNK_WRITE(V5) {
  Serial.println(6);
}

BLYNK_WRITE(V6) {
  Serial.println(7);
}

BLYNK_WRITE(V7) {
  Serial.println(8);
}

void loop() {
  Blynk.run(); // ฟังก์ชันนี้เรียกให้ Blynk สามารถดำเนินการต่างๆ
  delay(200); 
}
