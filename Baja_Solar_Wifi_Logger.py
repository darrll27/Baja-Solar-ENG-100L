import requests
import re
import time
import csv
from datetime import datetime

# ESP32's IP address
esp32_ip = "192.168.4.1"  # Replace with your ESP32's actual IP address

# CSV file path
csv_file = "data.csv"

# Define regular expressions to extract data
set_temp_pattern = r"Set Temperature:\s*([\d.-]+)"
target_temp_pattern = r"Target Temperature:\s*([\d.-]+)"
pump_status_pattern = r"Pump Status:\s*([\d.-]+)"
last_temp_1_pattern = r"Last Temperature \(Sensor 1\):\s*([\d.-]+)"
last_temp_2_pattern = r"Last Temperature \(Sensor 2\):\s*([\d.-]+)"
last_flow_rate_pattern = r"Last Flow Rate:\s*([\d.-]+)"

# Create the CSV file and write the first row with data types
with open(csv_file, mode='w', newline='') as file:
    writer = csv.writer(file)
    writer.writerow(["Timestamp", "Set Temperature", "Target Temperature", "Pump Status", "Last Temperature (Sensor 1)", "Last Temperature (Sensor 2)", "Last Flow Rate"])

# Collect data 10 times a second
while True:
    try:
        response = requests.get(f"http://{esp32_ip}/")
        if response.status_code == 200:
            data = response.text

            # Extract data using regular expressions
            set_temp = re.search(set_temp_pattern, data).group(1)
            target_temp = re.search(target_temp_pattern, data).group(1)
            pump_status = re.search(pump_status_pattern, data).group(1)
            last_temp_1 = re.search(last_temp_1_pattern, data).group(1)
            last_temp_2 = re.search(last_temp_2_pattern, data).group(1)
            last_flow_rate = re.search(last_flow_rate_pattern, data).group(1)

            # Get the current timestamp
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")

            # Append the data to the CSV file
            with open(csv_file, mode='a', newline='') as file:
                writer = csv.writer(file)
                writer.writerow([timestamp, set_temp, target_temp, pump_status, last_temp_1, last_temp_2, last_flow_rate])

        else:
            print(f"Failed to fetch data. Status code: {response.status_code}")
    except Exception as e:
        print(f"An error occurred: {e}")

    # Sleep for 0.1 seconds (10 times a second)
    time.sleep(0.1)
