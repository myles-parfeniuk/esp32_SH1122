from PIL import Image
import os
import argparse

OLED_WIDTH = 256
OLED_HEIGHT = 64
PIXEL_INTENSITY_TRANSPARENT = 16
PIXEL_INTENSITY_MAX = 15

BIT_0 = 1
BIT_1 = 2
BIT_2 = 4
BIT_3 = 8
BIT_4 = 16
BIT_5 = 32
BIT_6 = 64
BIT_7 = 128
BIT_8 = 256
BIT_9 = 512
BIT_10 = 1024

R_VAL_LOW_BIT_MSK = (BIT_0 | BIT_1 | BIT_2)
R_VAL_HIGH_BIT_MSK = (BIT_3 | BIT_4 | BIT_5 | BIT_6 | BIT_7 | BIT_8 | BIT_9)
R_VAL_WORD_MAX = 2**10 - 1
R_VAL_BYTE_MAX = 2**2 - 1
R_VAL_LOW_BIT_POS = 5
WORD_FLG_BIT = BIT_7

def map_gray_scale_value(raw_value, transparency):
	if(transparency):
		if raw_value[1] > 30:
			return ((raw_value[0] * PIXEL_INTENSITY_MAX) // (OLED_WIDTH - 1))
		else:
			return PIXEL_INTENSITY_TRANSPARENT

	else:
		return ((raw_value * PIXEL_INTENSITY_MAX) // (OLED_WIDTH - 1))

def load_image(filepath, transparency):
	if transparency:
		im = Image.open(filepath).convert('LA') #load image and convert to black and white with alpha channel
	else:
		im = Image.open(filepath).convert('L') #load image and convert to black and white

	cols, rows = im.size
	#resize image if its width exceeds oled width, maintain aspect ratio
	if cols > OLED_WIDTH:
		height = int(im.height * (OLED_WIDTH / im.width))
		im = im.resize((OLED_WIDTH, height), Image.LANCZOS) #lanczos interpolation for best result when downscaling 
		cols, rows = im.size

	#resize image if its height exceeds oled height, maintain aspect ratio
	if rows > OLED_HEIGHT:
		width = int(im.width * (OLED_HEIGHT / im.height))
		im = im.resize((width, OLED_HEIGHT), Image.LANCZOS)

	return im

def encode_word(gray_scale_value, repeated_val_cnt, inverted):
	if inverted:
		if gray_scale_value != PIXEL_INTENSITY_TRANSPARENT:
			gray_scale_value = PIXEL_INTENSITY_MAX - gray_scale_value

	r_val_count_high = (repeated_val_cnt & R_VAL_HIGH_BIT_MSK) >> 3
	r_val_count_low = repeated_val_cnt & R_VAL_LOW_BIT_MSK
	encoded_upper_byte = WORD_FLG_BIT | r_val_count_high
	encoded_lower_byte = (r_val_count_low << R_VAL_LOW_BIT_POS) | gray_scale_value
	return (encoded_upper_byte, encoded_lower_byte)

def encode_byte(gray_scale_value, repeated_val_cnt, inverted):
	if inverted:
		if gray_scale_value != PIXEL_INTENSITY_TRANSPARENT:
			gray_scale_value = PIXEL_INTENSITY_MAX - gray_scale_value

	encoded_byte = (repeated_val_cnt << R_VAL_LOW_BIT_POS) | gray_scale_value
	encoded_byte &= ~WORD_FLG_BIT
	return encoded_byte

def encode_bitmap(im, transparency, inverted):
	pixels = im.load()
	cols, rows = im.size
	array_str = " " + str(cols) + ", " + str(rows) + ", \n" #store the width and height of the bitmap
	elem_count = 2
	gray_scale_value = map_gray_scale_value(pixels[0, 0], transparency) #remap raw bw value to a valid Sh1122Oled::PixelIntensity level
	prev_gray_scale_value = gray_scale_value
	repeated_val_cnt = 0; 
	total_pixels = 0

	for y in range(rows):
		for x in range(cols):
			gray_scale_value = map_gray_scale_value(pixels[x, y], transparency)

			if (gray_scale_value != prev_gray_scale_value) or (repeated_val_cnt >=  R_VAL_WORD_MAX):
				if repeated_val_cnt > 2: #word length pixel data
					encoded_upper_byte, encoded_lower_byte = encode_word(prev_gray_scale_value, repeated_val_cnt, inverted)
					array_str += " " + str(encoded_upper_byte) + ", " + str(encoded_lower_byte) + ",\n" 
					elem_count += 2
				else: #byte length pixel data
					encoded_byte = encode_byte(prev_gray_scale_value, repeated_val_cnt, inverted)
					array_str += " " + str(encoded_byte) + ",\n"
					elem_count += 1

				total_pixels += repeated_val_cnt 
				repeated_val_cnt = 0

			repeated_val_cnt += 1

			if (x == (cols - 1)) and (y == rows - 1):
				if repeated_val_cnt > 2: #word length pixel data
					encoded_upper_byte, encoded_lower_byte = encode_word(gray_scale_value, repeated_val_cnt, inverted)
					array_str += " " + str(encoded_upper_byte) + ", " + str(encoded_lower_byte) + "\n" 
					elem_count += 2
				else: #byte length pixel data
					encoded_byte = encode_byte(prev_gray_scale_value, repeated_val_cnt, inverted)
					array_str += " " + str(encoded_byte) + "\n"
					elem_count += 1

				total_pixels += repeated_val_cnt 

				break

			prev_gray_scale_value = gray_scale_value

	array_str = " " + str(((elem_count - 2) & 0xFF00) >> 8) + ", " + str((elem_count - 2) & 0x00FF) + ", \n" + array_str #store the total size of the bitmap data
	elem_count += 2

	return (array_str, elem_count, cols, rows)

def write_file(filename, array_data, total_elements, cols, rows, transparency, inverted):
	og_filename = filename
	filename = filename[:-4]
	filename = "sh1122_bitmap_" + filename
	file_str = "#pragma once \n"
	file_str += "#include <stdint.h> \n\n"
	file_str += "/*\n"
	file_str += " Converted with sh1122_encode_bitmap.py \n Source Image Name: " + og_filename +"\n Width: " + str(cols) + "\n"
	file_str += " Height: " + str(rows) + "\n Conversion Settings: transparency= " + str(transparency) + ", inverted= " +str(inverted) +"\n"
	file_str += "*/\n"
	file_str += "static const uint8_t " + filename + "[" + str(total_elements) + "] = \n{ \n"
	file_str += array_data
	file_str += "};"
	filename += ".hpp"
	filepath = f"output/{filename}"
	with open(filepath, 'w') as file:
		file.write(file_str)
	print(filename + " successfully converted.")

def main(transparency, inverted):
	input_dir = 'input'

	for filename in os.listdir(input_dir):
		if filename.endswith('.png'):
			filepath = os.path.join(input_dir, filename)
			
			im = load_image(filepath, transparency)
			array_data, total_elements, cols, rows, = encode_bitmap(im, transparency, inverted)
			write_file(filename, array_data, total_elements, cols, rows, transparency, inverted)

if __name__ == "__main__":
	parser = argparse.ArgumentParser(prog = 'sh1122 encode bitmap', description='Encodes images with custom run-line format for 16 shade grayscale displays.')
	parser.add_argument('-T', '-t', '--transparency', action='store_true', help='Transparent pixels will not be ignored, if unused  they may appear as black or white.')
	parser.add_argument('-I', '-i', '--inverted', action='store_true', default = False, help = 'Invert the grayscale intensity of the generated bitmap.')
	args = parser.parse_args()
	main(args.transparency, args.inverted)


