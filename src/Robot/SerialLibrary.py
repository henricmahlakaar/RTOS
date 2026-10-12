import serial

ser = None

def open_port(port):
    global ser
    ser = serial.Serial(port, 115200, timeout=2)

def send_text(text):
    ser.write((text + "\r\n").encode())

def read_line():
    while True:
        line = ser.readline().decode(errors="ignore").strip()

        if line in ["80", "-1", "-3", "86399"]:
            return line

def close_port():
    global ser
    if ser:
        ser.close()
        ser = None