#include <avr/io.h>                   // รวมไฟล์ส่วนหัวเพื่อเข้าถึงเรจิสเตอร์ IO ของไมโครคอนโทรลเลอร์ AVR
#include <util/delay.h>               // รวมไฟล์ส่วนหัวสำหรับฟังก์ชันที่เกี่ยวข้องกับการหน่วงเวลา
#include <avr/interrupt.h>            // รวมไฟล์ส่วนหัวสำหรับฟังก์ชันที่เกี่ยวข้องกับการจัดการขัดจังหวะ
#include <compat/twi.h>               // รวมไฟล์ส่วนหัวสำหรับความเข้ากันได้กับ Two-Wire Interface (TWI/I2C)
#include <util/atomic.h>              // รวมไฟล์ส่วนหัวสำหรับการดำเนินการที่ต้องการการป้องกันขัดจังหวะ

#define F_CPU 16000000UL              // กำหนดความถี่ของไมโครคอนโทรลเลอร์เป็น 16 MHz
#define PCF8574_ADDR 0x20             // กำหนดที่อยู่ I2C ของอุปกรณ์ PCF8574
#define EEPROM_ADDRESS_OUTPUT_STATE 0x00  // กำหนดที่อยู่ของสถานะเอาต์พุตใน EEPROM

uint8_t output_state = 0b00000000;   // ตัวแปรสำหรับเก็บสถานะเอาต์พุตปัจจุบัน
int lastSensorRange = -1;            // ตัวแปรเพื่อเก็บช่วงการตรวจจับล่าสุดของเซนเซอร์

volatile unsigned long timer_millis = 0;  // ตัวแปรชนิด volatile สำหรับเก็บเวลาที่ผ่านไปในหน่วยมิลลิวินาที
unsigned long startmillis = 0;       // ตัวแปรเพื่อเก็บเวลาเริ่มต้นในการตรวจจับ

// ประกาศต้นแบบฟังก์ชัน
void uart_init(unsigned long baud);  // ฟังก์ชันเริ่มต้น UART
unsigned char uart_receive(void);    // ฟังก์ชันรับข้อมูล UART
void uart_transmit(unsigned char data);  // ฟังก์ชันส่งข้อมูล UART
void uart_print(char *str);          // ฟังก์ชันพิมพ์ข้อความผ่าน UART
void uart_print_int(int value);      // ฟังก์ชันพิมพ์ค่าจำนวนเต็มผ่าน UART
void i2c_init(void);                 // ฟังก์ชันเริ่มต้น I2C
void i2c_start(void);                // ฟังก์ชันส่งสัญญาณเริ่มต้น I2C
void i2c_stop(void);                 // ฟังก์ชันส่งสัญญาณหยุด I2C
void i2c_write(uint8_t data);        // ฟังก์ชันเขียนข้อมูลไปยัง I2C
void start_adc_conversion(void);     // ฟังก์ชันเริ่มการแปลงค่า ADC
int read_adc_value(void);            // ฟังก์ชันอ่านค่าจาก ADC
void EEPROM_write(uint8_t uiAddress, uint8_t ucData); // ฟังก์ชันเขียนข้อมูลลง EEPROM
uint8_t EEPROM_read(uint8_t uiAddress); // ฟังก์ชันอ่านข้อมูลจาก EEPROM
void save_output_state(void);        // ฟังก์ชันบันทึกสถานะเอาต์พุตลง EEPROM
void load_output_state(void);        // ฟังก์ชันโหลดสถานะเอาต์พุตจาก EEPROM
void init_millis(unsigned long f_cpu); // ฟังก์ชันเริ่มต้นตัวนับเวลา
unsigned long millis(void);          // ฟังก์ชันอ่านค่าเวลาที่ผ่านไปในหน่วยมิลลิวินาที

// ฟังก์ชัน EEPROM
void EEPROM_write_all() {
  for (uint16_t i = 0; i < 1024; i++) {
    EEPROM_write(i, 0xFF); // เขียนค่า 0xFF ลงทุกตำแหน่งใน EEPROM
  }
}

void EEPROM_read_all() {
  uint8_t data;
  for (uint16_t i = 0; i < 1024; i++) {
    data = EEPROM_read(i);
    uart_transmit(data);  
  }
}

int main(void) {
  uart_init(9600);                      // เริ่มต้นการใช้งาน UART ที่ baud rate 9600
  i2c_init();                           // เริ่มต้นการใช้งาน I2C
  init_millis(F_CPU);                   // เริ่มต้นตัวนับเวลาโดยใช้ความถี่ของไมโครคอนโทรลเลอร์
  sei();                                // อนุญาตให้มีการใช้งาน interrupt

  startmillis = millis();               // บันทึกเวลาเริ่มต้น

  load_output_state();                  // โหลดสถานะเอาต์พุตจาก EEPROM

  ADMUX = (1 << REFS0);                 // เลือกใช้ Vcc เป็นแรงดันอ้างอิงสำหรับ ADC
  ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);  // ใช้งาน ADC และตั้งค่า prescaler

  while (1) {
    int adcSensorRange = -1;            // ตัวแปรเพื่อเก็บช่วงการตรวจจับของ ADC
    int uartSensorRange = -1;           // ตัวแปรเพื่อเก็บช่วงการตรวจจับของ UART

    if (millis() - startmillis > 15000) { // หากผ่านไป 15 วินาที
      if (UCSR0A & (1 << RXC0)) {        // ตรวจสอบว่ามีข้อมูลใหม่เข้ามาใน UART หรือไม่
        unsigned char received_data = uart_receive();  // อ่านข้อมูลที่ได้รับ
        if (received_data >= '1' && received_data <= '8') {
          uartSensorRange = received_data - '1';  // แปลงข้อมูลที่ได้รับเป็นช่วงของเซนเซอร์
        }
        uart_transmit(received_data);  // ส่งข้อมูลที่ได้รับกลับไป
      }
    }

    if (!(ADCSRA & (1 << ADSC))) {     // ตรวจสอบว่า ADC ไม่อยู่ในระหว่างการแปลงข้อมูล
      start_adc_conversion();          // เริ่มการแปลงข้อมูลของ ADC
      int sensorValue = read_adc_value();  // อ่านค่าที่ได้จาก ADC

      // ตรวจสอบช่วงค่าของ sensorValue และตั้งค่า adcSensorRange ตามช่วงที่เหมาะสม
      if ((sensorValue > 750) && (sensorValue < 1023)) {
        adcSensorRange = 0;
      } else if ((sensorValue > 640) && (sensorValue < 750)) {
        adcSensorRange = 1;  
      } else if ((sensorValue > 550) && (sensorValue < 640)) {
        adcSensorRange = 2;  
      } else if ((sensorValue > 480) && (sensorValue < 550)) {
        adcSensorRange = 3;
      } else if ((sensorValue > 350) && (sensorValue < 480)) {
        adcSensorRange = 4;
      } else if ((sensorValue > 270) && (sensorValue < 350)) {
        adcSensorRange = 5;
      } else if ((sensorValue > 200) && (sensorValue < 270)) {
        adcSensorRange = 6;
      } else if ((sensorValue > 95) && (sensorValue < 200)) {
        adcSensorRange = 7; 
      }
    }

    // หากมีการตรวจจับผ่าน UART ให้ใช้ค่านั้น
    if (uartSensorRange != -1) {
      adcSensorRange = uartSensorRange;
      _delay_ms(200);
      uartSensorRange = -1;
    }

    // หากมีการเปลี่ยนแปลงช่วงการตรวจจับ ให้เปลี่ยนสถานะเอาต์พุตและบันทึกลง EEPROM
    if (adcSensorRange != -1 && adcSensorRange != lastSensorRange) {
      output_state ^= (1 << adcSensorRange);  
      lastSensorRange = adcSensorRange;
      _delay_ms(200);
      lastSensorRange = -1;
      save_output_state();
    }

    // ส่งสัญญาณ I2C เพื่อควบคุมอุปกรณ์ภายนอกด้วยสถานะเอาต์พุตปัจจุบัน
    i2c_start();
    i2c_write(PCF8574_ADDR << 1); 
    i2c_write(output_state);  
    i2c_stop();
  }
}

void uart_init(unsigned long baud) {
  unsigned int ubrr = F_CPU / 16 / baud - 1;  // คำนวณค่า UBRR สำหรับการตั้งค่าบอดเรต
  UBRR0H = (unsigned char)(ubrr >> 8);  // ตั้งค่า 8 บิตบนของ UBRR
  UBRR0L = (unsigned char)ubrr;         // ตั้งค่า 8 บิตล่างของ UBRR
  UCSR0B = (1 << RXEN0) | (1 << TXEN0); // เปิดใช้งานโมดูลรับและส่งของ UART
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // ตั้งค่าขนาดข้อมูล 8 บิต
}

unsigned char uart_receive(void) {
  while (!(UCSR0A & (1 << RXC0)));  // รอจนกว่าจะมีข้อมูลให้รับ
  return UDR0;  // อ่านข้อมูลที่ได้รับจาก UART
}

void uart_transmit(unsigned char data) {
  while (!(UCSR0A & (1 << UDRE0))); // รอจนกว่า UART พร้อมส่งข้อมูล
  UDR0 = data;  // ส่งข้อมูลผ่าน UART
}

void i2c_init(void) {
  TWSR = 0x00;  // ตั้งค่า Prescaler ของ I2C เป็น 0
  TWBR = ((F_CPU / 100000UL) - 16) / 2;  // คำนวณค่า TWBR สำหรับอัตราบิต I2C 100 kHz
  TWCR = (1 << TWEN);  // เปิดใช้งานโมดูล I2C
}

void i2c_start(void) {
  TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);  // ส่งสัญญาณเริ่มต้น I2C
  while (!(TWCR & (1 << TWINT)));  // รอจนกว่าการส่งสัญญาณเริ่มต้นจะสำเร็จ
}

void i2c_stop(void) {
  TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);  // ส่งสัญญาณหยุด I2C
}

void i2c_write(uint8_t data) {
  TWDR = data;  // ตั้งค่าข้อมูลที่ต้องการส่งไปยัง TWDR
  TWCR = (1 << TWINT) | (1 << TWEN);  // เริ่มการส่งข้อมูล
  while (!(TWCR & (1 << TWINT)));  // รอจนกว่าข้อมูลจะถูกส่งไป
}

void start_adc_conversion() {
  ADCSRA |= (1 << ADSC);  // เริ่มการแปลงข้อมูลของ ADC
}

int read_adc_value() {
  while (ADCSRA & (1 << ADSC));  // รอจนกว่าการแปลงข้อมูลจะเสร็จสิ้น
  return ADC;  // อ่านค่าที่ได้จาก ADC
}

void EEPROM_write(uint8_t uiAddress, uint8_t ucData) {
  while (EECR & (1 << EEPE));  // รอจนกว่า EEPROM จะพร้อม
  EEAR = uiAddress;  // ตั้งค่าที่อยู่ที่ต้องการเขียนข้อมูล
  EEDR = ucData;  // ตั้งค่าข้อมูลที่ต้องการเขียน
  EECR |= (1 << EEMPE);  // เปิดใช้งานโหมดการเขียน
  EECR |= (1 << EEPE);  // เริ่มการเขียนข้อมูลลง EEPROM
}

uint8_t EEPROM_read(uint8_t uiAddress) {
  while (EECR & (1 << EEPE));  // รอจนกว่า EEPROM จะพร้อม
  EEAR = uiAddress;  // ตั้งค่าที่อยู่ที่ต้องการอ่านข้อมูล
  EECR |= (1 << EERE);  // เปิดใช้งานโหมดการอ่าน
  return EEDR;  // อ่านข้อมูลที่ได้จาก EEPROM
}

void save_output_state(void) {
  EEPROM_write(EEPROM_ADDRESS_OUTPUT_STATE, output_state);  // เขียนสถานะเอาต์พุตปัจจุบันลง EEPROM
}

void load_output_state(void) {
  output_state = EEPROM_read(EEPROM_ADDRESS_OUTPUT_STATE);  // โหลดสถานะเอาต์พุตจาก EEPROM
}

void init_millis(unsigned long f_cpu) {
  unsigned long ctc_match_overflow;  // ค่านี้ใช้ในการตั้งค่า timer
  ctc_match_overflow = ((f_cpu / 1000) / 64) - 1;  // คำนวณค่าสำหรับ timer CTC mode
  TCCR1B |= (1 << WGM12);  // ตั้งค่าให้ Timer1 ทำงานในโหมด CTC
  OCR1A = (uint16_t)ctc_match_overflow;  // ตั้งค่าเป้าหมายการเปรียบเทียบ
  TIMSK1 |= (1 << OCIE1A);  // เปิดใช้งาน interrupt ของ Timer1 ใน OCR1A
  TCCR1B |= ((1 << CS11) | (1 << CS10));  // ตั้งค่า prescaler ของ timer
}

ISR(TIMER1_COMPA_vect) {
  timer_millis++;  // นับเวลาที่ผ่านไปทุกๆ 1 มิลลิวินาที
}

unsigned long millis() {
  unsigned long millis_return;

  ATOMIC_BLOCK(ATOMIC_FORCEON) {  // บล็อกนี้ใช้เพื่อป้องกันการถูกขัดจังหวะ
    millis_return = timer_millis;  // อ่านค่าเวลาที่ผ่านไป
  }
  
  return millis_return;  // คืนค่าเวลาที่ผ่านไปในหน่วยมิลลิวินาที
}
