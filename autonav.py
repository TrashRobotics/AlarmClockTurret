import cv2
import numpy as np
import socket
import json
import time


def autonav(rawImage, l=(45, 70, 70), h=(90, 255, 255), target=(320, 240), debug=False):
    """
    l - нижняя цветовая граница в HSV
    h - верхняя цветовая граница в HSV
    target - целевая точка, куда целится башня
    выход: x_dir, y_dir - направления движения башни
    """
    image = rawImage.copy()
    if debug:
        cv2.imshow("raw", image)
    height, width = image.shape[0:2]
    hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
    binary = cv2.inRange(hsv, l, h)     # выделяем области с выбранным диапазоном цветов
    if debug:
        cv2.imshow("bin", binary)
    contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL,
                                   cv2.CHAIN_APPROX_NONE)  # получаем контуры областей

    # мертвая зона целевой точки - прямоугольник, размером 20% от  изображения
    deadZoneSize = 0.20     # 20%
    dzrw = width*deadZoneSize   # deadZoneRectangleWidth
    dzrh = height*deadZoneSize  #

    deadZoneRectangle = ((int(target[0] - dzrw/2), int(target[0] + dzrw/2)),
                         (int(target[1] - dzrh/2), int(target[1] + dzrh/2)))

    cx, cy = target
    if len(contours) != 0:
        maxc = max(contours, key=cv2.contourArea)   # находим наибольший контур
        moments = cv2.moments(maxc)  # получаем моменты этого контура
        if moments["m00"] != 0:
            cx = int(moments["m10"] / moments["m00"])  # координата центра контура по x
            cy = int(moments["m01"] / moments["m00"])  # координата центра по y

        if debug:
            cv2.drawContours(image, maxc, -1, (120, 0, 0), 2)   # рисуем контур

    if debug:
        cv2.rectangle(image, (deadZoneRectangle[0][0], deadZoneRectangle[1][0]),
                      (deadZoneRectangle[0][1], deadZoneRectangle[1][1]), (0, 0, 200), 2)
        cv2.line(image, (width//2, 0), (width//2, height), (0, 0, 200), 1)
        cv2.line(image, (0, height//2), (width, height//2), (0, 0, 200), 1)
        cv2.line(image, (cx, 0), (cx, height), (0, 255, 0), 2)  # рисуем линию линию по x
        cv2.line(image, (0, cy), (width, cy), (0, 255, 0), 2)  # линия по y

    x_dir, y_dir = 0, 0
    if cx > target[0]:      # двигаемся в сторону целевой точки
        x_dir = 1
    elif cx < target[0]:
        x_dir = -1
    if deadZoneRectangle[0][0] < cx < deadZoneRectangle[0][1]:  # если попали в мертвую зону
        x_dir = 0

    if cy > target[1]:
        y_dir = -1
    elif cy < target[1]:
        y_dir = 1
    if deadZoneRectangle[1][0] < cy < deadZoneRectangle[1][1]:  # если попали в мертвую зону
        y_dir = 0

    if debug:
        cv2.putText(image, 'HSV color range:', (10, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.75, (0, 0, 0), 1, cv2.LINE_AA)
        lcolor = (cv2.cvtColor(np.uint8([[l]]), cv2.COLOR_HSV2BGR)[0][0]).tolist()
        hcolor = (cv2.cvtColor(np.uint8([[h]]), cv2.COLOR_HSV2BGR)[0][0]).tolist()
        cv2.rectangle(image, (10, 30), (30, 50), lcolor, -1)
        cv2.rectangle(image, (40, 30), (60, 50), hcolor, -1)
        cv2.putText(image, 'x_dir: {x}'.format(x=x_dir), (width - 250, height - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.75, (0, 0, 255), 2, cv2.LINE_AA)
        cv2.putText(image, 'y_dir: {y}'.format(y=y_dir), (width - 125, height - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2, cv2.LINE_AA)

    if debug:
        cv2.imshow("end", image)

    return x_dir, y_dir


if __name__ == '__main__':
    msg = {
        "x_dir": 0,
        "y_dir": 0,
        "shot": 0
    }

    host = ("192.168.1.1", 5000)
    sendFreq = 30  # слать 30 пакетов в секунду

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    cap = cv2.VideoCapture(1)
    if not cap.isOpened():
        print("Cannot open camera")
        exit()

    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                break

            msg["x_dir"], msg["y_dir"] = autonav(frame, l=(45, 70, 70), h=(90, 255, 255), debug=True)
            print(msg["x_dir"], msg["y_dir"])

            sock.sendto(json.dumps(msg, ensure_ascii=False).encode("utf8"), host)   # отправляем сообщение в виде json файла
            cv2.waitKey(int((1 / sendFreq) * 1000))
            #time.sleep(1 / sendFreq)
    except KeyboardInterrupt:
        sock.close()
        cap.release()
        cv2.destroyAllWindows()
