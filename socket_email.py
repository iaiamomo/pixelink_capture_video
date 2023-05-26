from multiprocessing import Process
import socket
import json
from flask import Flask
from flask_mail import Mail, Message
from datetime import datetime
import sys, subprocess

PATH = 'captureVideo.exe'
HOST = socket.gethostbyname(socket.gethostname())
PORT = 2345

app = Flask(__name__)
mail = Mail(app)

app.config['MAIL_SERVER'] = "smtp.gmail.com"
app.config['MAIL_PORT'] = 465
app.config['MAIL_USERNAME'] = "email@gmail.com"
app.config['MAIL_PASSWORD'] = "password"
app.config['MAIL_USE_TLS'] = False
app.config['MAIL_USE_SSL'] = True
mail = Mail(app)

sender_mail = "email@gmail.com"
receiver_mail = ["receiver1@gmail.com"]

def get_request(host = HOST, port = PORT):
    print(f"Start script, host:{HOST}, port:{PORT}")

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind((host, port))
    s.listen()
    conn, addr = s.accept()
    print(datetime.now(), 'Connected to ruuviapp by', addr)

    capt_proc = {}

    while True:
        try:
            data = conn.recv(1024)
        except:
            if len(capt_proc) != 0:
                for elem in capt_proc.keys():
                    capt_proc[elem].terminate()
                capt_proc = {}
            conn = reconnect(s, host, port)
            continue

        if not data:
            if len(capt_proc) != 0:
                for elem in capt_proc.keys():
                    capt_proc[elem].terminate()
                capt_proc = {}
            conn = reconnect(s, host, port)
            continue

        json_obj = json.loads(data.decode("utf-8"))
        fustella_id = json_obj["diecutter_id"]
        session_id = json_obj["session_id"]
        in_session = json_obj["in_session"]

        print(datetime.now(), "fustella_id:", fustella_id, "session_id:", str(session_id), "in_session:", str(in_session))

        with app.app_context():
            send_email(fustella_id, session_id, in_session)
            print(datetime.now(), "Email sent")

        # start thread that run a script
        if in_session:
            subprocess.call(PATH, shell=True)
            print(datetime.now(), "First video captured")
            thread = Process(target = run_script, args = ())
            thread.start()
            capt_proc[fustella_id] = thread
        else:
            thread = capt_proc[fustella_id]
            thread.terminate()
            capt_proc.pop(fustella_id)


def run_script():
    i = 1
    t1 = datetime.now()
    while True:
        t2 = datetime.now()
        diff = t2 - t1
        if diff.seconds > 300:
            subprocess.call(PATH, shell=True)
            print(f"{datetime.now()} Video {i} captured")
            i+=1
            t1 = datetime.now()
        

# inivio mail
def send_email(fustella_id, session_id, in_session):
    contenuto = "fustella_id: " + str(fustella_id) + ", session_id: " + str(session_id) + ", in_session: " + str(in_session)
    oggetto = "FUSTELLA in_session " + str(in_session)
    msg = Message(oggetto, sender = sender_mail, recipients = receiver_mail)
    msg.body = contenuto
    mail.send(msg)

def reconnect(old_socket, host, port):
    print(datetime.now(), "Reconnecting...")
    while True:
        old_socket.close()

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind((host, port))
        s.listen()
        conn, addr = s.accept()
        print(datetime.now(), 'Reconnected to ruuviapp by', addr)

        return conn

if __name__ == "__main__":
    # get input from command line
    if len(sys.argv) == 2:
        port = int(sys.argv[1])
        get_request (port = port)
    elif len(sys.argv) == 3:
        host = str(sys.argv[1])
        port = int(sys.argv[2])
        get_request(host = host, port = port)
    else:
        get_request()