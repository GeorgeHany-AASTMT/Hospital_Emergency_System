import subprocess
import threading
import time
import csv
import os

class EngineManager:
    def __init__(self, executable_path="engine.exe"):
        self.executable_path = executable_path
        self.process = None
        self.lock = threading.Lock()
        self.start_engine()

    def start_engine(self):
        print(f"Starting {self.executable_path}...")
        self.process = subprocess.Popen(
            [self.executable_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
            cwd=os.path.dirname(os.path.abspath(__file__))
        )
        
        # Thread to consume stdout so the pipe doesn't block
        def consume_stdout():
            try:
                for line in self.process.stdout:
                    # Optional: uncomment to see C++ logs in Flask terminal
                    # print(f"[C++] {line}", end='')
                    pass
            except Exception:
                pass
                
        self.stdout_thread = threading.Thread(target=consume_stdout, daemon=True)
        self.stdout_thread.start()
        
        # Initial wait and login
        time.sleep(0.5)
        self.send_input("admin\n")
        time.sleep(0.5)

    def send_input(self, text):
        with self.lock:
            if self.process and self.process.poll() is None:
                try:
                    self.process.stdin.write(text)
                    self.process.stdin.flush()
                except Exception as e:
                    print(f"Error writing to C++ engine: {e}")

    def stop_engine(self):
        if self.process:
            self.send_input("0\n") # try graceful logout
            time.sleep(0.5)
            self.process.kill()

engine = EngineManager()

def read_csv_safe(filename, fieldnames=None):
    if not os.path.exists(filename):
        return []
    try:
        with open(filename, 'r', newline='') as f:
            if fieldnames:
                reader = csv.DictReader(f, fieldnames=fieldnames)
            else:
                reader = csv.DictReader(f)
            return list(reader)
    except Exception as e:
        print(f"Error reading {filename}: {e}")
        return []

def get_patients_data():
    return read_csv_safe("patients.csv")

def get_doctors_data():
    return read_csv_safe("doctors.csv")

def get_history_data():
    history_fields = ["ID", "Name", "Age", "Phone", "SSN", "Priority", "Symptoms", "Specialty", "Duration", "Day", "TreatedBy"]
    return read_csv_safe("history.csv", fieldnames=history_fields)
