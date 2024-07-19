import serial
import serial.tools.list_ports
import threading
import keyboard
from terminaltables import AsciiTable
import time
from PIL import Image
import argparse

OLED_HEIGHT = 64
OLED_WIDTH = 256
FRAME_BUFFER_LENGTH = 8192
stop_scanning_event = threading.Event()

def task_scan_keyboard():
	global stop_scanning_event

	while(True):
		if keyboard.is_pressed('q'):
			stop_scanning_event.set()
			time.sleep(0.5)
			print("--------------------------------------")
			print("quitting, please wait...")
			break
		time.sleep(0.1)

def get_serial_ports():
   	
	port_list = []
	port_list.append(['Name', 'Description'])

	for p in serial.tools.list_ports.comports():
		port_list.append([p.device, p.description])

	return port_list

def read_screenshot(comport):
	time_stamp = 0
	screenshot = ""
	line = ""

	while line != "SCREENSHOT_S":
		line = comport.readline().decode('utf-8').strip()

	line = comport.readline().decode('utf-8').strip()
	line = line.split()

	if line[0] == "TIMESTAMP":
		time_stamp = int(line[1])

		while line != "SCREENSHOT_P":
			line = comport.readline().decode('utf-8').strip()
			if line != "SCREENSHOT_P":
				screenshot += line

	return (screenshot, time_stamp)

def parse_screenshot(screenshot):
	corrected_frame_buffer = []
	screenshot = screenshot.split(',')
	numeric_strings = [s for s in screenshot if s.strip().isdigit()]
	frame_buffer = [int(elem_val) for elem_val in numeric_strings]
	for i in range(len(frame_buffer)):
		corrected_frame_buffer.append(frame_buffer[i] & 0x00FF)
		corrected_frame_buffer.append((frame_buffer[i] & 0xFF00) >> 8)

	return corrected_frame_buffer

def decode_frame_buffer(frame_buffer):
	x_it = 0
	y_it = 0
	pixel_data = [[0]*OLED_WIDTH for _ in range(OLED_HEIGHT)]

	for y in range(OLED_HEIGHT):
		for x in range(OLED_WIDTH):
			if x != 0:
				x_it = x // 2

			if y != 0:
				y_it = (y * OLED_WIDTH) // 2

			pixel = frame_buffer[x_it + y_it]

			if x % 2 == 1:
				intensity = pixel & 0x0F
			else:
				intensity = (pixel & 0xF0) >> 4

			pixel_data[y][x] = intensity

	return pixel_data

def map_gray_scale_value(scaled_value):
	return scaled_value * 255 // 15

def create_image(pixel_data, scaling_factor):
	img = Image.new('L', (OLED_WIDTH * scaling_factor, OLED_HEIGHT * scaling_factor))

	for y in range(OLED_HEIGHT):
		for x in range(OLED_WIDTH):
			pixel_color = map_gray_scale_value(pixel_data[y][x])

			# Set the 2x2 block of pixels in the scaled image
			for dy in range(scaling_factor):
				for dx in range(scaling_factor):
					img.putpixel((x * scaling_factor + dx, y * scaling_factor + dy), pixel_color)

	return img

def save_image(img, shot_total, time_stamp):
	file_name = f"screenshot_{shot_total}_{time_stamp}_ms.png"
	print(file_name + " saved.")
	img.save(f"output/{file_name}")

def save_images(images):
	for i in range(len(images)):
		if(i == 0):
			time_stamp_corrected = 0;
			prev_time_stamp = images[i][2]
		else:
			d_t = (images[i][2] - prev_time_stamp) // 1000
			prev_time_stamp = images[i][2]
			time_stamp_corrected += d_t

		save_image(images[i][0], images[i][1], time_stamp_corrected)


if __name__ == '__main__':
	task_scan_keyboard_hdl = threading.Thread(target = task_scan_keyboard, name = 'Scan Keyboard Task')
	parser = argparse.ArgumentParser(prog = 'sh1122 screenshot', description='Takes screenshots of SH1122 driven displays for building documentation.')
	parser.add_argument('comport_name', type = str, help='Name of comport for which esp32 is attatched, for ex. \'COM*\' on windows or \'dev/ttyUSB*\' on linux')
	parser.add_argument('-b', '--baud_rate', type=int, default=115200, help = 'Baud rate of esp32 serial communications, in idf.py menuconfig: component config->ESP System Settings -> UART console baud rate.')
	args = parser.parse_args()
	images_to_save = []
	scaling_factor = 2

	baud_rate = args.baud_rate
	comport_name = args.comport_name

	comport = serial.Serial()
	comport.port = comport_name
	comport.setDTR(False)
	comport.setRTS(False)
	comport.baudrate = baud_rate

	
	try:
		comport.open()
		print(f"serial port {comport_name} successfully opened at {baud_rate}baud...")
		for i in range(2):
			comport.readline()

		task_scan_keyboard_hdl.start()
		shot_total = 0

		while not stop_scanning_event.is_set():
			screenshot, time_stamp = read_screenshot(comport)
			frame_buffer = parse_screenshot(screenshot)

			if len(frame_buffer) == FRAME_BUFFER_LENGTH:
				pixel_data = decode_frame_buffer(frame_buffer)
				img = create_image(pixel_data, scaling_factor)
				shot_total+=1
				images_to_save.append((img, shot_total, time_stamp))
				print(f"{shot_total} screenshots captured, press q to quit and save.")
			else:
				print("Screenshot dropped, error.")

		comport.close()
		save_images(images_to_save)
		task_scan_keyboard_hdl.join()

	except serial.SerialException as e:
		print(e)
	
	print("sh1122_screenshot.py exited...")		