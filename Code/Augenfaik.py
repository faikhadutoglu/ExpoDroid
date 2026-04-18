import cv2
import pygame
import sys
import time
import random
import threading

# ==============================================================
# DUAL·E Robot – Augen-Animation
# Display: HMTECH 10.1" Touchscreen 1024x600 IPS
# Smooth Face Tracking, optimiert für Raspberry Pi 5 + parallele KI
# ==============================================================

# ---------- Design (passend zur Frontplatte) ----------
BG_COLOR       = (245, 245, 245)   # weißes Panel
LEFT_EYE_COL   = (200, 35, 45)     # rotes Quadrat
RIGHT_EYE_COL  = (60, 65, 75)      # Anthrazit
PUPIL_COL      = (20, 20, 20)
HIGHLIGHT_COL  = (255, 255, 255)

# ---------- Screen (1024x600) ----------
SCREEN_W, SCREEN_H = 1024, 600
FULLSCREEN         = True          # Für den fertigen Roboter: True
#                                    Zum Testen am PC:         False

# ---- Augen füllen das Display fast komplett ----
EYE_SIZE        = 400              # passt in 600 px Höhe mit ~60 px Rand
EYE_GAP         = 60               # Lücke zwischen den Augen
EYE_CENTER_Y    = SCREEN_H // 2
EYE_CENTER_OFF  = (EYE_SIZE + EYE_GAP) // 2
LEFT_EYE        = (SCREEN_W // 2 - EYE_CENTER_OFF, EYE_CENTER_Y)
RIGHT_EYE       = (SCREEN_W // 2 + EYE_CENTER_OFF, EYE_CENTER_Y)

PUPIL_SIZE      = 150              # proportional kräftig
MAX_OFF_X       = 120              # max. horizontale Pupillenbewegung
MAX_OFF_Y       = 80               # max. vertikale Pupillenbewegung

EYE_RADIUS      = 42               # Eckenrundung der Augenquadrate
PUPIL_RADIUS    = 24               # Eckenrundung der Pupillen

# ---------- Empfindlichkeit (Gain) ----------
GAIN_X     = 2.2
GAIN_Y     = 2.0
DEADZONE   = 0.05

# ---------- Verhalten ----------
SMOOTH_FACTOR   = 0.22
RECENTER_SMOOTH = 0.05
LOST_TIMEOUT    = 0.8
BLINK_MIN_S     = 3.5
BLINK_MAX_S     = 7.0
BLINK_DUR       = 0.14

# ---------- Kamera / Performance ----------
CAM_W, CAM_H   = 320, 240
DETECT_EVERY_N = 2
DETECTOR_SCALE = 1.2
MIN_NEIGHBORS  = 4
MIN_FACE_SIZE  = (50, 50)
FPS            = 30

# ---------- Spiegelung ----------
MIRROR_CAMERA  = True
DEBUG          = False


# ----------------------------------------------------------
# Kamera-Thread
# ----------------------------------------------------------
class CameraThread(threading.Thread):
    def __init__(self, src=0):
        super().__init__(daemon=True)
        self.cap = cv2.VideoCapture(src)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH,  CAM_W)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, CAM_H)
        self.cap.set(cv2.CAP_PROP_BUFFERSIZE,   1)
        self.frame = None
        self.lock = threading.Lock()
        self.running = True

    def run(self):
        while self.running:
            ok, frame = self.cap.read()
            if not ok:
                time.sleep(0.01)
                continue
            if MIRROR_CAMERA:
                frame = cv2.flip(frame, 1)
            with self.lock:
                self.frame = frame

    def read(self):
        with self.lock:
            return None if self.frame is None else self.frame

    def stop(self):
        self.running = False
        try:
            self.cap.release()
        except Exception:
            pass


def apply_deadzone_and_gain(value, gain, deadzone):
    """Totzone um 0, dann linear verstärken, final auf -1..1 clampen."""
    if abs(value) < deadzone:
        return 0.0
    sign = 1.0 if value > 0 else -1.0
    scaled = (abs(value) - deadzone) / (1.0 - deadzone)
    v = sign * scaled * gain
    return max(-1.0, min(1.0, v))


def draw_eye(surface, center, size, color, pupil_off, pupil_size, blink_t):
    cx, cy = center
    half = size // 2
    # Auge (farbiges Quadrat)
    pygame.draw.rect(surface, color,
                     (cx - half, cy - half, size, size),
                     border_radius=EYE_RADIUS)

    # Blink -> oberen + unteren Rand mit Hintergrund überdecken
    if blink_t < 1.0:
        cover = int(half * (1 - blink_t))
        if cover > 0:
            pygame.draw.rect(surface, BG_COLOR,
                             (cx - half, cy - half, size, cover))
            pygame.draw.rect(surface, BG_COLOR,
                             (cx - half, cy + half - cover, size, cover))

    # Pupille nur zeichnen, wenn Auge weit genug offen
    if blink_t > 0.25:
        px = cx + pupil_off[0]
        py = cy + pupil_off[1]
        pygame.draw.rect(surface, PUPIL_COL,
                         (px - pupil_size // 2, py - pupil_size // 2,
                          pupil_size, pupil_size),
                         border_radius=PUPIL_RADIUS)
        # Glanzpunkt (proportional zur Pupille)
        hl = pupil_size // 5
        pygame.draw.rect(surface, HIGHLIGHT_COL,
                         (px - pupil_size // 2 + pupil_size // 10,
                          py - pupil_size // 2 + pupil_size // 10,
                          hl, hl),
                         border_radius=max(3, hl // 3))


def main():
    pygame.init()
    pygame.mouse.set_visible(False)    # Mauszeiger ausblenden

    flags = pygame.FULLSCREEN if FULLSCREEN else 0
    screen = pygame.display.set_mode((SCREEN_W, SCREEN_H), flags)
    pygame.display.set_caption("DUAL·E Eyes")
    clock = pygame.time.Clock()

    cascade_path = cv2.data.haarcascades + 'haarcascade_frontalface_default.xml'
    face_cascade = cv2.CascadeClassifier(cascade_path)

    cam = CameraThread(0)
    cam.start()

    target_off      = [0.0, 0.0]
    current_off     = [0.0, 0.0]
    last_face_rect  = None
    last_frame_size = (CAM_W, CAM_H)
    last_face_time  = 0.0
    frame_counter   = 0

    next_blink  = time.time() + random.uniform(BLINK_MIN_S, BLINK_MAX_S)
    blink_start = None

    try:
        while True:
            for event in pygame.event.get():
                if event.type == pygame.QUIT or (
                    event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE
                ):
                    raise KeyboardInterrupt

            now = time.time()
            frame = cam.read()

            # ---- Detection ----
            if frame is not None and frame_counter % DETECT_EVERY_N == 0:
                fh, fw = frame.shape[:2]
                gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                faces = face_cascade.detectMultiScale(
                    gray,
                    scaleFactor=DETECTOR_SCALE,
                    minNeighbors=MIN_NEIGHBORS,
                    minSize=MIN_FACE_SIZE,
                )
                if len(faces) > 0:
                    last_face_rect  = max(faces, key=lambda r: r[2] * r[3])
                    last_frame_size = (fw, fh)
                    last_face_time  = now
                elif now - last_face_time > LOST_TIMEOUT:
                    last_face_rect = None
            frame_counter += 1

            # ---- Zielposition ----
            if last_face_rect is not None:
                x, y, w, h = last_face_rect
                fw, fh = last_frame_size
                nx = ((x + w / 2) / fw) * 2 - 1
                ny = ((y + h / 2) / fh) * 2 - 1
                nx = apply_deadzone_and_gain(nx, GAIN_X, DEADZONE)
                ny = apply_deadzone_and_gain(ny, GAIN_Y, DEADZONE)
                target_off[0] = nx * MAX_OFF_X
                target_off[1] = ny * MAX_OFF_Y
                smooth = SMOOTH_FACTOR

                if DEBUG:
                    print(f"face x={x} y={y} w={w} h={h} | "
                          f"frame {fw}x{fh} | nx={nx:+.2f} ny={ny:+.2f}")
            else:
                target_off[0] = 0.0
                target_off[1] = 0.0
                smooth = RECENTER_SMOOTH

            # ---- Smoothing ----
            current_off[0] += (target_off[0] - current_off[0]) * smooth
            current_off[1] += (target_off[1] - current_off[1]) * smooth

            # ---- Blinzeln ----
            blink_t = 1.0
            if blink_start is None and now >= next_blink:
                blink_start = now
            if blink_start is not None:
                dt = now - blink_start
                if dt < BLINK_DUR / 2:
                    blink_t = 1 - (dt / (BLINK_DUR / 2))
                elif dt < BLINK_DUR:
                    blink_t = (dt - BLINK_DUR / 2) / (BLINK_DUR / 2)
                else:
                    blink_start = None
                    next_blink = now + random.uniform(BLINK_MIN_S, BLINK_MAX_S)

            # ---- Render ----
            screen.fill(BG_COLOR)
            off = (int(current_off[0]), int(current_off[1]))
            draw_eye(screen, LEFT_EYE,  EYE_SIZE, LEFT_EYE_COL,  off, PUPIL_SIZE, blink_t)
            draw_eye(screen, RIGHT_EYE, EYE_SIZE, RIGHT_EYE_COL, off, PUPIL_SIZE, blink_t)
            pygame.display.flip()
            clock.tick(FPS)

    except KeyboardInterrupt:
        pass
    finally:
        cam.stop()
        pygame.quit()
        sys.exit()


if __name__ == "__main__":
    main()