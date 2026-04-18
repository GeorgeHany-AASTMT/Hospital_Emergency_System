#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <fstream>
#include <sstream>

using namespace std;

// ========== HELPERS ==========
string toLower(const string& str) {
    string s = str;
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

bool isValidPhone(const string& p) {
    if (p.length() != 11 || p[0] != '0') return false;
    for (char c : p) if (!isdigit(c)) return false;
    return true;
}

bool isValidSSN(const string& s) {
    if (s.length() != 14) return false;
    for (char c : s) if (!isdigit(c)) return false;
    return true;
}

int getTodayAsInt() {
    auto now = chrono::system_clock::now();
    time_t t = chrono::system_clock::to_time_t(now);
    tm* now_tm = localtime(&t);
    return (now_tm->tm_year + 1900) * 10000 + (now_tm->tm_mon + 1) * 100 + now_tm->tm_mday;
}

void printLine() { cout << string(60, '=') << "\n"; }

// ========== PATIENT ==========
struct Patient {
    int id;
    string name;
    int age;
    string phone;
    string ssn;
    int priority;       // 5 (highest/most critical) to 1 (lowest)  // 3-5: critical, 1-2: normal
    string symptoms;
    string specialty;
    string treatedBy;
    int treatmentDuration;
    int day;
    chrono::system_clock::time_point arrivalTime;
    chrono::system_clock::time_point startTime;
    bool started;

    Patient() : id(0), age(0), priority(1), treatmentDuration(60), day(0), started(false), treatedBy("None") {}

    Patient(int _id, string _name, int _age, string _phone, string _ssn,
            int _p, string _sym, string _spec, int _dur, int _day)
        : id(_id), name(_name), age(_age), phone(_phone), ssn(_ssn),
          priority(_p), symptoms(_sym), specialty(_spec),
          treatmentDuration(_dur), day(_day), started(false), treatedBy("None") {
        arrivalTime = chrono::system_clock::now();
    }

    int getWaitTime() const {
        auto end = started ? startTime : chrono::system_clock::now();
        return (int)chrono::duration_cast<chrono::seconds>(end - arrivalTime).count();
    }
};

// ========== PATIENT NODE ==========
struct PatientNode {
    Patient data;
    PatientNode* next;
    PatientNode(Patient p) : data(p), next(nullptr) {}
};

// ========== PATIENT QUEUE ==========
// A sorted Priority Queue implemented with a custom Linked List.
// HIGHER priority number = higher urgency (5 is most critical, 1 is least).
// Same priority tie-break: earlier arrival goes first.
class PatientQueue {
private:
    PatientNode* head;

public:
    PatientQueue() : head(nullptr) {}
    ~PatientQueue() {
        while (head) {
            PatientNode* temp = head;
            head = head->next;
            delete temp;
        }
    }

    void enqueue(const Patient& p) {
        PatientNode* newNode = new PatientNode(p);
        if (!head) {
            head = newNode;
        } else if (p.priority > head->data.priority ||
                  (p.priority == head->data.priority && p.arrivalTime < head->data.arrivalTime)) {
            // Higher number = higher urgency → goes to front
            newNode->next = head;
            head = newNode;
        } else {
            PatientNode* curr = head;
            while (curr->next != nullptr) {
                Patient& nextP = curr->next->data;
                if (p.priority > nextP.priority ||
                   (p.priority == nextP.priority && p.arrivalTime < nextP.arrivalTime)) {
                    break;
                }
                curr = curr->next;
            }
            newNode->next = curr->next;
            curr->next = newNode;
        }
    }

    Patient* findById(int id) {
        PatientNode* curr = head;
        while (curr) {
            if (curr->data.id == id) return &(curr->data);
            curr = curr->next;
        }
        return nullptr;
    }

    bool dequeueBySpecialty(const string& spec, Patient& out) {
        PatientNode* curr = head;
        PatientNode* prev = nullptr;
        while (curr) {
            if (curr->data.specialty == spec) {
                out = curr->data;
                if (prev) prev->next = curr->next;
                else head = curr->next;
                delete curr;
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
        return false;
    }

    bool removePatientById(int id) {
        PatientNode* curr = head;
        PatientNode* prev = nullptr;
        while (curr) {
            if (curr->data.id == id) {
                if (prev) prev->next = curr->next;
                else head = curr->next;
                delete curr;
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
        return false;
    }

    void removeExpired(const string& spec, int thresholdSec) {
        PatientNode* curr = head;
        PatientNode* prev = nullptr;
        while (curr) {
            if (curr->data.specialty == spec && curr->data.getWaitTime() >= thresholdSec) {
                cout << "\n[ALERT] Priority " << curr->data.priority << " patient " << curr->data.name
                     << " transferred to another hospital (no doctor for '"
                     << spec << "' after " << thresholdSec << "s).\n";

                PatientNode* toDelete = curr;
                if (prev) {
                    prev->next = curr->next;
                    curr = curr->next;
                } else {
                    head = curr->next;
                    curr = head;
                }
                delete toDelete;
            } else {
                prev = curr;
                curr = curr->next;
            }
        }
    }

    bool isEmpty() const { return head == nullptr; }

    int size() const {
        int count = 0;
        PatientNode* curr = head;
        while (curr) { count++; curr = curr->next; }
        return count;
    }

    vector<Patient> getAll() const {
        vector<Patient> items;
        PatientNode* curr = head;
        while (curr) {
            items.push_back(curr->data);
            curr = curr->next;
        }
        return items;
    }
};

// ========== PATIENT HISTORY LIST (Linked List) ==========
class PatientHistoryList {
private:
    PatientNode* head;
    PatientNode* tail;
public:
    PatientHistoryList() : head(nullptr), tail(nullptr) {}

    PatientHistoryList(const PatientHistoryList& other) : head(nullptr), tail(nullptr) {
        PatientNode* curr = other.head;
        while (curr) { push_back(curr->data); curr = curr->next; }
    }
    PatientHistoryList(PatientHistoryList&& other) noexcept : head(other.head), tail(other.tail) {
        other.head = nullptr; other.tail = nullptr;
    }
    PatientHistoryList& operator=(const PatientHistoryList& other) {
        if (this == &other) return *this;
        clear();
        PatientNode* curr = other.head;
        while (curr) { push_back(curr->data); curr = curr->next; }
        return *this;
    }
    PatientHistoryList& operator=(PatientHistoryList&& other) noexcept {
        if (this == &other) return *this;
        clear();
        head = other.head; tail = other.tail;
        other.head = nullptr; other.tail = nullptr;
        return *this;
    }
    ~PatientHistoryList() { clear(); }

    void clear() {
        PatientNode* curr = head;
        while (curr) { PatientNode* nxt = curr->next; delete curr; curr = nxt; }
        head = nullptr; tail = nullptr;
    }

    void push_back(Patient p) {
        PatientNode* newNode = new PatientNode(p);
        if (!head) { head = tail = newNode; }
        else { tail->next = newNode; tail = newNode; }
    }

    bool empty() const { return head == nullptr; }
    PatientNode* getHead() const { return head; }
};

// ========== DOCTOR ==========
class Doctor {
public:
    int    id;
    string name;
    int    age;
    string phone;
    string ssn;
    string address;
    string specialty;
    bool   isBusy;
    Patient currentPatient;
    int    totalTreated;
    chrono::steady_clock::time_point treatmentStart;
    PatientHistoryList history;

    Doctor(int _id, string _name, int _age, string _phone, string _ssn,
           string _addr, string _spec)
        : id(_id), name(_name), age(_age), phone(_phone), ssn(_ssn),
          address(_addr), specialty(_spec), isBusy(false), totalTreated(0) {}

    bool isFree() const { return !isBusy; }

    void assignPatient(Patient p) {
        p.started   = true;
        p.startTime = chrono::system_clock::now();
        p.treatedBy = name;
        currentPatient = p;
        isBusy         = true;
        treatmentStart = chrono::steady_clock::now();
        cout << "\n[ASSIGNED] Dr. " << name << " treating "
             << p.name << " (" << p.specialty << ")\n";
    }

    bool checkCompletion() {
        if (!isBusy) return false;
        auto elapsed = chrono::duration_cast<chrono::seconds>(
            chrono::steady_clock::now() - treatmentStart).count();
        if (elapsed >= currentPatient.treatmentDuration) {
            isBusy = false;
            totalTreated++;
            history.push_back(currentPatient);
            cout << "\n[DONE] Dr. " << name
                 << " finished treating " << currentPatient.name << "\n";
            return true;
        }
        return false;
    }

    int getRemainingTime() const {
        if (!isBusy) return 0;
        auto elapsed = chrono::duration_cast<chrono::seconds>(
            chrono::steady_clock::now() - treatmentStart).count();
        return max(0, (int)(currentPatient.treatmentDuration - elapsed));
    }
};

// ========== HOSPITAL SYSTEM ==========
class HospitalSystem {
private:
    PatientQueue    criticalQueue;   // Priority 3-5
    PatientQueue    normalQueue;     // Priority 1-2
    vector<Patient> appointmentBook;
    vector<Doctor>  doctors;
    int             nextId;
    int             doctorNextId;
    mutex           mtx;
    int             totalPatients, totalCritical, totalNormal;
    long long       totalWaitTime;
    int             treatedCount;
    int             treatedCountWhoWaited;

public:
    HospitalSystem()
        : nextId(1000), doctorNextId(6), totalPatients(0), totalCritical(0),
          totalNormal(0), totalWaitTime(0), treatedCount(0), treatedCountWhoWaited(0) {
        doctors.emplace_back(1, "Ahmed",   35, "01011112222", "12345678901234", "Cairo",      "internal");
        doctors.emplace_back(2, "Sarah",   29, "01022223333", "22345678901234", "Alexandria", "pediatrics");
        doctors.emplace_back(3, "Mohamed", 42, "01033334444", "32345678901234", "Giza",       "surgery");
        doctors.emplace_back(4, "Fatima",  31, "01044445555", "42345678901234", "Mansoura",   "cardiology");
        doctors.emplace_back(5, "Khaled",  38, "01055556666", "52345678901234", "Tanta",      "general");
        loadDoctorsFromCSV();
        loadFromCSV();
        loadHistoryFromCSV();
    }

    void saveHistoryToCSV(const Patient& p) {
        ofstream file("history.csv", ios::app);
        if (!file.is_open()) return;
        file << p.id << "," << p.name << "," << p.age << "," << p.phone << ","
             << p.ssn << "," << p.priority << "," << p.symptoms << ","
             << p.specialty << "," << p.treatmentDuration << "," << p.day << "," << p.treatedBy << "\n";
        file.close();
    }

    void loadHistoryFromCSV() {
        ifstream file("history.csv");
        if (!file.is_open()) return;
        string line, word;
        while (getline(file, line)) {
            stringstream ss(line);
            vector<string> row;
            while (getline(ss, word, ',')) row.push_back(word);
            if (row.size() < 11) continue;
            int id = stoi(row[0]);
            // FIX: Keep nextId ahead of ALL patient IDs ever seen (including treated ones).
            // Without this, restarting the engine would re-issue IDs of treated patients
            // since they are removed from patients.csv and only live in history.csv.
            if (id >= nextId) nextId = id + 1;
            Patient p(id, row[1], stoi(row[2]), row[3], row[4],
                      stoi(row[5]), row[6], row[7], stoi(row[8]), stoi(row[9]));
            p.treatedBy = row[10];
            for (auto& d : doctors) {
                if (d.name == p.treatedBy) { d.history.push_back(p); break; }
            }
        }
        file.close();
    }

    void saveDoctorsToCSV() {
        ofstream file("doctors.csv");
        if (!file.is_open()) return;
        file << "ID,Name,Age,Phone,SSN,Address,Specialty,TotalTreated\n";
        for (const auto& d : doctors)
            file << d.id << "," << d.name << "," << d.age << "," << d.phone << ","
                 << d.ssn << "," << d.address << "," << d.specialty << "," << d.totalTreated << "\n";
        file.close();
    }

    void loadDoctorsFromCSV() {
        ifstream file("doctors.csv");
        if (!file.is_open()) return;
        doctors.clear();
        string line, word;
        getline(file, line);
        while (getline(file, line)) {
            stringstream ss(line);
            vector<string> row;
            while (getline(ss, word, ',')) row.push_back(word);
            if (row.size() < 8) continue;
            int id = stoi(row[0]);
            doctors.emplace_back(id, row[1], stoi(row[2]), row[3], row[4], row[5], row[6]);
            doctors.back().totalTreated = stoi(row[7]);
            if (id >= doctorNextId) doctorNextId = id + 1;
        }
        file.close();
    }

    void saveToCSV() {
        ofstream file("patients.csv");
        if (!file.is_open()) return;
        file << "ID,Name,Age,Phone,SSN,Priority,Symptoms,Specialty,Duration,Day,TreatedBy,ArrivalTime,TreatmentStartTime\n";
        auto writePatient = [&](const Patient& p) {
            auto arrival = chrono::system_clock::to_time_t(p.arrivalTime);
            long long startTs = 0;
            if (p.started) {
                startTs = (long long)chrono::system_clock::to_time_t(p.startTime);
            }
            file << p.id << "," << p.name << "," << p.age << "," << p.phone << ","
                 << p.ssn << "," << p.priority << "," << p.symptoms << ","
                 << p.specialty << "," << p.treatmentDuration << "," << p.day << ","
                 << p.treatedBy << "," << (long long)arrival << "," << startTs << "\n";
        };
        for (const auto& p : criticalQueue.getAll()) writePatient(p);
        for (const auto& p : normalQueue.getAll())   writePatient(p);
        for (const auto& p : appointmentBook)        writePatient(p);
        for (const auto& d : doctors)
            if (d.isBusy) writePatient(d.currentPatient);
        file.close();
    }

    void loadFromCSV() {
        ifstream file("patients.csv");
        if (!file.is_open()) return;
        string line, word;
        getline(file, line);
        while (getline(file, line)) {
            stringstream ss(line);
            vector<string> row;
            while (getline(ss, word, ',')) row.push_back(word);
            if (row.size() < 11) continue;
            int id   = stoi(row[0]);
            int prio = stoi(row[5]);
            int dur  = stoi(row[8]);
            int day  = stoi(row[9]);
            long long arrivalTs = (row.size() >= 12) ? stoll(row[11]) : 0;
            long long startTs   = (row.size() >= 13) ? stoll(row[12]) : 0;
            
            Patient p(id, row[1], stoi(row[2]), row[3], row[4], prio, row[6], row[7], dur, day);
            p.treatedBy = row[10];
            if (arrivalTs > 0) p.arrivalTime = chrono::system_clock::from_time_t((time_t)arrivalTs);
            if (startTs > 0) {
                p.started = true;
                p.startTime = chrono::system_clock::from_time_t((time_t)startTs);
            }
            
            if (id >= nextId) nextId = id + 1;
            
            // Restore "Currently Treating" state
            if (p.treatedBy != "None" && p.started) {
                bool assigned = false;
                for (auto& d : doctors) {
                    if (d.name == p.treatedBy) {
                        d.currentPatient = p;
                        d.isBusy = true;
                        // Synchronize steady_clock with the saved system_clock startTime
                        auto diff = chrono::system_clock::now() - p.startTime;
                        d.treatmentStart = chrono::steady_clock::now() - chrono::duration_cast<chrono::steady_clock::duration>(diff);
                        assigned = true;
                        break;
                    }
                }
                if (!assigned) {
                    // Doctor not found or something, put back in queue? 
                    // For now, let's just put it in queue as a fallback.
                    if (prio >= 3) { criticalQueue.enqueue(p); totalCritical++; }
                    else           { normalQueue.enqueue(p);   totalNormal++;   }
                }
            } else {
                int today = getTodayAsInt();
                if (day > today) {
                    appointmentBook.push_back(p);
                } else {
                    if (prio >= 3) { criticalQueue.enqueue(p); totalCritical++; }
                    else           { normalQueue.enqueue(p);   totalNormal++;   }
                }
            }
            totalPatients++;
        }
        file.close();
    }

    // ---------- Patient Management ----------
    void addPatient(string name, int age, string phone, string ssn,
                    int priority, string symptoms, string spec, int duration, int day) {
        lock_guard<mutex> lock(mtx);
        string s = toLower(spec);
        Patient p(nextId++, name, age, phone, ssn, priority, symptoms, s, duration, day);
        totalPatients++;

        int today = getTodayAsInt();
        if (day > today) {
            appointmentBook.push_back(p);
            cout << "\n[SCHEDULED] " << name << " booked for future date: " << day << "\n";
        } else {
            // 5 = most critical, 1 = least urgent
            if (priority >= 3) { criticalQueue.enqueue(p); totalCritical++; }
            else               { normalQueue.enqueue(p);   totalNormal++;   }
            cout << "\n[NEW PATIENT] " << name << " | Priority: " << priority << " (Entered Active Queue)\n";
        }
        saveToCSV();

        bool hasDoc = false;
        for (const auto& d : doctors)
            if (d.specialty == s) { hasDoc = true; break; }
        if (!hasDoc)
            cout << "[WARNING] No doctor for '" << s
                 << "'. Patient will be transferred if waiting > 30s.\n";
    }

    void checkInScheduledPatients() {
        int today = getTodayAsInt();
        bool changed = false;
        for (auto it = appointmentBook.begin(); it != appointmentBook.end(); ) {
            if (it->day <= today) {
                cout << "\n[CHECK-IN] Appointment day arrived for " << it->name << ". Moving to active queue.\n";
                if (it->priority >= 3) { criticalQueue.enqueue(*it); totalCritical++; }
                else                   { normalQueue.enqueue(*it);   totalNormal++;   }
                it = appointmentBook.erase(it);
                changed = true;
            } else { ++it; }
        }
        if (changed) saveToCSV();
    }

    void displayPatients() {
        lock_guard<mutex> lock(mtx);
        printLine();
        cout << setw(28) << right << "PATIENT QUEUES\n";
        printLine();

        auto printQueue = [](const string& label, const PatientQueue& q) {
            cout << label << " (" << q.size() << " waiting):\n";
            if (q.isEmpty()) { cout << "   (empty)\n"; return; }
            for (const auto& p : q.getAll())
                cout << "   [ID:" << p.id << "] " << setw(12) << left << p.name
                     << " Pri:" << p.priority
                     << " Spec:" << p.specialty
                     << " Day:" << p.day
                     << " Wait:" << p.getWaitTime() << "s\n";
        };

        printQueue(">> CRITICAL QUEUE (Priority 3-5)", criticalQueue);
        printQueue(">> NORMAL QUEUE   (Priority 1-2)", normalQueue);

        cout << ">> FUTURE APPOINTMENTS (In Book):\n";
        if (appointmentBook.empty()) { cout << "   (none)\n"; }
        else {
            for (const auto& p : appointmentBook)
                cout << "   [ID:" << p.id << "] " << setw(12) << left << p.name
                     << " Day:" << p.day << " Pri:" << p.priority << "\n";
        }

        cout << ">> CURRENTLY IN TREATMENT:\n";
        bool any = false;
        for (const auto& d : doctors) {
            if (d.isBusy) {
                cout << "   " << d.currentPatient.name
                     << " with Dr." << d.name
                     << " | Rem: " << d.getRemainingTime() << "s\n";
                any = true;
            }
        }
        if (!any) cout << "   (none)\n";
        printLine();
    }

    void removePatient(int id) {
        lock_guard<mutex> lock(mtx);
        if (criticalQueue.removePatientById(id)) {
            totalCritical--; totalPatients--;
            cout << "\n[SYSTEM] Removed Patient ID " << id << " from Critical Queue.\n";
            saveToCSV(); return;
        }
        if (normalQueue.removePatientById(id)) {
            totalNormal--; totalPatients--;
            cout << "\n[SYSTEM] Removed Patient ID " << id << " from Normal Queue.\n";
            saveToCSV(); return;
        }
        for (auto it = appointmentBook.begin(); it != appointmentBook.end(); ++it) {
            if (it->id == id) {
                cout << "\n[SYSTEM] Removed Patient ID " << id << " from Appointment Book.\n";
                appointmentBook.erase(it); totalPatients--;
                saveToCSV(); return;
            }
        }
        cout << "\n[ERROR] Patient ID " << id << " not found anywhere.\n";
    }

    void changePatientDay(int id, int newDay) {
        lock_guard<mutex> lock(mtx);
        Patient* p = nullptr;
        p = criticalQueue.findById(id);
        if (!p) p = normalQueue.findById(id);
        if (!p) {
            for (auto& ap : appointmentBook) {
                if (ap.id == id) { p = &ap; break; }
            }
        }
        if (!p) { cout << "\n[ERROR] Patient ID " << id << " not found.\n"; return; }
        if (p->day == 0) { cout << "\n[ERROR] Patient does not have an initial day assigned.\n"; return; }
        int today = getTodayAsInt();
        if (today == p->day) {
            cout << "\n[ERROR] Cannot change appointment on the same day (Today is " << today << ").\n"; return;
        }
        if (newDay <= today) {
            cout << "\n[ERROR] New day (" << newDay << ") must be in the future (Today is " << today << ").\n"; return;
        }
        int oldDay = p->day;
        p->day = newDay;
        cout << "\n[SUCCESS] Patient " << p->name << " (ID:" << id << ") changed day from " << oldDay << " to " << newDay << ".\n";
        saveToCSV();
    }

    // ---------- Doctor Management ----------
    void addDoctor(string name, int age, string phone, string ssn,
                   string address, string spec) {
        lock_guard<mutex> lock(mtx);
        int newId = doctorNextId++;
        string s = toLower(spec);
        doctors.emplace_back(newId, name, age, phone, ssn, address, s);
        cout << "\n[SYSTEM] Added Dr." << name << " (" << s << ") ID:" << newId << "\n";
        saveDoctorsToCSV();
    }

    void displayDoctors() {
        lock_guard<mutex> lock(mtx);
        printLine();
        cout << setw(28) << right << "DOCTORS LIST\n";
        printLine();
        cout << left << setw(5)<<"ID" << setw(12)<<"Name"
             << setw(14)<<"Specialty" << setw(6)<<"Age"
             << setw(18)<<"Status" << "Treated\n"
             << string(60,'-') << "\n";
        for (const auto& d : doctors) {
            string spec = d.specialty;
            if (!spec.empty()) spec[0] = toupper(spec[0]);
            string status = d.isBusy
                ? "BUSY (" + to_string(d.getRemainingTime()) + "s left)"
                : "FREE";
            cout << left << setw(5)  << d.id
                 << setw(12) << d.name
                 << setw(14) << spec
                 << setw(6)  << d.age
                 << setw(18) << status
                 << d.totalTreated << "\n";
        }
        int avg = treatedCountWhoWaited > 0 ? (int)(totalWaitTime / treatedCountWhoWaited) : 0;
        int activeWaiting = criticalQueue.size() + normalQueue.size();
        cout << string(60,'-') << "\n"
             << "Finished:" << treatedCount
             << " | Waiting:" << activeWaiting
             << " | Scheduled:" << (int)appointmentBook.size()
             << " | AvgWait:" << avg << "s\n";
        printLine();
    }

    void removeDoctor(int docId) {
        lock_guard<mutex> lock(mtx);
        for (auto it = doctors.begin(); it != doctors.end(); ++it) {
            if (it->id == docId) {
                cout << "\n[SYSTEM] Removed Dr." << it->name << "\n";
                doctors.erase(it); saveDoctorsToCSV(); return;
            }
        }
        cout << "\n[ERROR] Doctor ID " << docId << " not found.\n";
    }

    vector<string> getAvailableSpecialties() {
        lock_guard<mutex> lock(mtx);
        vector<string> specs;
        for (const auto& d : doctors)
            if (find(specs.begin(), specs.end(), d.specialty) == specs.end())
                specs.push_back(d.specialty);
        sort(specs.begin(), specs.end());
        return specs;
    }

    void displayDoctorHistoryById(int docId) {
        lock_guard<mutex> lock(mtx);
        for (const auto& d : doctors) {
            if (d.id == docId) {
                cout << "\n"; printLine();
                cout << "     PATIENTS DIAGNOSED BY DR. " << d.name << "\n"; printLine();
                if (d.history.empty()) {
                    cout << "  No patients treated yet.\n";
                } else {
                    int idx = 1;
                    PatientNode* curr = d.history.getHead();
                    while (curr != nullptr) {
                        const Patient& p = curr->data;
                        cout << "  " << idx++ << ". Patient: " << p.name
                             << " | ID: " << p.id
                             << "\n     Symptoms: " << p.symptoms
                             << "\n     Doctor: Dr. " << p.treatedBy
                             << "\n     Treatment: " << p.treatmentDuration << "s\n";
                        curr = curr->next;
                    }
                }
                printLine();
                return;
            }
        }
        cout << "\n[ERROR] Doctor ID " << docId << " not found.\n";
    }

    // ---------- Background Processor ----------
    void update() {
        lock_guard<mutex> lock(mtx);
        bool stateChanged = false;

        checkInScheduledPatients();

        vector<string> specs;
        for (const auto& p : criticalQueue.getAll())
            if (find(specs.begin(), specs.end(), p.specialty) == specs.end())
                specs.push_back(p.specialty);
        for (const auto& p : normalQueue.getAll())
            if (find(specs.begin(), specs.end(), p.specialty) == specs.end())
                specs.push_back(p.specialty);

        for (const string& spec : specs) {
            bool hasDoc = false;
            for (const auto& d : doctors)
                if (d.specialty == spec) { hasDoc = true; break; }
            if (!hasDoc) {
                int beforeC = criticalQueue.size(), beforeN = normalQueue.size();
                criticalQueue.removeExpired(spec, 30);
                normalQueue.removeExpired(spec, 30);
                if (criticalQueue.size() != beforeC || normalQueue.size() != beforeN)
                    stateChanged = true;
            }
        }

        for (auto& doc : doctors) {
            if (doc.checkCompletion()) {
                saveHistoryToCSV(doc.currentPatient);
                int wt = doc.currentPatient.getWaitTime();
                if (wt > 0) { treatedCountWhoWaited++; totalWaitTime += wt; }
                treatedCount++;
                saveDoctorsToCSV();
                stateChanged = true;
            }
            if (doc.isFree()) {
                Patient p;
                if (criticalQueue.dequeueBySpecialty(doc.specialty, p)) {
                    doc.assignPatient(p); stateChanged = true;
                } else if (normalQueue.dequeueBySpecialty(doc.specialty, p)) {
                    doc.assignPatient(p); stateChanged = true;
                }
            }
        }

        if (stateChanged) saveToCSV();
    }
};

// ========== GLOBAL ==========
HospitalSystem hospital;
string adminPassword = "admin";
atomic<bool> simulationRunning(true);
mutex terminal_mtx;

void safePrint(const string& msg) {
    lock_guard<mutex> lock(terminal_mtx);
    cout << msg;
}

void simulationThread() {
    while (simulationRunning) {
        hospital.update();
        this_thread::sleep_for(chrono::seconds(1));
    }
}

// ========== MENUS ==========
void patientManagementMenu() {
    int choice;
    while (true) {
        cout << "\n"; printLine();
        cout << "         PATIENT MANAGEMENT\n"; printLine();
        cout << "  1. Add Patient\n  2. Display Patients\n  3. Remove Patient (by ID)\n  4. Change Patient Day\n  0. Go Back\n";
        printLine();
        cout << "Choice: ";
        if (!(cin >> choice)) { cin.clear(); cin.ignore(1000,'\n'); continue; }
        if (choice == 0) break;

        if (choice == 1) {
            string name, phone, ssn, sym, spec;
            int age, prio, dur, day;

            cout << "\nName: "; cin.ignore(); getline(cin, name);
            while (true) {
                cout << "Age (1-100): ";
                if (cin >> age && age >= 1 && age <= 100) break;
                cout << "Invalid. Must be 1-100.\n";
                cin.clear(); cin.ignore(1000,'\n');
            }
            while (true) {
                cout << "Phone (11 digits, starts with 0): "; cin >> phone;
                if (isValidPhone(phone)) break;
                cout << "Invalid phone.\n";
            }
            while (true) {
                cout << "SSN (14 digits): "; cin >> ssn;
                if (isValidSSN(ssn)) break;
                cout << "Invalid SSN.\n";
            }
            {
                vector<string> avail = hospital.getAvailableSpecialties();
                cout << "Available Specialties: ";
                for (int i = 0; i < (int)avail.size(); i++)
                    cout << avail[i] << (i+1 < (int)avail.size() ? "/" : "");
                cout << "\nSpecialty: ";
            }
            cin >> spec;
            while (true) {
                // 5 = most critical, 1 = least urgent
                cout << "Priority (5=Critical, 4=High, 3=medium, 2=Low, 1=Minimal): ";
                if (cin >> prio && prio >= 1 && prio <= 5) break;
                cout << "Invalid priority.\n";
                cin.clear(); cin.ignore(1000,'\n');
            }
            while (true) {
                cout << "Appointment Day (Format YYYYMMDD, e.g. " << getTodayAsInt() << "): ";
                if (cin >> day && day >= getTodayAsInt()) break;
                cout << "Invalid day. Must be today or in the future.\n";
                cin.clear(); cin.ignore(1000, '\n');
            }
            while (true) {
                cout << "Treatment Duration in seconds (1-1800): ";
                if (cin >> dur && dur >= 1 && dur <= 1800) break;
                cout << "Invalid duration.\n";
                cin.clear(); cin.ignore(1000,'\n');
            }
            cout << "Symptoms: "; cin.ignore(); getline(cin, sym);
            hospital.addPatient(name, age, phone, ssn, prio, sym, spec, dur, day);

        } else if (choice == 2) {
            hospital.displayPatients();
        } else if (choice == 3) {
            int id;
            cout << "\nEnter Patient ID to remove: ";
            if (cin >> id) hospital.removePatient(id);
            else { cin.clear(); cin.ignore(1000, '\n'); cout << "Invalid input.\n"; }
        } else if (choice == 4) {
            int id, newDay;
            cout << "\nEnter Patient ID: ";
            if (!(cin >> id)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
            cout << "Current System Day: " << getTodayAsInt() << "\n";
            cout << "Enter New Appointment Day (YYYYMMDD): ";
            if (!(cin >> newDay)) { cin.clear(); cin.ignore(1000, '\n'); continue; }
            hospital.changePatientDay(id, newDay);
        } else {
            cout << "Invalid choice.\n";
        }
    }
}

void doctorManagementMenu() {
    int choice;
    while (true) {
        cout << "\n"; printLine();
        cout << "         DOCTOR MANAGEMENT\n"; printLine();
        cout << "  1. Add Doctor\n  2. Display Doctors\n  3. Remove Doctor (by ID)\n  0. Go Back\n";
        printLine();
        cout << "Choice: ";
        if (!(cin >> choice)) { cin.clear(); cin.ignore(1000,'\n'); continue; }
        if (choice == 0) break;

        if (choice == 1) {
            string name, phone, ssn, address, spec;
            int age;
            cout << "\nDoctor Name: "; cin.ignore(); getline(cin, name);
            while (true) {
                cout << "Age (25-60): ";
                if (cin >> age && age >= 25 && age <= 60) break;
                cout << "Invalid. Must be 25-60.\n";
                cin.clear(); cin.ignore(1000,'\n');
            }
            while (true) {
                cout << "Phone (11 digits, starts with 0): "; cin >> phone;
                if (isValidPhone(phone)) break;
                cout << "Invalid phone.\n";
            }
            while (true) {
                cout << "SSN (14 digits): "; cin >> ssn;
                if (isValidSSN(ssn)) break;
                cout << "Invalid SSN.\n";
            }
            cout << "Address: "; cin.ignore(); getline(cin, address);
            cout << "Specialty: "; cin >> spec;
            hospital.addDoctor(name, age, phone, ssn, address, spec);

        } else if (choice == 2) {
            hospital.displayDoctors();
            int docId;
            cout << "\nEnter Doctor ID to view diagnosed patients (or 0 to skip): ";
            if (!(cin >> docId)) { cin.clear(); cin.ignore(1000, '\n'); }
            else if (docId != 0) hospital.displayDoctorHistoryById(docId);
        } else if (choice == 3) {
            int id;
            cout << "Enter Doctor ID to remove: ";
            if (cin >> id) hospital.removeDoctor(id);
        } else {
            cout << "Invalid choice.\n";
        }
    }
}

bool loginScreen() {
    printLine();
    cout << "     HOSPITAL EMERGENCY SYSTEM - LOGIN\n";
    printLine();
    string pwd;
    cout << "Password: "; cin >> pwd;
    if (pwd == adminPassword) { cout << "\n[OK] Logged in.\n"; return true; }
    cout << "\n[ERROR] Wrong password.\n";
    return false;
}

void mainMenu() {
    int choice;
    while (true) {
        cout << "\n"; printLine();
        cout << "       HOSPITAL EMERGENCY SYSTEM\n"; printLine();
        cout << "  1. Patient Management\n  2. Doctor Management\n  0. Logout\n";
        printLine();
        cout << "Choice: ";
        if (!(cin >> choice)) { cin.clear(); cin.ignore(1000,'\n'); continue; }
        if (choice == 0) { cout << "\n[SYSTEM] Logged out.\n"; return; }
        else if (choice == 1) patientManagementMenu();
        else if (choice == 2) doctorManagementMenu();
        else cout << "Invalid choice.\n";
    }
}

// ========== MAIN ==========
int main() {
    thread sim(simulationThread);
    sim.detach();
    while (true) {
        cout << "\n";
        if (loginScreen()) mainMenu();
    }
    return 0;
}