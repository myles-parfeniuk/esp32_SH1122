"""
sh1122_encode_bitmap.py

This script encodes images into a custom run-length format suitable for 16-shade grayscale displays using an SH1122 controller.

Requirements:
- Python 3.x
- PIL (Python Imaging Library)

Usage:
python sh1122_encode_bitmap.py [-h] [-T] [-TT x] [-I]

Options:
  -h, --help            Show this help message and exit.
  -T, --transparency    Enable transparency mode. Preserves alpha channel if true.
  -TT x, --transparency_threshold x
                        Set threshold for pixel transparency (0-255). Default is 30.
  -I, --inverted        Invert grayscale intensity of the generated bitmap.

Output:
Generates a .hpp file in the 'output' directory for each PNG image found in the 'input' directory.

Note:
- The script resizes images to fit the SH1122 OLED dimensions (256x64) if they have not been presized.
- Transparency mode preserves alpha channel, converting images to black and white with alpha.
- When transparency mode is enabled, pixels with alpha channel values less than the passed threshold will be set to transparent.
  If the pixel's alpha channel level is above the transparency level, its scaled grayscale value will be used instead. 
- Inverted mode flips grayscale intensity values if specified.
"""
from PIL import Image
import os
import argparse

OLED_WIDTH = 256
OLED_HEIGHT = 64
PIXEL_INTENSITY_TRANSPARENT = 16
PIXEL_INTENSITY_MAX = 15
transparency_threshold = 30

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

"""
	Maps a raw grayscale value from image to a scaled 16 level value for SH1122.

	Args:
	    raw_value (int or tuple): The raw grayscale value or tuple (gray_value, transparency_value) if transparency is True.
	    transparency (bool): Flag indicating if transparency mode is enabled. Preserves alpha channel if true.

	Returns:
	    int: Scaled grayscale intensity value, or PIXEL_INTENSITY_TRANSPARENT if transparency condition is met.
"""
def map_gray_scale_value(raw_value, transparency):
	global transparency_threshold
	if(transparency):
		if raw_value[1] > transparency_threshold:
			return ((raw_value[0] * PIXEL_INTENSITY_MAX) // (OLED_WIDTH - 1))
		else:
			return PIXEL_INTENSITY_TRANSPARENT

	else:
		return ((raw_value * PIXEL_INTENSITY_MAX) // (OLED_WIDTH - 1))

"""
	Opens an image, converts it to either black and white, or black and white with an alpha/transparency channel.

	Args:
	    filepath (string): Path to the file being opened.
	    transparency (bool): Flag indicating if transparency mode is enabled. Perserves alpha channel if true.

	Returns:
	    Image object: An object containing information about the opened image.
"""
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

"""
	Encodes a word pixel block. A word pixel block contains 16 bits. 
	The MSb of the first element of a word pixel block is the word flag, which is always set to 1 to indicate the data is a word.
	
	Encoded Data Format: 
	encoded_word[15] = word flag
	encoded_word[14:8] = Count bits 9 to 3
	encoded_word[7:5] = Count bits 2 to 0
	encoded_word[4:0] = gray scale intensity value

	Args:
	    gray_scale_value (int): The scaled grayscale intensity (ranging from 0 to 16).
	    repeated_val_cnt (int): The total amount of times the pixel appears in sequence (ranging from 1 to 1023)
	    inverted (bool): True if the colors of the image should be inverted. 

	Returns:
	    tupple (encoded_upper_byte, encoded_lower_byte)(int, int): A tupple containing the MSB followed by the LSB of the encoded word.
"""
def encode_word(gray_scale_value, repeated_val_cnt, inverted):
	if inverted:
		if gray_scale_value != PIXEL_INTENSITY_TRANSPARENT:
			gray_scale_value = PIXEL_INTENSITY_MAX - gray_scale_value

	r_val_count_high = (repeated_val_cnt & R_VAL_HIGH_BIT_MSK) >> 3
	r_val_count_low = repeated_val_cnt & R_VAL_LOW_BIT_MSK
	encoded_upper_byte = WORD_FLG_BIT | r_val_count_high
	encoded_lower_byte = (r_val_count_low << R_VAL_LOW_BIT_POS) | gray_scale_value
	return (encoded_upper_byte, encoded_lower_byte)

"""
	Encodes a byte pixel block. A byte pixel block contains 8 bits. 
	The MSb of a byte pixel block is the word flag, which is always set to 0 to indicate the data is a byte.
	
	Encoded Data Format:
	encoded_word[7] = word flag
	encoded_word[6:5] = Count bits 
	encoded_word[4:0] = gray scale intensity value

	Args:
	    gray_scale_value (int): The scaled grayscale intensity (ranging from 0 to 16).
	    repeated_val_cnt (int): The total amount of times the pixel appears in sequence (ranging from 1 to 3).
	    inverted (bool): True if the colors of the image should be inverted. 

	Returns:
	    int: The encoded byte.
"""
def encode_byte(gray_scale_value, repeated_val_cnt, inverted):
	if inverted:
		if gray_scale_value != PIXEL_INTENSITY_TRANSPARENT:
			gray_scale_value = PIXEL_INTENSITY_MAX - gray_scale_value

	encoded_byte = (repeated_val_cnt << R_VAL_LOW_BIT_POS) | gray_scale_value
	encoded_byte &= ~WORD_FLG_BIT
	return encoded_byte

"""
	Encodes a bitmap with a custom flavor of RLE. Each bitmap contains its total length in elements, followed by its height, width, 
	then data.	The data is broken into chunk of pixels is refered to as a pixel blocks which can either be a word in length (16 bits) 
	or byte sized (8 bits). 

	Each pixel block contains:
	- A flag to indicate if the data is a byte or word
	- The intensity of the pixel(s)
	- The amount of times pixels of the given intensity repeat
	
	See encode_byte and encode_word for more details on pixel block format.

	Args:
	    im (Image object): The image to be encoded should be preprocessed for black and white, or black and white with alphachannel before passing. 
	    transparency (bool): Flag indicating if transparency mode is enabled. Preserves alpha channel if true.
	    inverted (bool): True if the colors of the image should be inverted. 

	Returns:
	    tupple (array_str, elem_count, cols, rows)(int, int, int, int): A tupple containing array string for the header file, total element count, amount of columns, and amount of rows.
"""
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

"""
	Creates a .hpp file with encoded bitmap data for use with esp_SH1122 component. 

	Args:
	    filename (string): The name of the file to save.
	    array_data (string): The encoded array data returned from encode_bitmap
	    total_elements (int): Total amount of elements returned from encode_bitmap
	    cols (int): Column count/ width of bitmap returned from encode_bitmap
	    rows (int): Row count/ height of bitmap returned from encode_bitmap
	    transparency (bool): Flag indicating if transparency mode is enabled. 
	    inverted (bool): True if the colors of the image are inverted. 

	Returns:
	    n/a
"""
def write_file(filename, array_data, total_elements, cols, rows, transparency, inverted):
	og_filename = filename
	filename = filename[:-4]
	filename = "sh1122_bitmap_" + filename
	file_str = "#pragma once \n"
	file_str += "#include <stdint.h> \n\n"
	file_str += "/*\n"
	file_str += " Converted with sh1122_encode_bitmap.py \n Source Image Name: " + og_filename +"\n Width: " + str(cols) + "\n"
	file_str += " Height: " + str(rows) + "\n Conversion Settings: transparency= " + str(transparency) + ", transparency_threshold=" + (str(transparency_threshold) if transparency else " n/a") + ", inverted= " +str(inverted) +"\n"
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
	global transparency_threshold
	input_dir = 'input'

	for filename in os.listdir(input_dir):
		if filename.endswith('.png'):
			filepath = os.path.join(input_dir, filename)
			
			im = load_image(filepath, transparency)
			array_data, total_elements, cols, rows, = encode_bitmap(im, transparency, inverted)
			write_file(filename, array_data, total_elements, cols, rows, transparency, inverted)

if __name__ == "__main__":
	parser = argparse.ArgumentParser(prog = 'sh1122 encode bitmap', description='Encodes images with custom run-length format for 16 shade grayscale displays.')
	parser.add_argument('-T', '-t', '--transparency', action='store_true', help='Transparent pixels will not be ignored, if unused  they may appear as black or white.')
	parser.add_argument('-TT', '-tt', '--transparency_threshold', type=int, metavar='x', choices=range(256), help='Pixels with alpha channel below this level will be set to transparent (default 30).')
	parser.add_argument('-I', '-i', '--inverted', action='store_true', default = False, help = 'Invert the grayscale intensity of the generated bitmap.')

	args = parser.parse_args()

	if args.transparency_threshold is not None:
		transparency_threshold = args.transparency_threshold

	#-tt only availble if -t is used
	if args.transparency_threshold is not None and not args.transparency:
		parser.error('-tt/--transparency_threshold can only be used with -t/--transparency')

	main(args.transparency, args.inverted)


