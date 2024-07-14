from PIL import Image
import os
import argparse


def map_gray_scale_value(raw_value, transparency):
	if(transparency):
		if raw_value[1] > 30:
			return ((raw_value[0] * 15) // 255)
		else:
			return 16

	else:
		return ((raw_value * 15) // 255)

def load_image(filepath, transparency):
	if transparency:
		im = Image.open(filepath).convert('LA') #load image and convert to black and white with alpha channel
	else:
		im = Image.open(filepath).convert('L') #load image and convert to black and white

	cols, rows = im.size
	#resize image if its width exceeds oled width, maintain aspect ratio
	if cols > 256:
		height = int(im.height * (256 / im.width))
		im = im.resize((256, height), Image.LANCZOS) #lanczos interpolation for best result when downscaling 
		cols, rows = im.size

	#resize image if its height exceeds oled height, maintain aspect ratio
	if rows > 64:
		width = int(im.width * (64 / im.height))
		im = im.resize((width, 64), Image.LANCZOS)

	return im

def encode_bitmap(im, transparency):
	pixels = im.load()
	cols, rows = im.size
	array_str = " " + str(cols) + ", " + str(rows) + ", \n" #store the width and height of the bitmap
	elem_count = 2
	gray_scale_value = map_gray_scale_value(pixels[0, 0], transparency) #remap raw bw value to a valid Sh1122Oled::PixelIntensity level
	prev_gray_scale_value = gray_scale_value
	repeated_val_cnt = 0; 
	total_pixels = 0

	#bitmap data is stored in groups of 2 elements, intensity followed by the amount of times the given intensity repeats, ie (15, 5) means 5 pixels of intensity 15 in a row
	for y in range(rows):
		for x in range(cols):
			gray_scale_value = map_gray_scale_value(pixels[x, y], transparency)

			if (gray_scale_value != prev_gray_scale_value) or (repeated_val_cnt >= 255):
				array_str += " " + str(prev_gray_scale_value) + ", " + str(repeated_val_cnt) + ",\n" #the lower element is grayscale intesity, upper is amount of times it appears in sequence
				elem_count += 2
				total_pixels += repeated_val_cnt
				repeated_val_cnt = 0


			repeated_val_cnt += 1

			if (x == (cols - 1)) and (y == rows - 1):
				array_str += " " + str(gray_scale_value) + ", " + str(repeated_val_cnt) + "\n"
				elem_count += 2
				total_pixels += repeated_val_cnt
				break

			prev_gray_scale_value = gray_scale_value

	array_str = " " + str(((elem_count - 2) & 0xFF00) >> 8) + ", " + str((elem_count - 2) & 0x00FF) + ", \n" + array_str #store the total size of the bitmap data
	elem_count += 2

	return (array_str, elem_count)

def write_file(filename, array_data, total_elements):
	filename = filename[:-4]
	filename = "sh1122_bitmap_" + filename
	file_str = "#pragma once \n"
	file_str += "#include <stdint.h> \n\n"
	file_str += "static const uint8_t " + filename + "[" + str(total_elements) + "] = \n{ \n"
	file_str += array_data
	file_str += "};"
	filename += ".hpp"
	filepath = f"output/{filename}"
	with open(filepath, 'w') as file:
		file.write(file_str)
	print(filename + " successfully converted.")

def main(transparency):
	input_dir = 'input'

	for filename in os.listdir(input_dir):
		if filename.endswith('.png'):
			filepath = os.path.join(input_dir, filename)
			
			im = load_image(filepath, transparency)
			array_data, total_elements = encode_bitmap(im, transparency)
			write_file(filename, array_data, total_elements)

if __name__ == "__main__":
	parser = argparse.ArgumentParser(description='Process some arguments.')
	parser.add_argument('-T', '--transparency', action='store_true', help='Enable transparency')
	args = parser.parse_args()
	main(args.transparency)


