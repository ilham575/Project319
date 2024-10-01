import time  # ใช้สำหรับการหยุดโปรแกรมชั่วคราว
import json  # ใช้สำหรับจัดการกับข้อมูลในรูปแบบ JSON
import requests  # ใช้สำหรับการส่งคำขอ HTTP ไปยังเซิร์ฟเวอร์
import serial  # ใช้สำหรับการเชื่อมต่อผ่านพอร์ตอนุกรม
import modbus_tk.defines as cst  # นำเข้า constants สำหรับโปรโตคอล Modbus
from modbus_tk import modbus_rtu  # นำเข้าโมดูล Modbus RTU

# การตั้งค่า ThingSpeak
THINGSPEAK_API_KEY = '6F82C4I0ANATTF3B'  # คีย์ API สำหรับส่งข้อมูลไปยัง ThingSpeak
THINGSPEAK_BASE_URL = 'https://api.thingspeak.com/update'  # URL พื้นฐานสำหรับการอัปเดตข้อมูลไปยัง ThingSpeak

# การตั้งค่า LINE Notify
LINE_NOTIFY_URL = 'https://notify-api.line.me/api/notify'  # URL สำหรับการส่งการแจ้งเตือนไปยัง LINE Notify
LINE_NOTIFY_TOKEN = 'fs8o9LcPLGcsqCflPLFT2V5kNxNk5ewJSHzn8UaaktM'  # โทเค็นการรับรองสิทธิ์ของ LINE Notify
LINE_NOTIFY_HEADERS = {  # หัวข้อที่จำเป็นสำหรับการแจ้งเตือนของ LINE Notify
    'content-type': 'application/x-www-form-urlencoded',  # ประเภทของเนื้อหา
    'Authorization': 'Bearer ' + LINE_NOTIFY_TOKEN  # หัวข้อ Authorization พร้อมโทเค็น
}

def send_line_notification(message):
    """ส่งการแจ้งเตือนทาง LINE ด้วยข้อความที่ระบุ"""
    payload = {'message': message}  # ข้อความที่ต้องการส่ง
    response = requests.post(LINE_NOTIFY_URL, headers=LINE_NOTIFY_HEADERS, data=payload)  # ส่งคำขอ POST ไปยัง LINE Notify
    if response.status_code == 200:  # ตรวจสอบสถานะการตอบกลับ
        print("ส่งการแจ้งเตือนไปยัง LINE สำเร็จ!")  # แสดงข้อความเมื่อส่งสำเร็จ
    else:
        print(f"การส่งการแจ้งเตือนไปยัง LINE ล้มเหลว รหัสสถานะ: {response.status_code}")  # แสดงข้อความเมื่อส่งล้มเหลว

if __name__ == "__main__":
    try:
        # เชื่อมต่อกับ PZEM-004T ผ่าน Modbus RTU
        serial_connection = serial.Serial(
            port='/dev/ttyUSB0',  # ระบุพอร์ตที่เชื่อมต่อกับอุปกรณ์
            baudrate=9600,  # กำหนด baud rate ของการเชื่อมต่อ
            bytesize=8,  # ขนาดของบิตต่อไบต์
            parity='N',  # ไม่มี parity bit
            stopbits=1,  # จำนวน stop bit
            xonxoff=0  # ไม่ใช้การควบคุมการไหล
        )

        master = modbus_rtu.RtuMaster(serial_connection)  # สร้างตัวควบคุม Modbus RTU
        master.set_timeout(2.0)  # ตั้งเวลาหมดเวลาเป็น 2 วินาที
        master.set_verbose(True)  # เปิดโหมดแสดงรายละเอียด

        while True:
            # อ่านข้อมูลจาก PZEM-004T
            data = master.execute(1, cst.READ_INPUT_REGISTERS, 0, 10)  # ดึงข้อมูลจาก PZEM-004T ผ่าน Modbus

            voltage = data[0] / 10.0  # แปลงข้อมูลแรงดันไฟฟ้า
            current_A = (data[1] + (data[2] << 16)) / 1000.0  # แปลงข้อมูลกระแสไฟฟ้า [A]
            power_W = (data[3] + (data[4] << 16)) / 10.0  # แปลงข้อมูลกำลังไฟฟ้า [W]
            energy_Wh = data[5] + (data[6] << 16)  # แปลงข้อมูลพลังงานไฟฟ้าที่ใช้ [Wh]
            frequency_Hz = data[7] / 10.0  # แปลงข้อมูลความถี่ไฟฟ้า [Hz]
            power_factor = data[8] / 100.0  # แปลงข้อมูลค่า power factor
            alarm = data[9]  # ดึงสถานะการเตือน (0 = ไม่มีเตือน)

            # สร้าง dictionary เพื่อจัดเก็บข้อมูล
            dict_payload = {
                "voltage": voltage,
                "current_A": current_A,
                "power_W": power_W,
                "energy_Wh": energy_Wh,
                "frequency_Hz": frequency_Hz,
                "power_factor": power_factor,
                "alarm": alarm
            }

            # แสดงข้อมูลเพื่อการดีบัก
            print(json.dumps(dict_payload, indent=2))

            # เตรียมข้อมูลเพื่ออัปโหลดไปยัง ThingSpeak
            thingspeak_payload = {
                'api_key': THINGSPEAK_API_KEY,  # ใช้คีย์ API ของ ThingSpeak
                'field1': voltage,  # ช่อง 1: แรงดันไฟฟ้า
                'field2': current_A,  # ช่อง 2: กระแสไฟฟ้า
                'field3': power_W,  # ช่อง 3: กำลังไฟฟ้า
                'field4': energy_Wh,  # ช่อง 4: พลังงานไฟฟ้า
                'field5': frequency_Hz,  # ช่อง 5: ความถี่ไฟฟ้า
                'field6': power_factor,  # ช่อง 6: ค่า power factor
                'field7': alarm  # ช่อง 7: สถานะการเตือน
            }

            # ส่งข้อมูลไปยัง ThingSpeak
            response = requests.get(THINGSPEAK_BASE_URL, params=thingspeak_payload)  # ส่งคำขอ GET ไปยัง ThingSpeak
            if response.status_code == 200:  # ตรวจสอบว่าการส่งสำเร็จหรือไม่
                print("ส่งข้อมูลไปยัง ThingSpeak สำเร็จ")  # แสดงข้อความเมื่อส่งสำเร็จ
            else:
                print(f"การส่งข้อมูลไปยัง ThingSpeak ล้มเหลว รหัสสถานะ: {response.status_code}")  # แสดงข้อความเมื่อส่งล้มเหลว

            # ตรวจสอบว่ากำลังไฟฟ้าเกิน 3500 วัตต์หรือไม่
            if power_W > 3500:
                notification_message = f"เตือน! การใช้พลังงานเกิน 3500W: {power_W}W"  # สร้างข้อความการแจ้งเตือน
                send_line_notification(notification_message)  # ส่งการแจ้งเตือนไปยัง LINE

            # หยุดโปรแกรมชั่วคราวก่อนทำงานซ้ำ
            time.sleep(15)

    except KeyboardInterrupt:  # จับสัญญาณการหยุดโปรแกรมจากคีย์บอร์ด
        print('ออกจากสคริปต์...')
    except Exception as e:  # จับข้อผิดพลาดอื่นๆ
        print(f"เกิดข้อผิดพลาด: {e}")  # แสดงข้อความข้อผิดพลาด
    finally:
        master.close()  # ปิดการเชื่อมต่อ Modbus
