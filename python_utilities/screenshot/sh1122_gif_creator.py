"""
Creates an animated gif from any images stored within the output directory, created by sh1122_screenshot.py
Duration for each frame is pulled directly from the timestamps in the filenames. 
"""
from PIL import Image
import os

input_directory = "output/"

#get all files from output of sh1122_screenshot.py
file_names = os.listdir(input_directory)

frames = [] #images
durations = [] #display duration

#sort files according to shot number
file_names = sorted(file_names, key=lambda name: int(name.split('_')[1]))

if len(file_names) > 2:
    for i in range(len(file_names) - 1):
        file_name_current = file_names[i]
        file_name_next = file_names[i + 1]
        timestamp_current = int(file_name_current.split('_')[-2]) 
        timestamp_next = int(file_name_next.split('_')[-2])

        duration = timestamp_next - timestamp_current
        img = Image.open(os.path.join(input_directory, file_name_current))
        durations.append(duration)
        frames.append(img)

    frames.append(Image.open(os.path.join(input_directory, file_names[-1])))
    durations.append(durations[-1])
    output_directory = "gif_output/"
    os.makedirs(output_directory, exist_ok=True)  #create gif_output directory if it does not exist
    frames[0].save(os.path.join(output_directory, "screen_recording.gif"),
              save_all=True, append_images=frames[1:], duration=durations, loop=0)

    print(f"Animated GIF '{gif_file_name}' created successfully and saved to '{output_directory}'.")

