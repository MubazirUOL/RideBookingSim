#include <iostream>
#include <vector>
#include <queue>
#include <stack>
#include <unordered_map>
#include <algorithm>
#include <limits>
#include <climits>
#include <map>
#include <fstream>
#include <sstream>
// #include <conio.h>
using namespace std;

/* ============================================================
CLASS MODELS
============================================================ */

class User {
private:
    int id;
    string name;

public:
    User() : id(0), name("") {}
    User(int i, const string& n) : id(i), name(n) {}

    int getId() const { return id; }
    string getName() const { return name; }
};

class Driver {
private:
    int id;
    string name;
    string currentLocation;
    float rating;
    int totalRatings;
    float ratingSum;
    bool available;

public:
    Driver() : id(0), name(""), currentLocation(""), rating(0.0f), totalRatings(0), ratingSum(0.0f), available(true) {}

    Driver(int i, const string& n, const string& loc, float r) : id(i), name(n), currentLocation(loc), rating(r), totalRatings(1), ratingSum(r), available(true) {}

    Driver(int i, const string& n, const string& loc, float r, int tr, float rs, bool av) : id(i), name(n), currentLocation(loc), rating(r), totalRatings(tr), ratingSum(rs), available(av) {}

    int    getId()                 const { return id; }
    string getName()               const { return name; }
    string getCurrentLocation()    const { return currentLocation; }
    float  getRating()             const { return rating; }
    int    getTotalRatings()       const { return totalRatings; }
    float  getRatingSum()          const { return ratingSum; }
    bool   isAvailable()           const { return available; }

    void setAvailable(bool val)                { available = val; }
    void setCurrentLocation(const string& loc) { currentLocation = loc; }

    // Updates running average rating
    void addRating(float newRating) {
        ratingSum    += newRating;
        totalRatings += 1;
        rating        = ratingSum / totalRatings;
    }
};

class Ride {
private:
    int    rideId;
    string userName;
    string driverName;
    string pickup;
    string destination;
    int    fare;
    string status;

public:
    Ride() : rideId(0), fare(0), status("Active") {}

    Ride(int r, const string& u, const string& d,
         const string& p, const string& des, int f)
        : rideId(r), userName(u), driverName(d),
          pickup(p), destination(des), fare(f), status("Active") {}

    int    getRideId()         const { return rideId; }
    string getUserName()       const { return userName; }
    string getDriverName()     const { return driverName; }
    string getPickup()         const { return pickup; }
    string getDestination()    const { return destination; }
    int    getFare()           const { return fare; }
    string getStatus()         const { return status; }

    void setStatus(const string& s) { status = s; }
};

/* ============================================================
   GLOBALS
   ============================================================ */

unordered_map<int, User>                        users;
vector<Driver>                                  drivers;
queue<Ride>                                     rideQueue;   // FIFO ride dispatch queue
stack<Ride>                                     rideHistory; // completed rides (LIFO)
unordered_map<string, vector<pair<string,int>>> graph;
vector<Ride>                                    activeRides;

int nextRideId = 1; // auto-incrementing, safe after file load

const float DISTANCE_WEIGHT = 1.0f;
const float RATING_WEIGHT   = 2.0f;

/* ============================================================
   FORWARD DECLARATIONS
   ============================================================ */

void registerUser();
void addDriver();
void updateDriverRating(const string& driverName, float newRating);
bool driverIdExists(int id);
bool userIdExists(int id);

pair<int, vector<string>> dijkstra(const string& source, const string& destination);
int   calculateDriverDistance(const string& driverLocation, const string& userPickup);
float calculateDriverScore(int distance, float rating);

// Manual sorting algorithms
void insertionSortByRating(vector<Driver>& arr);
void selectionSortByDistance(vector<pair<int, Driver*>>& arr);

void viewDrivers();
void viewAllDrivers();

void bookRide();
void cancelRide();
void completeRide();
void viewActiveRides();
void viewRideHistory();
void processNextQueuedRide();

void addRoad(const string& u, const string& v, int distance);
void addRoadFromInput();
void createCityMap();
void viewCityMap();

Driver* findNearestDriver(const string& pickup);
Driver* findBestDriver(const string& pickup);
void showNearbyDrivers();

void saveUsersToFile();
void loadUsersFromFile();
void saveDriversToFile();
void loadDriversFromFile();
void saveRidesToFile();
void loadRidesFromFile();

void adminDashboard();

void searchUserById();
void searchDriverById();
void searchDriverByName();
void searchRideById();
void searchMenu();

void userMenu();
void driverMenu();
void rideMenu();
void mapMenu();
void adminMenu();

int  readInt(const string& prompt);
string readLineAfterCin(const string& prompt);

/* ============================================================
   INPUT UTILITIES
   ============================================================ */

// Safe integer input — loops until a valid integer is entered
int readInt(const string& prompt) {
    int val;
    while (true) {
        cout << prompt;
        if (cin >> val) return val;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "  Invalid input. Please enter a number.\n";
    }
}

// Reads a full line after a cin >> (handles leftover newline)
string readLineAfterCin(const string& prompt) {
    string input;
    cout << prompt;
    if (cin.peek() == '\n') cin.ignore();
    getline(cin, input);
    return input;
}

bool driverIdExists(int id) {
    // Linear Search O(n)
    for (const auto& d : drivers)
        if (d.getId() == id) return true;
    return false;
}

bool userIdExists(int id) {
    return users.find(id) != users.end(); // O(1) average - hash map
}

/* ============================================================
   GRAPH / DIJKSTRA
   ============================================================ */

void addRoad(const string& u, const string& v, int distance) {
    graph[u].push_back({v, distance});
    graph[v].push_back({u, distance});
}

// Lets admin add a new road/node at runtime
void addRoadFromInput() {
    string u = readLineAfterCin("Enter first location (node): ");
    string v = readLineAfterCin("Enter second location (node): ");
    int dist = readInt("Enter distance in km: ");

    if (dist <= 0) {
        cout << "Distance must be positive.\n";
        return;
    }

    addRoad(u, v, dist);
    cout << "Road added: " << u << " <-> " << v << " (" << dist << " km)\n";
}

void createCityMap() {
    addRoad("A","B",5);
    addRoad("A","C",2);
    addRoad("B","D",4);
    addRoad("C","D",7);
    addRoad("C","E",6);
    addRoad("D","E",1);
    addRoad("A","F",8);
    addRoad("B","E",7);
    addRoad("B","G",6);
    addRoad("C","F",4);
    addRoad("D","G",3);
    addRoad("D","H",9);
    addRoad("E","H",5);
    addRoad("E","I",8);
    addRoad("F","G",2);
    addRoad("F","J",7);
    addRoad("G","I",4);
    addRoad("G","J",6);
    addRoad("H","I",3);
    addRoad("H","J",5);
    addRoad("I","J",2);
    addRoad("B","F",10);
    addRoad("C","G",9);
    addRoad("D","I",6);
}

void viewCityMap() {
    cout << "\n===== CITY MAP =====\n";
    cout << "Valid locations: ";
    for (const auto& node : graph)
        cout << "[" << node.first << "] ";
    cout << "\n\n";
    for (const auto& node : graph) {
        cout << node.first << "\n";
        for (const auto& neighbor : node.second)
            cout << "   -> " << neighbor.first << " (" << neighbor.second << " km)\n";
    }
}

// Dijkstra's Algorithm — O((V + E) log V) using min-heap priority queue
// Returns {shortest distance, path} or {-1, {}} if unreachable
pair<int, vector<string>> dijkstra(const string& source, const string& destination) {

    if (graph.find(source) == graph.end() || graph.find(destination) == graph.end())
        return {-1, {}};

    unordered_map<string, int>    dist;
    unordered_map<string, string> parent;

    for (const auto& node : graph)
        dist[node.first] = INT_MAX;

    dist[source] = 0;

    // Min-heap: {distance, node}
    priority_queue<
        pair<int, string>,
        vector<pair<int, string>>,
        greater<pair<int, string>>
    > pq;

    pq.push({0, source});

    while (!pq.empty()) {
        int currentDist = pq.top().first;
        string currentNode = pq.top().second;
        pq.pop();

        for (const auto& neighbor : graph[currentNode]) {
            const string& nextNode = neighbor.first;
            int weight = neighbor.second;

            if (currentDist + weight < dist[nextNode]) {
                dist[nextNode]   = currentDist + weight;
                parent[nextNode] = currentNode;
                pq.push({dist[nextNode], nextNode});
            }
        }
    }

    if (dist[destination] == INT_MAX)
        return {-1, {}};

    // Path reconstruction
    vector<string> path;
    string current = destination;

    while (current != source) {
        path.push_back(current);
        if (parent.find(current) == parent.end())
            return {-1, {}};
        current = parent[current];
    }
    path.push_back(source);
    reverse(path.begin(), path.end());

    return {dist[destination], path};
}

/* ============================================================
   SORTING ALGORITHMS (Manual Implementations)
   ============================================================ */

// Insertion Sort — O(n^2) — sorts drivers by rating descending
// Used in: viewDrivers()
void insertionSortByRating(vector<Driver>& arr) {
    int n = arr.size();
    for (int i = 1; i < n; i++) {
        Driver key = arr[i];
        int    j   = i - 1;
        // Shift elements with lower rating to the right
        while (j >= 0 && arr[j].getRating() < key.getRating()) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// Selection Sort — O(n^2) — sorts driver-distance pairs by distance ascending
// Used in: showNearbyDrivers()
void selectionSortByDistance(vector<pair<int, Driver*>>& arr) {
    int n = arr.size();
    for (int i = 0; i < n; i++) {
        int minIdx = i;
        // Find the index of the minimum distance in the unsorted portion
        for (int j = i + 1; j < n; j++) {
            if (arr[j].first < arr[minIdx].first)
                minIdx = j;
        }
        if (minIdx != i)
            swap(arr[i], arr[minIdx]);
    }
}

/* ============================================================
   DRIVER SCORING & SELECTION
   ============================================================ */

int calculateDriverDistance(const string& driverLocation, const string& userPickup) {
    auto result = dijkstra(driverLocation, userPickup);
    return result.first; // -1 if unreachable
}

float calculateDriverScore(int distance, float rating) {
    // Lower score = better driver assignment
    // Close drivers and high ratings both improve the score
    return (DISTANCE_WEIGHT * distance) - (RATING_WEIGHT * rating);
}

// Finds closest driver purely by graph distance — O(n * Dijkstra)
Driver* findNearestDriver(const string& pickup) {
    Driver* nearest = nullptr;
    int     minDist = INT_MAX;

    for (auto& d : drivers) {
        if (!d.isAvailable()) continue;
        int dist = calculateDriverDistance(d.getCurrentLocation(), pickup);
        if (dist != -1 && dist < minDist) {
            minDist = dist;
            nearest = &d;
        }
    }
    return nearest;
}

// Finds best driver using weighted score (distance + rating) — O(n * Dijkstra)
Driver* findBestDriver(const string& pickup) {
    Driver* best      = nullptr;
    float   bestScore = numeric_limits<float>::max();

    for (auto& d : drivers) {
        if (!d.isAvailable()) continue;
        int dist = calculateDriverDistance(d.getCurrentLocation(), pickup);
        if (dist == -1) continue;

        float score = calculateDriverScore(dist, d.getRating());
        if (score < bestScore) {
            bestScore = score;
            best      = &d;
        }
    }
    return best;
}

void updateDriverRating(const string& driverName, float newRating) {
    // Linear Search O(n) — find driver by name and update rating
    for (auto& d : drivers) {
        if (d.getName() == driverName) {
            d.addRating(newRating);
            cout << "  New average rating for " << driverName
                 << ": " << d.getRating() << "\n";
            saveDriversToFile();
            return;
        }
    }
}

/* ============================================================
   USER MANAGEMENT
   ============================================================ */

void registerUser() {
    int id = readInt("\nEnter User ID: ");

    if (userIdExists(id)) {
        cout << "User ID already exists!\n";
        return;
    }

    string name = readLineAfterCin("Enter Name: ");
    if (name.empty()) { cout << "Name cannot be empty.\n"; return; }

    users[id] = User(id, name);
    cout << "User Registered Successfully!\n";
    saveUsersToFile();
}

void viewUsers() {
    if (users.empty()) { cout << "\nNo users registered.\n"; return; }
    cout << "\n===== REGISTERED USERS =====\n";
    for (const auto& u : users)
        cout << "ID: " << u.second.getId()
             << " | Name: " << u.second.getName() << "\n";
}

/* ============================================================
   DRIVER MANAGEMENT
   ============================================================ */

void addDriver() {
    int id = readInt("\nEnter Driver ID: ");

    if (driverIdExists(id)) {
        cout << "Driver ID already exists!\n";
        return;
    }

    string name = readLineAfterCin("Enter Driver Name: ");
    if (name.empty()) { cout << "Name cannot be empty.\n"; return; }

    cout << "Valid map locations: ";
    for (const auto& n : graph) cout << "[" << n.first << "] ";
    cout << "\n";

    string location = readLineAfterCin("Enter Driver Current Location: ");

    if (graph.find(location) == graph.end()) {
        cout << "Invalid location '" << location << "'!\n";
        return;
    }

    float rating;
    cout << "Enter Initial Rating (0.0 - 5.0): ";
    cin  >> rating;

    if (rating < 0.0f || rating > 5.0f) {
        cout << "Invalid rating. Must be 0.0 - 5.0.\n";
        return;
    }

    drivers.push_back(Driver(id, name, location, rating));
    cout << "Driver Added Successfully!\n";
    saveDriversToFile();
}

// View available drivers — sorted by rating using Insertion Sort
void viewDrivers() {
    vector<Driver> sorted = drivers;

    // Insertion Sort O(n^2) — by rating descending
    insertionSortByRating(sorted);

    cout << "\n===== AVAILABLE DRIVERS (Insertion Sort by Rating) =====\n";
    bool any = false;
    for (const auto& d : sorted) {
        if (d.isAvailable()) {
            cout << "ID: "        << d.getId()
                 << " | Name: "   << d.getName()
                 << " | Loc: "    << d.getCurrentLocation()
                 << " | Rating: " << d.getRating()
                 << " (from "     << d.getTotalRatings() << " ratings)"
                 << "\n";
            any = true;
        }
    }
    if (!any) cout << "No available drivers.\n";
}

// View ALL drivers (available + busy)
void viewAllDrivers() {
    if (drivers.empty()) { cout << "\nNo drivers registered.\n"; return; }

    vector<Driver> sorted = drivers;
    insertionSortByRating(sorted); // Insertion Sort O(n^2)

    cout << "\n===== ALL DRIVERS (Insertion Sort by Rating) =====\n";
    for (const auto& d : sorted) {
        cout << "ID: "        << d.getId()
             << " | Name: "   << d.getName()
             << " | Loc: "    << d.getCurrentLocation()
             << " | Rating: " << d.getRating()
             << " | Status: " << (d.isAvailable() ? "Available" : "On Ride")
             << "\n";
    }
}

/* ============================================================
   RIDE BOOKING
   ============================================================ */

void bookRide() {
    if (drivers.empty()) {
        cout << "\nNo drivers registered!\n";
        return;
    }

    int userId = readInt("\nEnter User ID: ");
    if (!userIdExists(userId)) {
        cout << "User not found!\n";
        return;
    }

    cout << "Valid locations: ";
    for (const auto& n : graph) cout << "[" << n.first << "] ";
    cout << "\n";

    string pickup      = readLineAfterCin("Enter Pickup Location: ");
    string destination = readLineAfterCin("Enter Destination:     ");

    if (graph.find(pickup) == graph.end()) {
        cout << "Invalid pickup '" << pickup << "'!\n";
        return;
    }
    if (graph.find(destination) == graph.end()) {
        cout << "Invalid destination '" << destination << "'!\n";
        return;
    }

    auto result          = dijkstra(pickup, destination);
    int  shortestDist    = result.first;
    const vector<string>& path = result.second;

    if (shortestDist == -1) {
        cout << "No route between " << pickup << " and " << destination << "!\n";
        return;
    }

    cout << "\nShortest Distance: " << shortestDist << " km\n";
    cout << "Route: ";
    for (int i = 0; i < (int)path.size(); i++) {
        cout << path[i];
        if (i != (int)path.size() - 1) cout << " -> ";
    }
    cout << "\n";

    int fare = shortestDist * 20;
    cout << "Estimated Fare: Rs." << fare << "\n";

    // Fare confirmation before booking
    string confirm = readLineAfterCin("Confirm booking? (y/n): ");
    if (confirm != "y" && confirm != "Y") {
        cout << "Booking cancelled.\n";
        return;
    }

    Driver* selected = findBestDriver(pickup);
    if (selected == nullptr) {
        cout << "No available driver can reach your pickup!\n";
        return;
    }

    int driverDist = calculateDriverDistance(selected->getCurrentLocation(), pickup);
    selected->setAvailable(false);

    cout << "\nDriver Assigned: " << selected->getName()
         << " | Rating: "        << selected->getRating()
         << " | ETA: "           << driverDist << " km away\n";

    Ride newRide(
        nextRideId++,
        users[userId].getName(),
        selected->getName(),
        pickup,
        destination,
        fare
    );

    rideQueue.push(newRide);       // pushed to FIFO queue
    activeRides.push_back(newRide);

    cout << "Ride Booked! Ride ID: " << newRide.getRideId() << "\n";
    cout << "Rides waiting in queue: " << rideQueue.size() << "\n";

    saveRidesToFile();
    saveDriversToFile();
}

// Cancel an active ride and free the driver
void cancelRide() {
    if (activeRides.empty()) {
        cout << "\nNo active rides to cancel.\n";
        return;
    }

    int rideId = readInt("\nEnter Ride ID to Cancel: ");

    for (int i = 0; i < (int)activeRides.size(); i++) {
        if (activeRides[i].getRideId() == rideId) {

            // Free the driver
            const string& driverName = activeRides[i].getDriverName();
            for (auto& d : drivers)
                if (d.getName() == driverName) { d.setAvailable(true); break; }

            activeRides[i].setStatus("Cancelled");
            rideHistory.push(activeRides[i]);
            activeRides.erase(activeRides.begin() + i);

            cout << "Ride " << rideId << " cancelled. Driver freed.\n";
            saveRidesToFile();
            saveDriversToFile();
            return;
        }
    }
    cout << "Ride ID " << rideId << " not found.\n";
}

// Complete a ride, prompt for driver rating
void completeRide() {
    if (activeRides.empty()) {
        cout << "\nNo Active Rides!\n";
        return;
    }

    int rideId = readInt("\nEnter Ride ID to Complete: ");

    for (int i = 0; i < (int)activeRides.size(); i++) {
        if (activeRides[i].getRideId() == rideId) {

            const string driverName = activeRides[i].getDriverName();

            // Free the driver
            for (auto& d : drivers)
                if (d.getName() == driverName) { d.setAvailable(true); break; }

            activeRides[i].setStatus("Completed");
            rideHistory.push(activeRides[i]);
            activeRides.erase(activeRides.begin() + i);

            cout << "Ride Completed Successfully!\n";

            // Rating prompt
            cout << "Rate your driver " << driverName << " (1-5, or 0 to skip): ";
            float rating;
            cin  >> rating;
            if (rating >= 1.0f && rating <= 5.0f) {
                updateDriverRating(driverName, rating);
            } else {
                cout << "Rating skipped.\n";
            }

            // Dequeue from the ride queue (FIFO)
            if (!rideQueue.empty()) {
                rideQueue.pop();
                cout << "Next ride removed from queue. Rides still queued: "
                     << rideQueue.size() << "\n";
            }

            saveRidesToFile();
            saveDriversToFile();
            return;
        }
    }
    cout << "Ride ID " << rideId << " not found in active rides.\n";
}

void viewActiveRides() {
    if (activeRides.empty()) {
        cout << "\nNo Active Rides!\n";
        return;
    }
    cout << "\n===== ACTIVE RIDES =====\n";
    for (const auto& r : activeRides) {
        cout << "ID: "       << r.getRideId()
             << " | User: "  << r.getUserName()
             << " | Driver: "<< r.getDriverName()
             << " | From: "  << r.getPickup()
             << " | To: "    << r.getDestination()
             << " | Fare: Rs." << r.getFare()
             << " | Status: "<< r.getStatus()
             << "\n";
    }
}

void viewRideHistory() {
    if (rideHistory.empty()) {
        cout << "\nNo ride history found!\n";
        return;
    }
    stack<Ride> temp = rideHistory;
    cout << "\n===== RIDE HISTORY (Most Recent First) =====\n";
    while (!temp.empty()) {
        const Ride& r = temp.top();
        cout << "ID: "       << r.getRideId()
             << " | User: "  << r.getUserName()
             << " | Driver: "<< r.getDriverName()
             << " | From: "  << r.getPickup()
             << " | To: "    << r.getDestination()
             << " | Fare: Rs." << r.getFare()
             << " | Status: "<< r.getStatus()
             << "\n";
        temp.pop();
    }
}

// Shows the next ride waiting in the FIFO queue
void viewRideQueue() {
    if (rideQueue.empty()) {
        cout << "\nNo rides in the dispatch queue.\n";
        return;
    }
    cout << "\n===== RIDE DISPATCH QUEUE =====\n";
    cout << "Rides waiting: " << rideQueue.size() << "\n";
    cout << "Next ride to process:\n";
    const Ride& r = rideQueue.front();
    cout << "  ID: "       << r.getRideId()
         << " | User: "    << r.getUserName()
         << " | Driver: "  << r.getDriverName()
         << " | From: "    << r.getPickup()
         << " | To: "      << r.getDestination()
         << " | Fare: Rs." << r.getFare()
         << "\n";
}

/* ============================================================
   NEARBY DRIVERS — Selection Sort by distance
   ============================================================ */

void showNearbyDrivers() {
    cout << "Valid locations: ";
    for (const auto& n : graph) cout << "[" << n.first << "] ";
    cout << "\n";

    string pickup = readLineAfterCin("Enter your location: ");

    if (graph.find(pickup) == graph.end()) {
        cout << "Invalid location!\n";
        return;
    }

    // Build {distance, driver pointer} list — O(n * Dijkstra)
    vector<pair<int, Driver*>> nearby;
    for (auto& d : drivers) {
        if (!d.isAvailable()) continue;
        int dist = calculateDriverDistance(d.getCurrentLocation(), pickup);
        if (dist != -1)
            nearby.push_back({dist, &d});
    }

    if (nearby.empty()) {
        cout << "No available drivers reachable from " << pickup << ".\n";
        return;
    }

    // Selection Sort O(n^2) — by distance ascending
    selectionSortByDistance(nearby);

    cout << "\n===== NEARBY DRIVERS (Selection Sort by Distance) =====\n";
    for (const auto& p : nearby) {
        cout << "ID: "        << p.second->getId()
             << " | Name: "   << p.second->getName()
             << " | At: "     << p.second->getCurrentLocation()
             << " | Dist: "   << p.first << " km"
             << " | Rating: " << p.second->getRating()
             << "\n";
    }
}

/* ============================================================
   SEARCH FUNCTIONS — all use Linear Search O(n)
   ============================================================ */

void searchUserById() {
    int id = readInt("\nEnter User ID: ");
    // Hash map lookup O(1) average
    auto it = users.find(id);
    if (it != users.end()) {
        cout << "User Found!\n";
        cout << "ID: "   << it->second.getId()   << "\n";
        cout << "Name: " << it->second.getName() << "\n";
    } else {
        cout << "User Not Found!\n";
    }
}

void searchDriverById() {
    int id = readInt("\nEnter Driver ID: ");
    // Linear Search O(n) — drivers stored in vector
    for (const auto& d : drivers) {
        if (d.getId() == id) {
            cout << "Driver Found!\n";
            cout << "ID: "        << d.getId()              << "\n";
            cout << "Name: "      << d.getName()            << "\n";
            cout << "Location: "  << d.getCurrentLocation() << "\n";
            cout << "Rating: "    << d.getRating()
                 << " (from "     << d.getTotalRatings() << " ratings)\n";
            cout << "Available: " << (d.isAvailable() ? "Yes" : "No") << "\n";
            return;
        }
    }
    cout << "Driver Not Found!\n";
}

void searchDriverByName() {
    string name = readLineAfterCin("\nEnter Driver Name: ");
    // Linear Search O(n)
    for (const auto& d : drivers) {
        if (d.getName() == name) {
            cout << "Driver Found!\n";
            cout << "ID: "        << d.getId()              << "\n";
            cout << "Name: "      << d.getName()            << "\n";
            cout << "Location: "  << d.getCurrentLocation() << "\n";
            cout << "Rating: "    << d.getRating()
                 << " (from "     << d.getTotalRatings() << " ratings)\n";
            cout << "Available: " << (d.isAvailable() ? "Yes" : "No") << "\n";
            return;
        }
    }
    cout << "Driver Not Found!\n";
}

void searchRideById() {
    int rideId = readInt("\nEnter Ride ID: ");

    // Linear Search O(n) — active rides
    for (const auto& r : activeRides) {
        if (r.getRideId() == rideId) {
            cout << "Ride Found! (Active)\n";
            cout << "ID: "          << r.getRideId()      << "\n";
            cout << "User: "        << r.getUserName()    << "\n";
            cout << "Driver: "      << r.getDriverName()  << "\n";
            cout << "Pickup: "      << r.getPickup()      << "\n";
            cout << "Destination: " << r.getDestination() << "\n";
            cout << "Fare: Rs. "    << r.getFare()        << "\n";
            cout << "Status: "      << r.getStatus()      << "\n";
            return;
        }
    }

    // Linear Search O(n) — ride history stack
    stack<Ride> temp = rideHistory;
    while (!temp.empty()) {
        const Ride& r = temp.top();
        if (r.getRideId() == rideId) {
            cout << "Ride Found! (History)\n";
            cout << "ID: "          << r.getRideId()      << "\n";
            cout << "User: "        << r.getUserName()    << "\n";
            cout << "Driver: "      << r.getDriverName()  << "\n";
            cout << "Pickup: "      << r.getPickup()      << "\n";
            cout << "Destination: " << r.getDestination() << "\n";
            cout << "Fare: Rs. "    << r.getFare()        << "\n";
            cout << "Status: "      << r.getStatus()      << "\n";
            return;
        }
        temp.pop();
    }
    cout << "Ride Not Found!\n";
}

/* ============================================================
   FILE I/O
   ============================================================ */

void saveUsersToFile() {
    ofstream file("users.txt");
    for (const auto& u : users)
        file << u.second.getId() << "," << u.second.getName() << "\n";
    file.close();
}

void loadUsersFromFile() {
    ifstream file("users.txt");
    if (!file.is_open()) return;
    string line;
    while (getline(file, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        string id, name;
        getline(ss, id,   ',');
        getline(ss, name, ',');
        if (id.empty() || name.empty()) continue;
        int uid = stoi(id);
        users[uid] = User(uid, name);
    }
    file.close();
}

void saveDriversToFile() {
    ofstream file("drivers.txt");
    for (const auto& d : drivers) {
        file << d.getId()              << ","
             << d.getName()            << ","
             << d.getCurrentLocation() << ","
             << d.getRating()          << ","
             << d.getTotalRatings()    << ","
             << d.getRatingSum()       << ","
             << d.isAvailable()
             << "\n";
    }
    file.close();
}

void loadDriversFromFile() {
    ifstream file("drivers.txt");
    if (!file.is_open()) return;
    string line;
    while (getline(file, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        string id, name, location, rating, totalRatings, ratingSum, available;
        getline(ss, id,           ',');
        getline(ss, name,         ',');
        getline(ss, location,     ',');
        getline(ss, rating,       ',');
        getline(ss, totalRatings, ',');
        getline(ss, ratingSum,    ',');
        getline(ss, available,    ',');

        if (id.empty() || name.empty() || location.empty() ||
            rating.empty() || available.empty()) continue;

        int   tr = totalRatings.empty() ? 1 : stoi(totalRatings);
        float rs = ratingSum.empty()    ? stof(rating) : stof(ratingSum);

        Driver d(stoi(id), name, location, stof(rating), tr, rs, stoi(available));
        drivers.push_back(d);
    }
    file.close();
}

void saveRidesToFile() {
    ofstream file("rides.txt");

    // Save history
    stack<Ride> temp = rideHistory;
    while (!temp.empty()) {
        const Ride& r = temp.top();
        file << r.getRideId()      << "," << r.getUserName()    << ","
             << r.getDriverName()  << "," << r.getPickup()      << ","
             << r.getDestination() << "," << r.getFare()        << ","
             << r.getStatus()      << "\n";
        temp.pop();
    }

    // Save active rides
    for (const auto& r : activeRides) {
        file << r.getRideId()      << "," << r.getUserName()    << ","
             << r.getDriverName()  << "," << r.getPickup()      << ","
             << r.getDestination() << "," << r.getFare()        << ","
             << r.getStatus()      << "\n";
    }
    file.close();
}

void loadRidesFromFile() {
    ifstream file("rides.txt");
    if (!file.is_open()) return;
    string line;
    int    maxId = 0;

    while (getline(file, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        string rideId, userName, driverName, pickup, destination, fare, status;
        getline(ss, rideId,      ',');
        getline(ss, userName,    ',');
        getline(ss, driverName,  ',');
        getline(ss, pickup,      ',');
        getline(ss, destination, ',');
        getline(ss, fare,        ',');
        getline(ss, status,      ',');

        if (rideId.empty() || fare.empty()) continue;

        Ride r(stoi(rideId), userName, driverName, pickup, destination, stoi(fare));
        r.setStatus(status);

        int rid = stoi(rideId);
        if (rid > maxId) maxId = rid;

        if (status == "Active") {
            activeRides.push_back(r);
            rideQueue.push(r); // restore to FIFO queue as well
        } else {
            rideHistory.push(r);
        }
    }
    nextRideId = maxId + 1; // ensure no ID collision
    file.close();
}

/* ============================================================
   ADMIN DASHBOARD
   ============================================================ */

void adminDashboard() {
    cout << "\n***************************\n";
    cout << "*     ADMIN DASHBOARD      *\n";
    cout << "****************************\n";

    int   availableDrivers = 0;
    float maxRating        = -1.0f;
    string bestDriver      = "None";

    for (const auto& d : drivers) {
        if (d.isAvailable()) availableDrivers++;
        if (d.getRating() > maxRating) {
            maxRating  = d.getRating();
            bestDriver = d.getName();
        }
    }

    int totalRevenue = 0;
    stack<Ride> temp = rideHistory;
    while (!temp.empty()) {
        if (temp.top().getStatus() == "Completed")
            totalRevenue += temp.top().getFare();
        temp.pop();
    }

    cout << "  Total Users          : " << users.size()           << "\n";
    cout << "  Total Drivers        : " << drivers.size()         << "\n";
    cout << "  Available Drivers    : " << availableDrivers        << "\n";
    cout << "  Active Rides         : " << activeRides.size()     << "\n";
    cout << "  Rides in Queue       : " << rideQueue.size()       << "\n";
    cout << "  Completed Rides      : " << rideHistory.size()     << "\n";
    cout << "  Total Revenue        : Rs. " << totalRevenue       << "\n";
    cout << "  Top Rated Driver     : " << bestDriver
         << " (" << (maxRating >= 0 ? to_string(maxRating) : "N/A") << " stars)\n";
}

/* ============================================================
   SUB-MENUS
   ============================================================ */

void userMenu() {
    int choice;
    do {
        cout << "\n**************************\n";
        cout << "*       USER MENU         *\n";
        cout << "***************************\n";
        cout << "*  1. Register User       *\n";
        cout << "*  2. View All Users      *\n";
        cout << "*  3. Search User by ID   *\n";
        cout << "*  0. Back                *\n";
        cout << "***************************\n";
        choice = readInt("Choice: ");
        switch (choice) {
            case 1: registerUser();   break;
            case 2: viewUsers();      break;
            case 3: searchUserById(); break;
            case 0: break;
            default: cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

void driverMenu() {
    int choice;
    do {
        cout << "\n*******************************\n";
        cout << "*         DRIVER MENU          *\n";
        cout << "********************************\n";
        cout << "*  1. Add Driver               *\n";
        cout << "*  2. View Available Drivers   *\n";
        cout << "*  3. View All Drivers         *\n";
        cout << "*  4. Show Nearby Drivers      *\n";
        cout << "*  5. Search Driver by ID      *\n";
        cout << "*  6. Search Driver by Name    *\n";
        cout << "*  0. Back                     *\n";
        cout << "********************************\n";
        choice = readInt("Choice: ");
        switch (choice) {
            case 1: addDriver();          break;
            case 2: viewDrivers();        break;
            case 3: viewAllDrivers();     break;
            case 4: showNearbyDrivers();  break;
            case 5: searchDriverById();   break;
            case 6: searchDriverByName(); break;
            case 0: break;
            default: cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

void rideMenu() {
    int choice;
    do {
        cout << "\n*******************************\n";
        cout << "*          RIDE MENU           *\n";
        cout << "********************************\n";
        cout << "*  1. Book a Ride              *\n";
        cout << "*  2. Cancel a Ride            *\n";
        cout << "*  3. Complete a Ride          *\n";
        cout << "*  4. View Active Rides        *\n";
        cout << "*  5. View Ride Queue          *\n";
        cout << "*  6. View Ride History        *\n";
        cout << "*  7. Search Ride by ID        *\n";
        cout << "*  0. Back                     *\n";
        cout << "********************************\n";
        choice = readInt("Choice: ");
        switch (choice) {
            case 1: bookRide();       break;
            case 2: cancelRide();     break;
            case 3: completeRide();   break;
            case 4: viewActiveRides(); break;
            case 5: viewRideQueue();  break;
            case 6: viewRideHistory(); break;
            case 7: searchRideById(); break;
            case 0: break;
            default: cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

void mapMenu() {
    int choice;
    do {
        cout << "\n┌──────────────────────────────┐\n";
        cout << "│          MAP MENU            │\n";
        cout << "├──────────────────────────────┤\n";
        cout << "│  1. View City Map            │\n";
        cout << "│  2. Add New Road             │\n";
        cout << "│  0. Back                     │\n";
        cout << "└──────────────────────────────┘\n";
        choice = readInt("Choice: ");
        switch (choice) {
            case 1: viewCityMap();      break;
            case 2: addRoadFromInput(); break;
            case 0: break;
            default: cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

void adminMenu() {
    int choice;
    do {
        cout << "\n*******************************\n";
        cout << "*         ADMIN MENU           *\n";
        cout << "********************************\n";
        cout << "*  1. Admin Dashboard          *\n";
        cout << "*  2. User Management          *\n";
        cout << "*  3. Driver Management        *\n";
        cout << "*  4. Map Management           *\n";
        cout << "*  0. Back                     *\n";
        cout << "********************************\n";
        choice = readInt("Choice: ");
        switch (choice) {
            case 1: adminDashboard(); break;
            case 2: userMenu();       break;
            case 3: driverMenu();     break;
            case 4: mapMenu();        break;
            case 0: break;
            default: cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

/* ============================================================
   MAIN MENU
   ============================================================ */

#ifndef FLASK_CLI_MODE 
int main() {
    loadUsersFromFile();
    loadDriversFromFile();
    loadRidesFromFile();
    createCityMap();

    int choice;
    do {
        system("clear");
        cout << "\n************************************\n";
        cout << "*      RIDE BOOKING SIMULATOR       *\n";
        cout << "*************************************\n";
        cout << "*  RIDES                            *\n";
        cout << "*   1. Book a Ride                  *\n";
        cout << "*   2. Cancel a Ride                *\n";
        cout << "*   3. Complete a Ride              *\n";
        cout << "*   4. View Active Rides            *\n";
        cout << "*   5. View Ride Queue (FIFO)       *\n";
        cout << "*   6. View Ride History            *\n";
        cout << "*************************************\n";
        cout << "*  DRIVERS                          *\n";
        cout << "*   7. View Available Drivers       *\n";
        cout << "*   8. Show Nearby Drivers          *\n";
        cout << "*************************************\n";
        cout << "*  MAP & ROUTES                     *\n";
        cout << "*   9. View City Map                *\n";
        cout << "*************************************\n";
        cout << "*  SEARCH                           *\n";
        cout << "*  10. Search User by ID            *\n";
        cout << "*  11. Search Driver by ID          *\n";
        cout << "*  12. Search Driver by Name        *\n";
        cout << "*  13. Search Ride by ID            *\n";
        cout << "*************************************\n";
        cout << "*  ADMIN                            *\n";
        cout << "*  14. Admin Panel                  *\n";
        cout << "*************************************\n";
        cout << "*   0. Exit                         *\n";
        cout << "*************************************\n";

        choice = readInt("Enter Choice: ");

        switch (choice) {
            case  1: bookRide();          break;
            case  2: cancelRide();        break;
            case  3: completeRide();      break;
            case  4: viewActiveRides();   break;
            case  5: viewRideQueue();     break;
            case  6: viewRideHistory();   break;
            case  7: viewDrivers();       break;
            case  8: showNearbyDrivers(); break;
            case  9: viewCityMap();       break;
            case 10: searchUserById();    break;
            case 11: searchDriverById();  break;
            case 12: searchDriverByName(); break;
            case 13: searchRideById();    break;
            case 14: adminMenu();         break;
            case  0: cout << "\nThank you for using Ride Booking Simulator!\n"; break;
            default: cout << "Invalid choice. Try again.\n";
        }
        // getch();
    } while (choice != 0);

    return 0;
}
#endif