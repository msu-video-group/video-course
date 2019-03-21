import cv2
import numpy as np
import os


class ReadImageError(ValueError):
    pass


CV_COLOR_FLAGS = {
    'rgb': cv2.COLOR_BGR2RGB,
    'gray': cv2.COLOR_BGR2GRAY
}


def read_sequence(folder, color_map='rgb'):
    cv_color_flag = CV_COLOR_FLAGS[color_map]
    file_names = os.listdir(folder)
    
    def _read_image(file_name):
        file_path = os.path.abspath(os.path.join(folder, file_name))
        image_bgr = cv2.imread(file_path)
        if image_bgr is None:
            message = 'Failed while reading image from `{}`'.format(file_path)
            raise ReadImageError(message)
            
        image = cv2.cvtColor(image_bgr, cv_color_flag)
        return image
    
    sequence = np.array([
        _read_image(file_name) for file_name in sorted(file_names)])
        
    return sequence


def extract_fields(interlaced, top_field_first=True):
    num_frames, height, width, num_channels = interlaced.shape
    
    num_fields = num_frames * 2
    field_height = height // 2
    
    fields = np.zeros((num_frames * 2, field_height, width, num_channels), dtype='uint8')
    
    if top_field_first:
        first, second = 0, 1
    else:
        first, second = 1, 0
    
    fields[first::2] = interlaced[:, : field_height * 2: 2]
    fields[second::2] = interlaced[:, 1: field_height * 2: 2]
    
    return fields


def save_image_png(image, path):
    cv2.imwrite(path, image, [cv2.IMWRITE_PNG_COMPRESSION, 0.0])
    

def save_image_jpg(image, path):
    cv2.imwrite(path, image, [cv2.IMWRITE_JPEG_QUALITY, 95])


def rgb2gray(image):
    return cv2.cvtColor(image, cv2.COLOR_RGB2GRAY)
