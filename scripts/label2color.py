#!/usr/bin/python

background = 255 
label_to_color = {                                                                                                              
    2: (128, 64,128), # road
    3: (128, 64,128), # sidewalk
    5: ( 192, 192, 192), # building
    10: (192,192,192), # pole
    12: (255,255,0 ), # traffic sign
    6: (107,142, 35), # vegetation
    4: (107,142, 35), # terrain
    13: ( 135, 206, 236 ),  # sky
    1: (  30, 144, 255 ),  # water
    8 :(220, 20, 60), # person
    7: ( 0, 0,142),  # car
    9 : (119, 11, 32),# bike
    11 : (123, 104, 238), # stair

    0: (0 ,0, 0),       # background
    background: (0,0,0) # background
} 
