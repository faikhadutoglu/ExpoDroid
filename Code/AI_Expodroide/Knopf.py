from gpiozero import Button
from signal import pause

button = Button(17, pull_up=True)

button.when_pressed = lambda: print("Knopf gedrückt")
button.when_released = lambda: print("Knopf losgelassen")

pause()
