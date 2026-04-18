// Poll KPIs every 2 seconds
function pollKPIs() {
    fetch('/api/stats')
        .then(res => res.json())
        .then(data => {
            const finished = document.getElementById('val-finished');
            const waiting = document.getElementById('val-waiting');
            const scheduled = document.getElementById('val-scheduled');
            
            if (finished) finished.innerText = data.finished;
            if (waiting) waiting.innerText = data.waiting;
            if (scheduled) scheduled.innerText = data.scheduled;
        });
}

function pollQueues() {
    fetch('/api/queues')
        .then(res => res.json())
        .then(data => {
            const treating = document.getElementById('queue-treating');
            const critical = document.getElementById('queue-critical');
            const normal = document.getElementById('queue-normal');
            const future = document.getElementById('queue-future');

            if (treating) {
                treating.innerHTML = data.currently_treating.map(p => {
                    const elapsed = Math.floor(Date.now() / 1000) - p.startTime;
                    const remaining = Math.max(0, p.duration - elapsed);
                    return `
                    <div class="patient-pill" data-start="${p.startTime}" data-duration="${p.duration}">
                        <div style="display:flex; justify-content:space-between; align-items:start">
                            <h4>${p.name}</h4>
                            <span class="countdown-timer" style="color:var(--accent-color); font-weight:bold; font-size:1.1rem">${remaining}s</span>
                        </div>
                        <p>Specialty: ${p.specialty}</p>
                        <p>Priority: <span style="color:#ef4444">${p.priority}</span></p>
                        <p>Dr. ${p.doctor}</p>
                    </div>`;
                }).join('') || '<p>None currently treating</p>';
            }
            if (critical) {
                critical.innerHTML = data.critical_queue.map(p => `
                    <div class="patient-pill" style="border-color:#ef4444">
                        <h4>${p.name}</h4><p>Specialty: ${p.specialty}</p>
                    </div>`).join('') || '<p>Empty</p>';
            }
            if (normal) {
                normal.innerHTML = data.normal_queue.map(p => `
                    <div class="patient-pill">
                        <h4>${p.name}</h4><p>Specialty: ${p.specialty}</p>
                    </div>`).join('') || '<p>Empty</p>';
            }
            if (future) {
                future.innerHTML = data.future_appointments.map(p => `
                    <div class="patient-pill" style="border-color:#3b82f6">
                        <div style="display:flex; justify-content:space-between; align-items:start">
                            <h4>${p.name}</h4>
                            <span class="doc-id">ID: ${p.id}</span>
                        </div>
                        <p><i class="fa-regular fa-calendar"></i> Date: ${p.date}</p>
                        <p><i class="fa-solid fa-flag"></i> Prio: ${p.criticality}</p>
                        <div class="pill-actions mt-1">
                            <button class="btn-primary btn-sm" onclick="changePatientDate('${p.id}', '${p.date}')">
                                <i class="fa-solid fa-calendar-days"></i> Change
                            </button>
                            <button class="btn-danger btn-sm" onclick="removePatient('${p.id}')">
                                <i class="fa-solid fa-trash"></i> Remove
                            </button>
                        </div>
                    </div>`).join('') || '<p>Empty</p>';
            }
        });
}

function pollDoctors() {
    fetch('/api/doctors_state')
        .then(res => res.json())
        .then(data => {
            const list = document.getElementById('doctors-list');
            if (list) {
                list.innerHTML = data.map(d => `
                    <div class="doctor-item">
                        <div class="doc-header">
                            <h4>Dr. ${d.Name}</h4>
                            <span class="doc-id">ID: ${d.ID}</span>
                        </div>
                        <p>Specialty: ${d.Specialty}</p>
                        <p>Age: ${d.Age}</p>
                        <p>Status: ${d.TotalTreated > 0 ? "Treated "+d.TotalTreated : "0 Treated"}</p>
                        <button class="btn-primary mt-1" style="width:100%" onclick="showHistory('${d.Name}')">View History</button>
                    </div>
                `).join('');
            }
        });
}

// Submissions
const addPatForm = document.getElementById('addPatientForm');
if (addPatForm) {
    addPatForm.addEventListener('submit', e => {
        e.preventDefault();
        const data = {
            name: document.getElementById('p_name').value,
            age: document.getElementById('p_age').value,
            phone: document.getElementById('p_phone').value,
            ssn: document.getElementById('p_ssn').value,
            specialty: document.getElementById('p_spec').value,
            priority: document.getElementById('p_prio').value,
            day: document.getElementById('p_day').value,
            duration: document.getElementById('p_dur').value,
            symptoms: document.getElementById('p_sym').value
        };
        fetch('/api/add_patient', {
            method: 'POST', headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(data)
        }).then(() => { alert('Patient added!'); addPatForm.reset(); });
    });
}

const addDocForm = document.getElementById('addDoctorForm');
if (addDocForm) {
    addDocForm.addEventListener('submit', e => {
        e.preventDefault();
        const data = {
            name: document.getElementById('d_name').value,
            age: document.getElementById('d_age').value,
            phone: document.getElementById('d_phone').value,
            ssn: document.getElementById('d_ssn').value,
            address: document.getElementById('d_address').value,
            specialty: document.getElementById('d_spec').value
        };
        fetch('/api/add_doctor', {
            method: 'POST', headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(data)
        }).then(() => { alert('Doctor added!'); addDocForm.reset(); closeModal('addDoctorModal'); pollDoctors(); });
    });
}

const delDocForm = document.getElementById('deleteDoctorForm');
if (delDocForm) {
    delDocForm.addEventListener('submit', e => {
        e.preventDefault();
        fetch('/api/remove_doctor', {
            method: 'POST', headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({id: document.getElementById('del_d_id').value})
        }).then(() => { alert('Doctor removed!'); delDocForm.reset(); closeModal('deleteDoctorModal'); pollDoctors(); });
    });
}

// Modals
function openAddDoctorModal() { document.getElementById('modalOverlay').classList.add('active'); document.getElementById('addDoctorModal').classList.add('active'); }
function openDeleteDoctorModal() { document.getElementById('modalOverlay').classList.add('active'); document.getElementById('deleteDoctorModal').classList.add('active'); }
function closeModal(id) { document.getElementById('modalOverlay').classList.remove('active'); document.getElementById(id).classList.remove('active'); }

function showHistory(docName) {
    fetch('/api/doctor_history/' + docName)
        .then(res => res.json())
        .then(data => {
            document.getElementById('history-total').innerText = data.total;
            document.getElementById('history-max').innerText = data.highest + 's';
            const list = document.getElementById('history-patients');
            list.innerHTML = data.patients.map(p => `
                <div class="hist-item">
                    <span>${p.name} (ID: ${p.id})</span>
                    <span>${p.time}s</span>
                </div>
            `).join('') || '<p style="text-align:center; padding:1rem; color:gray">No history yet.</p>';
            
            document.getElementById('modalOverlay').classList.add('active');
            document.getElementById('historyModal').classList.add('active');
        });
}

function removePatient(id) {
    if (confirm('Are you sure you want to remove this patient appointment?')) {
        fetch('/api/remove_patient', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({id: id})
        }).then(() => {
            alert('Patient removed successfully.');
            pollQueues();
            pollKPIs();
        });
    }
}

function changePatientDate(id, currentDate) {
    const newDate = prompt('Enter new appointment date (YYYY-MM-DD):', currentDate);
    if (newDate && newDate !== currentDate) {
        fetch('/api/change_patient_date', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({id: id, new_date: newDate})
        }).then(res => res.json())
        .then(() => {
            alert('Appointment date updated.');
            pollQueues();
        });
    }
}

// Initialize polling based on page
setInterval(() => {
    if (document.getElementById('val-finished')) pollKPIs();
    if (document.getElementById('queue-treating')) pollQueues();
    if (document.getElementById('doctors-list')) pollDoctors();
}, 2000);

// Initial fetches
if (document.getElementById('val-finished')) pollKPIs();
if (document.getElementById('queue-treating')) pollQueues();
if (document.getElementById('doctors-list')) pollDoctors();

// Theme Toggle Logic
const themeBtn = document.getElementById('theme-toggle');
const themeIcon = document.getElementById('theme-icon');
const themeText = document.getElementById('theme-text');

function toggleTheme() {
    const isLight = document.body.classList.toggle('light-mode');
    localStorage.setItem('hospital-theme', isLight ? 'light' : 'dark');
    updateThemeUI(isLight);
}

function updateThemeUI(isLight) {
    if (isLight) {
        if (themeIcon) themeIcon.className = 'fa-solid fa-sun';
        if (themeText) themeText.innerText = 'Light Mode';
    } else {
        if (themeIcon) themeIcon.className = 'fa-solid fa-moon';
        if (themeText) themeText.innerText = 'Dark Mode';
    }
}

if (themeBtn) {
    themeBtn.addEventListener('click', toggleTheme);
    
    // Check saved preference
    const savedTheme = localStorage.getItem('hospital-theme');
    if (savedTheme === 'light') {
        document.body.classList.add('light-mode');
        updateThemeUI(true);
    }
}

// Global 1s countdown for UI smoothness
setInterval(() => {
    document.querySelectorAll('.patient-pill[data-start]').forEach(el => {
        const startTime = parseInt(el.dataset.start);
        const duration = parseInt(el.dataset.duration);
        if (startTime && duration) {
            const elapsed = Math.floor(Date.now() / 1000) - startTime;
            const remaining = Math.max(0, duration - elapsed);
            const timerEl = el.querySelector('.countdown-timer');
            if (timerEl) {
                timerEl.innerText = remaining + 's';
                if (remaining === 0) {
                    timerEl.innerText = 'Finishing...';
                    timerEl.style.color = 'var(--success)';
                }
            }
        }
    });
}, 1000);
