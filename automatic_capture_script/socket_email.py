import smtplib
from flask import Flask
from flask_mail import Mail, Message
from datetime import datetime, timedelta
import socket
import json
import subprocess
import multiprocessing
import logging, logging.handlers
import time

HOST = socket.gethostbyname(socket.gethostname())
PORT = 2345

app = Flask(__name__)
mail = Mail(app)

app.config['MAIL_SERVER'] = "smtp.gmail.com"
app.config['MAIL_PORT'] = 465
app.config['MAIL_USERNAME'] = "rotalaser.software@gmail.com"
app.config['MAIL_PASSWORD'] = "nmleyjwofltpgdik"
app.config['MAIL_USE_TLS'] = False
app.config['MAIL_USE_SSL'] = True
mail = Mail(app)

sender_mail = "rotalaser.software@gmail.com"
receiver_mail = ["monti.1632488@studenti.uniroma1.it", "mathew@diag.uniroma1.it", "monitorruuvi@gmail.com", "gabriele1.desantis@gmail.com"]

# inivio mail
def send_email(fustella_id, session_id, in_session):
    
    contenuto = "fustella_id: " + str(fustella_id) + ", session_id: " + str(session_id) + ", in_session: " + str(in_session)
    oggetto = "FUSTELLA in_session " + str(in_session)

    msg = Message(oggetto, sender = sender_mail, recipients = receiver_mail)
    msg.body = contenuto
    mail.send(msg)

def reconnect(old_socket):
    print("Reconnecting...")
    while True:
        old_socket.close()

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind((HOST, PORT))
        s.listen()
        conn, addr = s.accept()
        print('Reconnected by', addr)

        return conn

def capture_video():
    i = 0
    current_time = datetime.now()
    while True:
        t = datetime.now()
        diff = t-current_time
        if diff > timedelta(seconds=900): #1800sec = 30min
            subprocess.call('start C:/Users/ROTALASER/Desktop/captureVideo/x64/Debug/captureVideo.exe', shell=True)
            print("Video",str(i),"captured")
            
            i+=1
            current_time = datetime.now()


def main_mail():
    logger = logging.getLogger()
    h = logging.handlers.RotatingFileHandler("email.log", 'a', 10*1024*1024, 3)
    f = logging.Formatter('%(asctime)s %(processName)-10s %(name)s %(levelname)-8s %(message)s')
    h.setFormatter(f)
    logger.addHandler(h)
    logger.setLevel(logging.INFO)

    print("Waiting to connect...")
    logger.log(logging.INFO, "Waiting to connect...")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind((HOST, PORT))
    s.listen()
    conn, addr = s.accept()
    print('Connected by', addr)
    logger.log(logging.INFO, "Connected by"+str(addr))

    capt_proc = None
    capt_proc = {}

    while True:
        try:

            try:
                data = conn.recv(1024)
            except:
                if len(capt_proc) != 0:
                    for elem in capt_proc.keys():
                        capt_proc[elem].terminate()
                    capt_proc = {}

                conn = reconnect(s)
                logger.log(logging.INFO, "reconnected")
                continue

            if not data:
                if len(capt_proc) != 0:
                    for elem in capt_proc.keys():
                        capt_proc[elem].terminate()
                    capt_proc = {}

                conn = reconnect(s)
                logger.log(logging.INFO, "reconnected")
                continue

            print(data)
            json_obj = json.loads(data.decode("utf-8"))
            logger.log(logging.INFO, str(json_obj))
            fustella_id = json_obj["diecutter_id"]
            session_id = json_obj["session_id"]
            in_session = json_obj["in_session"]

            if in_session:
                print("Invio email",str(fustella_id),"in movimento")
                logger.log(logging.INFO, "invio email "+str(fustella_id)+" in movimento")
                send_email(fustella_id, session_id, in_session)

                subprocess.call('start C:/Users/ROTALASER/Desktop/captureVideo/x64/Debug/captureVideo.exe', shell=True)
                print("First video captured")
                logger.log(logging.INFO, "Start capturing videos")

                capt = multiprocessing.Process(target=capture_video, args=())
                capt.start()
                capt_proc[fustella_id] = capt
            else:
                print("Invio email",str(fustella_id),"ferma")
                logger.log(logging.INFO, "invio email "+str(fustella_id)+" ferma")
                send_email(fustella_id, session_id, in_session)

                if fustella_id in capt_proc:
                    capt = capt_proc[fustella_id]
                    capt.terminate()
                    capt_proc.pop(fustella_id)
        except Exception as e:
            logger.exception(e)
            raise e

if __name__ == "__main__":
    main_mail()
