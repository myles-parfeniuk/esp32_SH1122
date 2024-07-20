"""
sh1122_screenshot.py

Description:
	This script captures screenshots from calls to take_screen_shot().
	It receives the frame buffer over serial, which is then decoded, processed, 
	and saved as an image.

	After launching the script will continue to run and take screenshots until the 'q'
	key is hit.

Dependencies:
    - serial (pyserial library)
    - keyboard
    - terminaltables
    - PIL (Python Imaging Library)
    - argparse

Usage:
    python sh1122_screenshot.py <comport_name> [-b <baud_rate>] [-s <scaling_factor>]

Args:
    comport_name (str): Name of the serial port to which the esp32 is connected.
    -b, --baud_rate (int): Baud rate for serial communication (default: 115200).
    -s, --scaling_factor (int): Multiplier for image size (default: 1 (no change, 256x64)).

Example:
    python sh1122_screenshot.py COM5 -b 115200 -s 2 
"""
import serial
import serial.tools.list_ports
import threading
import keyboard
from terminaltables import AsciiTable
import time
from PIL import Image
import argparse
import os

OLED_HEIGHT = 64
OLED_WIDTH = 256
FRAME_BUFFER_LENGTH = 8192
stop_scanning_event = threading.Event()

"""
Task: task_scan_keyboard

Description:
    Monitors keyboard input to detect 'q' key press, ending screenshot session.

Args:
    None
    
Returns:
    None
"""
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

"""
Function: read_screenshot

Description:
    Receives a single screenshot from esp32 over serial.

Args:
    comport (serial.Serial): The serial port object to read from.
    
Returns:
    tuple: Tuple containing the screenshot data (string) and timestamp since boot in microseconds (int).
"""
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

"""
Function: parse_screenshot

Description:
    Parses the raw screenshot data into a list of integers that matches the original frame buffer.

Args:
    screenshot (str): Raw screenshot data as a comma-separated string.
    
Returns:
    list: Corrected frame buffer as a list of integers.
"""
def parse_screenshot(screenshot):
	corrected_frame_buffer = []
	screenshot = screenshot.split(',')
	numeric_strings = [s for s in screenshot if s.strip().isdigit()]
	frame_buffer = [int(elem_val) for elem_val in numeric_strings]
	for i in range(len(frame_buffer)):
		corrected_frame_buffer.append(frame_buffer[i] & 0x00FF)
		corrected_frame_buffer.append((frame_buffer[i] & 0xFF00) >> 8)

	return corrected_frame_buffer

"""
Function: decode_frame_buffer

Description:
    Decodes the frame buffer into pixel intensity data.

Args:
    frame_buffer (list): List of integers representing the frame buffer.
    
Returns:
    list: 2D list ([y][x]) representing pixel intensity data for the given screenshot.
"""
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

"""
Function: map_gray_scale_value

Description:
    Maps scaled intensity values to BW color values to be used by PIL.

Args:
    scaled_value (int): Scaled intensity value (0-15).
    
Returns:
    int: BW color value mapped from the scaled intensity.
"""
def map_gray_scale_value(scaled_value):
	return scaled_value * 255 // 15

"""
Function: create_image

Description:
    Creates an image from pixel intensity data.

Args:
    pixel_data (list): 2D list of pixel intensity values.
    scaling_factor (int): Scaling factor for image size.
    
Returns:
    Image: PIL Image object representing the created image.
"""
def create_image(pixel_data, scaling_factor):
	img = Image.new('L', (OLED_WIDTH * scaling_factor, OLED_HEIGHT * scaling_factor))

	for y in range(OLED_HEIGHT):
		for x in range(OLED_WIDTH):
			pixel_color = map_gray_scale_value(pixel_data[y][x])

			#apply scaling
			for dy in range(scaling_factor):
				for dx in range(scaling_factor):
					img.putpixel((x * scaling_factor + dx, y * scaling_factor + dy), pixel_color)

	return img

"""
Function: save_image

Description:
    Saves an image to the output directory.

Args:
    img (Image): PIL Image object to be saved.
    shot_total (int): Total number of screenshots taken.
    time_stamp (int): Timestamp of the screenshot in microseconds.
    
Returns:
    None
"""
def save_image(img, shot_total, time_stamp):

	file_name = f"screenshot_{shot_total}_{time_stamp}_ms.png"
	print(file_name + " saved.")
	img.save(f"output/{file_name}")

"""
Function: save_images

Description:
    Saves screenshots as .png files with timestamps appended to filenames, in output directory.
Args:
    images (list): List of tuples containing pixel data,
                   shot total, and timestamp for each image.
   	scaling_factor (int): The image width and height multiplier.
    
Returns:
    None
"""
def save_images(images, scaling_factor):
	os.makedirs("output/", exist_ok=True)  #create output directory if it does not exist
	for i in range(len(images)):
		time_stamp = images[i][2]
		shot_total = images[i][1]
		pixel_data = images[i][0]

		if(i == 0):
			time_stamp_corrected = 0;
			prev_time_stamp = time_stamp
		else:
			d_t = (time_stamp- prev_time_stamp) // 1000 #divide by 1000 to convert to ms
			prev_time_stamp = time_stamp
			time_stamp_corrected += d_t

		img = create_image(images[i][0], scaling_factor)

		save_image(img, shot_total, time_stamp_corrected)


if __name__ == '__main__':
	task_scan_keyboard_hdl = threading.Thread(target = task_scan_keyboard, name = 'Scan Keyboard Task')
	parser = argparse.ArgumentParser(prog = 'sh1122 screenshot', description='Takes screenshots of SH1122 driven displays for building documentation.')
	parser.add_argument('comport_name', type = str, help='Name of comport for which esp32 is attatched, for ex. \'COM*\' on windows or \'dev/ttyUSB*\' on linux')
	parser.add_argument('-b', '-B', '--baud_rate', type=int, default=115200, help = 'Baud rate of esp32 serial communications, in idf.py menuconfig: component config->ESP System Settings -> UART console baud rate.')
	parser.add_argument('-s', '-S', '--scaling_factor', type=int, default = 1, help = 'Multiplies the width and height for a larger image (defualt = 1, no change)')
	args = parser.parse_args()
	images_to_save = []

	baud_rate = args.baud_rate
	comport_name = args.comport_name
	scaling_factor = args.scaling_factor

	comport = serial.Serial()
	comport.port = comport_name
	"""prevents esp32 from resetting upon connection"""
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
				shot_total+=1
				images_to_save.append((pixel_data, shot_total, time_stamp))
				print(f"{shot_total} screenshots captured, press q to quit and save.")
			else:
				print("Screenshot dropped, error.")

		comport.close()
		save_images(images_to_save, scaling_factor)
		task_scan_keyboard_hdl.join()

	except serial.SerialException as e:
		print(e)
	
	print("sh1122_screenshot.py exited...")		