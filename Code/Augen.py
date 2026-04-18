import cv2

import mediapipe as mp

import pygame

import sys

 

# Mediapipe Face Detection

mp_face = mp.solutions.face_detection.FaceDetection(model_selection=0, min_detection_confidence=0.5)

 

# Kamera starten

cap = cv2.VideoCapture(0)

frame_width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))

frame_height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))

 

# Pygame Setup

pygame.init()

screen = pygame.display.set_mode((800, 480))

pygame.display.set_caption("Animierte Augen")

 

eye_radius = 80

pupil_radius = 20

left_eye_center = (250, 240)

right_eye_center = (550, 240)

 

def map_range(value, in_min, in_max, out_min, out_max):

    return out_min + (out_max - out_min) * ((value - in_min) / (in_max - in_min))

 

while True:

    for event in pygame.event.get():

        if event.type == pygame.QUIT:

            cap.release()

            pygame.quit()

            sys.exit()

 

    ret, frame = cap.read()

    if not ret:

        break

 

    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

    results = mp_face.process(rgb)

 

    pupil_offset_x = 0

    pupil_offset_y = 0

 

    if results.detections:

        detection = results.detections[0]

        bbox = detection.location_data.relative_bounding_box

        face_center_x = int(bbox.xmin * frame_width + bbox.width * frame_width / 2)

        face_center_y = int(bbox.ymin * frame_height + bbox.height * frame_height / 2)

 

        pupil_offset_x = map_range(face_center_x, 0, frame_width, -30, 30)

        pupil_offset_y = map_range(face_center_y, 0, frame_height, -30, 30)

 

    # Augen zeichnen

    screen.fill((0, 0, 0))

    pygame.draw.circle(screen, (255, 255, 255), left_eye_center, eye_radius)

    pygame.draw.circle(screen, (255, 255, 255), right_eye_center, eye_radius)

 

    pygame.draw.circle(screen, (0, 0, 0), (left_eye_center[0] + int(pupil_offset_x), left_eye_center[1] + int(pupil_offset_y)), pupil_radius)

    pygame.draw.circle(screen, (0, 0, 0), (right_eye_center[0] + int(pupil_offset_x), right_eye_center[1] + int(pupil_offset_y)), pupil_radius)

 

    pygame.display.flip()