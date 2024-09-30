import time
import json
import requests
import serial
import modbus_tk.defines as cst
from modbus_tk import modbus_rtu

# ThingSpeak settings
THINGSPEAK_API_KEY = '6F82C4I0ANATTF3B'
THINGSPEAK_BASE_URL = 'https://api.thingspeak.com/update'

# LINE Notify settings
LINE_NOTIFY_URL = 'https://notify-api.line.me/api/notify'
LINE_NOTIFY_TOKEN = 'fs8o9LcPLGcsqCflPLFT2V5kNxNk5ewJSHzn8UaaktM'
LINE_NOTIFY_HEADERS = {
    'content-type': 'application/x-www-form-urlencoded',
    'Authorization': 'Bearer ' + LINE_NOTIFY_TOKEN
}

def send_line_notification(message):
    """Send a LINE notification with the specified message."""
    payload = {'message': message}
    response = requests.post(LINE_NOTIFY_URL, headers=LINE_NOTIFY_HEADERS, data=payload)
    if response.status_code == 200:
        print("LINE notification sent successfully!")
    else:
        print(f"Failed to send LINE notification. Status code: {response.status_code}")

if __name__ == "__main__":
    try:
        # Connect to the PZEM-004T (Modbus RTU)
        serial_connection = serial.Serial(
            port='/dev/ttyUSB0',
            baudrate=9600,
            bytesize=8,
            parity='N',
            stopbits=1,
            xonxoff=0
        )

        master = modbus_rtu.RtuMaster(serial_connection)
        master.set_timeout(2.0)
        master.set_verbose(True)

        while True:
            # Read data from PZEM-004T
            data = master.execute(1, cst.READ_INPUT_REGISTERS, 0, 10)
            
            voltage = data[0] / 10.0
            current_A = (data[1] + (data[2] << 16)) / 1000.0  # [A]
            power_W = (data[3] + (data[4] << 16)) / 10.0  # [W]
            energy_Wh = data[5] + (data[6] << 16)  # [Wh]
            frequency_Hz = data[7] / 10.0  # [Hz]
            power_factor = data[8] / 100.0
            alarm = data[9]  # 0 = no alarm

            # Create a dictionary to store the data
            dict_payload = {
                "voltage": voltage,
                "current_A": current_A,
                "power_W": power_W,
                "energy_Wh": energy_Wh,
                "frequency_Hz": frequency_Hz,
                "power_factor": power_factor,
                "alarm": alarm
            }

            # Print the data for debugging
            print(json.dumps(dict_payload, indent=2))

            # Prepare data for ThingSpeak upload
            thingspeak_payload = {
                'api_key': THINGSPEAK_API_KEY,
                'field1': voltage,        # Field 1: Voltage
                'field2': current_A,      # Field 2: Current
                'field3': power_W,        # Field 3: Power
                'field4': energy_Wh,      # Field 4: Energy
                'field5': frequency_Hz,   # Field 5: Frequency
                'field6': power_factor,   # Field 6: Power Factor
                'field7': alarm           # Field 7: Alarm Status
            }

            # Send the data to ThingSpeak
            response = requests.get(THINGSPEAK_BASE_URL, params=thingspeak_payload)
            if response.status_code == 200:
                print("Data successfully sent to ThingSpeak")
            else:
                print(f"Failed to send data to ThingSpeak. Status code: {response.status_code}")

            # Check if power exceeds 3500 watts
            if power_W > 3500:
                notification_message = f"Alert! Power consumption has exceeded 3500W: {power_W}W"
                send_line_notification(notification_message)

            # Wait before sending the next data
            time.sleep(15)

    except KeyboardInterrupt:
        print('Exiting the script...')
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        master.close()
