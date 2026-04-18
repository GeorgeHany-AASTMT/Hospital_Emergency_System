from flask import Flask, render_template, request, jsonify, redirect, url_for, session
from engine_bridge import engine, get_patients_data, get_doctors_data, get_history_data
import os
import time

app = Flask(__name__)
app.secret_key = 'super_secret_hospital_key'

# --- HELPERS ---
def date_to_int(date_str):
    # Convert "YYYY-MM-DD" to YYYYMMDD integer
    try:
        return int(date_str.replace("-", ""))
    except:
        return 0

def int_to_date(date_int):
    # Convert YYYYMMDD integer to "YYYY-MM-DD" string
    s = str(date_int)
    if len(s) == 8:
        return f"{s[:4]}-{s[4:6]}-{s[6:]}"
    return s

# --- VIEWS ---

@app.route('/')
def index():
    if not session.get('logged_in'):
        return redirect(url_for('login'))
    return redirect(url_for('add_patient'))

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        password = request.form.get('password')
        if password == 'drtamer':
            session['logged_in'] = True
            return redirect(url_for('add_patient'))
        else:
            return render_template('login.html', error="Invalid Password")
    return render_template('login.html')

@app.route('/logout')
def logout():
    session.pop('logged_in', None)
    return redirect(url_for('login'))

@app.route('/add_patient')
def add_patient():
    if not session.get('logged_in'): return redirect(url_for('login'))
    doctors = get_doctors_data()
    # Extract unique specialties and sort them. Always include "General" if not present.
    specialties = set(d.get('Specialty', '').strip().title() for d in doctors if d.get('Specialty'))
    specialties.add("General")
    sorted_specs = sorted(list(specialties))
    return render_template('add_patient.html', specialties=sorted_specs)

@app.route('/patients')
def patients():
    if not session.get('logged_in'): return redirect(url_for('login'))
    return render_template('patients.html')

@app.route('/doctors')
def doctors():
    if not session.get('logged_in'): return redirect(url_for('login'))
    return render_template('doctors.html')

# --- APIs ---

@app.route('/api/stats')
def api_stats():
    # Gather KPI stats: Finished Treatments, Waiting in queues, Scheduled
    patients = get_patients_data()
    history = get_history_data()
    
    # Calculate stats based on day and priority
    # A patient is in active queue if day <= today, scheduled if day > today
    # But C++ system uses current day int
    import datetime
    now = datetime.datetime.now()
    today_int = (now.year) * 10000 + (now.month) * 100 + now.day

    waiting = 0
    scheduled = 0
    for p in patients:
        # Check if currently not in treatment. The CSV outputs treatedBy if in treatment
        # But wait, C++ might output treatedBy="None" or empty if waiting.
        # Actually p['TreatedBy'] == 'None' means waiting.
        is_scheduled = int(p.get('Day', 0)) > today_int
        
        if is_scheduled:
            scheduled += 1
        elif p.get('TreatedBy', '') == 'None':
            waiting += 1
            
    finished = len(history)

    return jsonify({
        "finished": finished,
        "waiting": waiting,
        "scheduled": scheduled
    })

@app.route('/api/queues')
def api_queues():
    patients = get_patients_data()
    import datetime
    now = datetime.datetime.now()
    today_int = (now.year) * 10000 + (now.month) * 100 + now.day

    currently_treating = []
    critical_queue = []
    normal_queue = []
    future_appointments = []

    for p in patients:
        is_scheduled = int(p.get('Day', 0)) > today_int
        prio = int(p.get('Priority', 1))
        
        if p.get('TreatedBy', '') != 'None':
            # Row index 12 corresponds to TreatmentStartTime (the 13th field)
            # However, get_patients_data() uses DictReader. 
            # If the CSV header is updated in C++, DictReader will pick it up if it reads the first line.
            # Otherwise we need to handle it.
            currently_treating.append({
                "name": p.get("Name"),
                "specialty": p.get("Specialty"),
                "priority": p.get("Priority"),
                "doctor": p.get("TreatedBy"),
                "duration": int(p.get("Duration", 0)),
                "startTime": int(p.get("TreatmentStartTime", 0))
            })
        elif is_scheduled:
            future_appointments.append({
                "id": p.get("ID"),
                "name": p.get("Name"),
                "specialty": p.get("Specialty"),
                "criticality": p.get("Priority"),
                "date": int_to_date(int(p.get("Day", 0)))
            })
        else:
            obj = {
                "name": p.get("Name"),
                "specialty": p.get("Specialty"),
                "waiting": "Waiting..."
            }
            if prio >= 3:
                critical_queue.append(obj)
            else:
                normal_queue.append(obj)

    return jsonify({
        "currently_treating": currently_treating,
        "critical_queue": critical_queue,
        "normal_queue": normal_queue,
        "future_appointments": future_appointments
    })

@app.route('/api/doctors_state')
def api_doctors_state():
    return jsonify(get_doctors_data())

@app.route('/api/doctor_history/<doctor_name>')
def api_doctor_history(doctor_name):
    history = get_history_data()
    doc_history = [p for p in history if p.get("TreatedBy") == doctor_name]
    highest_time = 0
    if doc_history:
        highest_time = max([int(p.get("Duration", 0)) for p in doc_history])
        
    return jsonify({
        "total": len(doc_history),
        "highest": highest_time,
        "patients": [{
            "name": p.get("Name"),
            "id": p.get("ID"),
            "time": p.get("Duration")
        } for p in doc_history]
    })

@app.route('/api/add_patient', methods=['POST'])
def api_add_patient():
    data = request.json
    engine.send_input("1\n") # Patient Menu
    time.sleep(0.1)
    engine.send_input("1\n") # Add Patient
    time.sleep(0.1)
    engine.send_input(f"{data['name']}\n")
    engine.send_input(f"{data['age']}\n")
    engine.send_input(f"{data['phone']}\n")
    engine.send_input(f"{data['ssn']}\n")
    engine.send_input(f"{data['specialty']}\n")
    engine.send_input(f"{data['priority']}\n")
    engine.send_input(f"{date_to_int(data['day'])}\n")
    engine.send_input(f"{data['duration']}\n")
    engine.send_input(f"{data['symptoms']}\n")
    time.sleep(0.1)
    engine.send_input("0\n") # Back to Main Menu
    return jsonify({"status": "success"})

@app.route('/api/remove_patient', methods=['POST'])
def api_remove_patient():
    data = request.json
    engine.send_input("1\n") # Patient Menu
    time.sleep(0.1)
    engine.send_input("3\n") # Remove Patient
    time.sleep(0.1)
    engine.send_input(f"{data['id']}\n")
    time.sleep(0.1)
    engine.send_input("0\n") # Back to Main Menu
    return jsonify({"status": "success"})

@app.route('/api/change_patient_date', methods=['POST'])
def api_change_patient_date():
    data = request.json
    engine.send_input("1\n") # Patient Menu
    time.sleep(0.1)
    engine.send_input("4\n") # Change Day
    time.sleep(0.1)
    engine.send_input(f"{data['id']}\n")
    time.sleep(0.1)
    engine.send_input(f"{date_to_int(data['new_date'])}\n")
    time.sleep(0.1)
    engine.send_input("0\n") # Back to Main Menu
    return jsonify({"status": "success"})

@app.route('/api/add_doctor', methods=['POST'])
def api_add_doctor():
    data = request.json
    engine.send_input("2\n") # Doctor Menu
    time.sleep(0.1)
    engine.send_input("1\n") # Add Doctor
    time.sleep(0.1)
    engine.send_input(f"{data['name']}\n")
    engine.send_input(f"{data['age']}\n")
    engine.send_input(f"{data['phone']}\n")
    engine.send_input(f"{data['ssn']}\n")
    engine.send_input(f"{data['address']}\n")
    engine.send_input(f"{data['specialty']}\n")
    time.sleep(0.1)
    engine.send_input("0\n") # Back to Main Menu
    return jsonify({"status": "success"})

@app.route('/api/remove_doctor', methods=['POST'])
def api_remove_doctor():
    data = request.json
    engine.send_input("2\n") # Doctor Menu
    time.sleep(0.1)
    engine.send_input("3\n") # Remove Doctor
    time.sleep(0.1)
    engine.send_input(f"{data['id']}\n")
    time.sleep(0.1)
    engine.send_input("0\n") # Back to Main Menu
    return jsonify({"status": "success"})

if __name__ == '__main__':
    app.run(debug=True, use_reloader=False)
