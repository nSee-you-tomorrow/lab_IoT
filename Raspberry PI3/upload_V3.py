import serial
import random
import time
import urllib.request
import os
import json
import datetime
from datetime import datetime
import gspread
from oauth2client.service_account import ServiceAccountCredentials
import RPi.GPIO as GPIO

#Setup mode GPIO
GPIO.setmode(GPIO.BCM)
# Relay 1,2 PUM IN
GPIO.setup(17, GPIO.OUT)
GPIO.setup(27, GPIO.OUT)
# Relay 3,4 PUM OUT
GPIO.setup(5, GPIO.OUT)
GPIO.setup(6, GPIO.OUT)
# Set all Pump to OFF
GPIO.output(17, 1)
GPIO.output(27, 1)
GPIO.output(5, 1)
GPIO.output(6, 1)

key = '03V8FS8A9EVRBWVU'  
spreadsheetId = '10Myrw7_qE3QmQu1EYNFFTVnnWfsACi2bpg-5YQbge2s'
namesheet = 'DATA'
baseURL = 'https://api.thingspeak.com/update?api_key=%s' % key


def readData():
    while True:
        try:
            ser = serial.Serial(
                port='/dev/ttyUSB0',
                baudrate=9600,
            )
            read_serial = ser.readline()
            print(read_serial)
            j = json.loads(read_serial.decode("utf-8"))
            return j
        except Exception as e:
            return None
        break

def thermometer(j):
    if j is None:
        print("no data")
        return False
    try:
        # Sending the data to thingspeak
        conn = urllib.request.urlopen(baseURL + '&field1=%s&field2=%s&field3=%s&field4=%s&field5=%s&field6=%s' % (
            j['temp'], j['humidity'], j['light'], j['ec'], j['ph'], j['waterTemp']))
        # print(j['temp'], j['humidity'], j['light'], j['ec'], j['ph'], j['waterTemp'])
        # Closing the connection
        conn.close()
        return True
    except Exception as e:
        print(e)
        # print("connection fail")
        return False

def writeFile(j):
    try:
        f = open('data.txt', 'a')
        str = '{"temp":%s,"humidity":%s,"light":%s,"ec":%s,"ph":%s,"waterTemp":%s}' % (
        j['temp'], j['humidity'], j['light'], j['ec'], j['ph'], j['waterTemp'])
        f.write(str + " " + time.ctime())
        f.write('\n')
        # print("write success")
    except Exception as e:
        print(e)
        # print("write fail")

def authen():
	try:
		scope = ['https://spreadsheets.google.com/feeds','https://www.googleapis.com/auth/drive']
		creds = ServiceAccountCredentials.from_json_keyfile_name('client_secret.json', scope)
		client = gspread.authorize(creds)
		return client
	except Exception as e:
		print(e)
		return False

def upload_sheet(j,client):
	if j is None:
		print("no data")
		return False
	try:
		sheet = client.open_by_key(spreadsheetId).worksheet(namesheet)
		index = int(sheet.acell('J2').value) + 1
		row = [index,datetime.now().strftime('%d-%m-%Y | %H:%M:%S'),j['temp'],j['humidity'],j['light'],j['ec'],j['ph'],j['waterTemp']]
		sheet.insert_row(row, index+1)
		sheet.update_acell('J2',index)
		return True
	except Exception as e:
		print(e)
		return False

def pump_out():
	print("=> PUMP OLD SOLUTION : 35s")
	GPIO.output(5, 0)
	GPIO.output(6, 0)
	time.sleep(40)
	GPIO.output(5, 1)
	GPIO.output(6, 1)
	print("=> OK!")

def pump_in():
	print("=> PUMP NEW SOLUTION : 30s")
	GPIO.output(17, 0)
	GPIO.output(27, 0)
	time.sleep(30)
	GPIO.output(17, 1)
	GPIO.output(27, 1)
	print("=> OK!")

if __name__ == "__main__":
	print("")
	print("START - PUMP OLD SOLUTION : 35s")
	pump_out()
	print("START - PIE IN : 30s")
	pump_in()
	print("START - PIE OUT : 35s")
	pump_out()

	status_solution = False
	key_end_day = 0
	print("=================== SYSTEM READY ===================")
	while True:
		time.sleep(1)
		now = datetime.now()
		current_hour = now.hour
		current_minute = now.minute
		time_str = "Time - " + str(current_hour) + " : " + str(current_minute)
		minute_of_day = current_hour*60 +current_minute
		if(current_hour>=5 and current_hour<20) :
			if(minute_of_day%5==0):
				if(status_solution == False):
					continue
				else:
					print(time_str)
					print("=> SEND DATA TO CLOUD")
					re = thermometer(j)
					client = authen()
					if(client!=False):
						print("=> SHEET OK!")
						re2 = upload_sheet(j,client)
					if (re):
						writeFile(j)
						print("=> THINGSPEAK OK!")
						time.sleep(60)
					else:
						print("=> THINGSPEAK FAIL! TRY AFTER 10s")
						time.sleep(10)
						continue

			if(minute_of_day%5==1):
				if(status_solution == True):
					print(time_str)
					pump_out()
					status_solution = False
				else:
					continue

			if(minute_of_day%5==3):
				if(status_solution == False):
					print(time_str)
					pump_in()
					status_solution = True
					print("=> WAIT STABILITY")
				else:
					continue

		if(current_hour ==19 && key_end_day == 1):
			key_end_day = 0
			continue

		if(current_hour ==20 && key_end_day == 0):
			pump_out()
			key_end_day =1
			continue

		else:
			continue

# while True:
#         time.sleep(10)
#         # j = readData()
#         re = thermometer(j)
#         if (re):
#             writeFile(j)
#             time.sleep(900)
#         else:
#             time.sleep(10)
#             continue
