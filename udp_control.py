import socket
import json
from pynput import keyboard     # нужно установить для управления с клавиатуры
import time

msg = {
    "x_dir": 0,
    "y_dir": 0,
    "shot": 0
}

host = ("192.168.1.1", 5000)
sendFreq = 15       # слать 15 пакетов в секунду


def onPress(key):
    """ вызывается при нажатии какой либо клавиши на клавиатуре """
    global msg
    if key == keyboard.Key.up:      # управление стрелочками
        msg["y_dir"] = 1
    elif key == keyboard.Key.down:
        msg["y_dir"] = -1
    elif key == keyboard.Key.left:
        msg["x_dir"] = -1
    elif key == keyboard.Key.right:
        msg["x_dir"] = 1
    elif key == keyboard.Key.space:
        msg["shot"] = 1


def onRelease(key):
    """ вызывается при отпускании какой либо клавиши на клавиатуре """
    global msg
    if (key == keyboard.Key.up) or (key == keyboard.Key.down):      # управление стрелочками
        msg["y_dir"] = 0
    elif (key == keyboard.Key.left) or (key == keyboard.Key.right):      # управление стрелочками
        msg["x_dir"] = 0
    elif key == keyboard.Key.space:
        msg["shot"] = 0


if __name__ == '__main__':
    listener = keyboard.Listener(
        on_press=onPress,
        on_release=onRelease)
    listener.start()    # запускаем обработчик нажатия клавиш в неблокирующем режиме

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    try:
        while True:
            sock.sendto(json.dumps(msg, ensure_ascii=False).encode("utf8"), host)   # отправляем сообщение в виде json файла
            time.sleep(1 / sendFreq)
    except KeyboardInterrupt:
        sock.close()
        listener.stop()
