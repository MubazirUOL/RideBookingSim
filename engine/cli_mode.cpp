/*  cli_mode.cpp  —  JSON CLI entry point for the Ride Booking DSA Engine
 *
 *  Usage:
 *      ./engine <command> '<json_payload>'
 *
 *  Every command reads a JSON object from argv[2] (can be "{}") and
 *  writes a single JSON object to stdout.  stderr is reserved for
 *  fatal build / link errors only — Flask reads stdout exclusively.
 *
 *  Commands
 *  ────────
 *  System      : ping
 *  Map         : get_map, add_road, shortest_path
 *  Users       : register_user, get_users, search_user
 *  Drivers     : add_driver, get_drivers, get_all_drivers,
 *                nearby_drivers, search_driver_id, search_driver_name
 *  Rides       : book_ride, cancel_ride, complete_ride,
 *                get_active_rides, get_ride_queue, get_ride_history,
 *                search_ride
 *  Admin       : admin_dashboard
 */

/* ── pull in the entire engine (no separate compilation needed) ────────── */
/* We define FLASK_CLI_MODE so main.cpp can guard its own main() away.     */
#define FLASK_CLI_MODE
#include "main.cpp"         // brings in every class, global, and function

#include "json.hpp"
using json = nlohmann::json;

#include <iostream>
#include <string>
#include <sstream>

/* ═══════════════════════════════════════════════════════════════════════
   HELPERS
   ═══════════════════════════════════════════════════════════════════════ */

/* Serialize the graph into a JSON-friendly structure */
static json graphToJson() {
    json nodes = json::array();
    json edges = json::array();

    for (const auto& [node, neighbors] : graph) {
        nodes.push_back(node);
        for (const auto& [neighbor, dist] : neighbors) {
            /* Emit each undirected edge once (alphabetical order) */
            if (node < neighbor) {
                edges.push_back({
                    {"from",     node},
                    {"to",       neighbor},
                    {"distance", dist}
                });
            }
        }
    }
    return {{"nodes", nodes}, {"edges", edges}};
}

/* Driver → JSON */
static json driverToJson(const Driver& d) {
    return {
        {"id",              d.getId()},
        {"name",            d.getName()},
        {"location",        d.getCurrentLocation()},
        {"rating",          d.getRating()},
        {"totalRatings",    d.getTotalRatings()},
        {"available",       d.isAvailable()}
    };
}

/* User → JSON */
static json userToJson(const User& u) {
    return {{"id", u.getId()}, {"name", u.getName()}};
}

/* Ride → JSON */
static json rideToJson(const Ride& r) {
    return {
        {"rideId",      r.getRideId()},
        {"user",        r.getUserName()},
        {"driver",      r.getDriverName()},
        {"pickup",      r.getPickup()},
        {"destination", r.getDestination()},
        {"fare",        r.getFare()},
        {"status",      r.getStatus()}
    };
}

/* Convenience error/ok builders */
static json err(const std::string& msg) {
    return {{"success", false}, {"error", msg}};
}
static json ok(json payload = {}) {
    payload["success"] = true;
    return payload;
}

/* ═══════════════════════════════════════════════════════════════════════
   COMMAND HANDLERS
   Each function receives the parsed input JSON and returns a result JSON.
   ═══════════════════════════════════════════════════════════════════════ */

/* ── System ─────────────────────────────────────────────────────────── */

json cmd_ping(const json&) {
    return ok({{"message", "DSA engine is running"}});
}

/* ── Map ─────────────────────────────────────────────────────────────── */

json cmd_get_map(const json&) {
    return ok(graphToJson());
}

json cmd_add_road(const json& in) {
    if (!in.contains("from") || !in.contains("to") || !in.contains("distance"))
        return err("Required fields: from, to, distance");

    std::string u    = in["from"];
    std::string v    = in["to"];
    int         dist = in["distance"];

    if (dist <= 0)
        return err("Distance must be positive");

    addRoad(u, v, dist);

    return ok({
        {"message",  "Road added"},
        {"from",     u},
        {"to",       v},
        {"distance", dist},
        {"map",      graphToJson()}
    });
}

json cmd_shortest_path(const json& in) {
    if (!in.contains("from") || !in.contains("to"))
        return err("Required fields: from, to");

    std::string src  = in["from"];
    std::string dest = in["to"];

    if (graph.find(src) == graph.end())
        return err("Unknown location: " + src);
    if (graph.find(dest) == graph.end())
        return err("Unknown location: " + dest);

    auto [dist, path] = dijkstra(src, dest);

    if (dist == -1)
        return err("No route between " + src + " and " + dest);

    return ok({
        {"distance", dist},
        {"path",     path},
        {"fare",     dist * 20}
    });
}

/* ── Users ───────────────────────────────────────────────────────────── */

json cmd_register_user(const json& in) {
    if (!in.contains("id") || !in.contains("name"))
        return err("Required fields: id, name");

    int         id   = in["id"];
    std::string name = in["name"];

    if (name.empty())
        return err("Name cannot be empty");
    if (userIdExists(id))
        return err("User ID " + std::to_string(id) + " already exists");

    users[id] = User(id, name);
    saveUsersToFile();

    return ok({{"user", userToJson(users[id])}});
}

json cmd_get_users(const json&) {
    json list = json::array();
    for (const auto& [id, u] : users)
        list.push_back(userToJson(u));
    return ok({{"users", list}, {"count", (int)users.size()}});
}

json cmd_search_user(const json& in) {
    if (!in.contains("id"))
        return err("Required field: id");

    int id = in["id"];
    auto it = users.find(id);
    if (it == users.end())
        return err("User not found");

    return ok({{"user", userToJson(it->second)}});
}

/* ── Drivers ─────────────────────────────────────────────────────────── */

json cmd_add_driver(const json& in) {
    if (!in.contains("id") || !in.contains("name") ||
        !in.contains("location") || !in.contains("rating"))
        return err("Required fields: id, name, location, rating");

    int         id       = in["id"];
    std::string name     = in["name"];
    std::string location = in["location"];
    float       rating   = in["rating"];

    if (name.empty())
        return err("Name cannot be empty");
    if (driverIdExists(id))
        return err("Driver ID " + std::to_string(id) + " already exists");
    if (graph.find(location) == graph.end())
        return err("Invalid location: " + location);
    if (rating < 0.0f || rating > 5.0f)
        return err("Rating must be 0.0 – 5.0");

    drivers.push_back(Driver(id, name, location, rating));
    saveDriversToFile();

    return ok({{"driver", driverToJson(drivers.back())}});
}

json cmd_get_drivers(const json&) {
    /* Available drivers sorted by rating (Insertion Sort — your DSA impl) */
    std::vector<Driver> sorted = drivers;
    insertionSortByRating(sorted);

    json list = json::array();
    for (const auto& d : sorted)
        if (d.isAvailable())
            list.push_back(driverToJson(d));

    return ok({
        {"drivers",   list},
        {"count",     (int)list.size()},
        {"sortedBy",  "rating (insertion sort)"}
    });
}

json cmd_get_all_drivers(const json&) {
    std::vector<Driver> sorted = drivers;
    insertionSortByRating(sorted);

    json list = json::array();
    for (const auto& d : sorted)
        list.push_back(driverToJson(d));

    return ok({
        {"drivers",  list},
        {"count",    (int)list.size()},
        {"sortedBy", "rating (insertion sort)"}
    });
}

json cmd_nearby_drivers(const json& in) {
    if (!in.contains("location"))
        return err("Required field: location");

    std::string pickup = in["location"];
    if (graph.find(pickup) == graph.end())
        return err("Invalid location: " + pickup);

    /* Build distance list — O(n × Dijkstra) */
    std::vector<std::pair<int, Driver*>> nearby;
    for (auto& d : drivers) {
        if (!d.isAvailable()) continue;
        int dist = calculateDriverDistance(d.getCurrentLocation(), pickup);
        if (dist != -1)
            nearby.push_back({dist, &d});
    }

    /* Selection Sort — your DSA impl */
    selectionSortByDistance(nearby);

    json list = json::array();
    for (const auto& [dist, dp] : nearby) {
        json entry = driverToJson(*dp);
        entry["distanceKm"] = dist;
        list.push_back(entry);
    }

    return ok({
        {"drivers",  list},
        {"count",    (int)list.size()},
        {"from",     pickup},
        {"sortedBy", "distance (selection sort)"}
    });
}

json cmd_search_driver_id(const json& in) {
    if (!in.contains("id"))
        return err("Required field: id");

    int id = in["id"];
    for (const auto& d : drivers)
        if (d.getId() == id)
            return ok({{"driver", driverToJson(d)}});

    return err("Driver not found");
}

json cmd_search_driver_name(const json& in) {
    if (!in.contains("name"))
        return err("Required field: name");

    std::string name = in["name"];
    for (const auto& d : drivers)
        if (d.getName() == name)
            return ok({{"driver", driverToJson(d)}});

    return err("Driver not found");
}

/* ── Rides ───────────────────────────────────────────────────────────── */

json cmd_book_ride(const json& in) {
    if (!in.contains("userId") || !in.contains("pickup") || !in.contains("destination"))
        return err("Required fields: userId, pickup, destination");

    int         userId      = in["userId"];
    std::string pickup      = in["pickup"];
    std::string destination = in["destination"];

    if (!userIdExists(userId))
        return err("User not found");
    if (graph.find(pickup) == graph.end())
        return err("Invalid pickup: " + pickup);
    if (graph.find(destination) == graph.end())
        return err("Invalid destination: " + destination);

    /* Shortest path (Dijkstra) */
    auto [shortestDist, path] = dijkstra(pickup, destination);
    if (shortestDist == -1)
        return err("No route between " + pickup + " and " + destination);

    int fare = shortestDist * 20;

    /* Best driver (weighted score) */
    Driver* selected = findBestDriver(pickup);
    if (!selected)
        return err("No available driver can reach your pickup location");

    int driverDist = calculateDriverDistance(selected->getCurrentLocation(), pickup);
    selected->setAvailable(false);

    Ride newRide(
        nextRideId++,
        users[userId].getName(),
        selected->getName(),
        pickup,
        destination,
        fare
    );

    rideQueue.push(newRide);
    activeRides.push_back(newRide);

    saveRidesToFile();
    saveDriversToFile();

    return ok({
        {"ride",          rideToJson(newRide)},
        {"route",         path},
        {"distanceKm",    shortestDist},
        {"fare",          fare},
        {"driver",        driverToJson(*selected)},
        {"driverEtaKm",   driverDist},
        {"queueSize",     (int)rideQueue.size()}
    });
}

json cmd_cancel_ride(const json& in) {
    if (!in.contains("rideId"))
        return err("Required field: rideId");

    int rideId = in["rideId"];

    for (int i = 0; i < (int)activeRides.size(); i++) {
        if (activeRides[i].getRideId() == rideId) {
            /* Free driver */
            const std::string& driverName = activeRides[i].getDriverName();
            for (auto& d : drivers)
                if (d.getName() == driverName) { d.setAvailable(true); break; }

            activeRides[i].setStatus("Cancelled");
            json cancelledRide = rideToJson(activeRides[i]);
            rideHistory.push(activeRides[i]);
            activeRides.erase(activeRides.begin() + i);

            saveRidesToFile();
            saveDriversToFile();

            return ok({
                {"ride",    cancelledRide},
                {"message", "Ride cancelled and driver freed"}
            });
        }
    }
    return err("Ride ID " + std::to_string(rideId) + " not found in active rides");
}

json cmd_complete_ride(const json& in) {
    if (!in.contains("rideId"))
        return err("Required field: rideId");

    int   rideId = in["rideId"];
    float rating = in.value("rating", -1.0f);   /* optional */

    for (int i = 0; i < (int)activeRides.size(); i++) {
        if (activeRides[i].getRideId() == rideId) {
            const std::string driverName = activeRides[i].getDriverName();

            /* Free driver */
            for (auto& d : drivers)
                if (d.getName() == driverName) { d.setAvailable(true); break; }

            activeRides[i].setStatus("Completed");
            json completedRide = rideToJson(activeRides[i]);
            rideHistory.push(activeRides[i]);
            activeRides.erase(activeRides.begin() + i);

            /* Apply rating if valid */
            float newAvg = -1.0f;
            if (rating >= 1.0f && rating <= 5.0f) {
                for (auto& d : drivers) {
                    if (d.getName() == driverName) {
                        d.addRating(rating);
                        newAvg = d.getRating();
                        break;
                    }
                }
            }

            if (!rideQueue.empty()) rideQueue.pop();

            saveRidesToFile();
            saveDriversToFile();

            json res = {
                {"ride",       completedRide},
                {"message",    "Ride completed successfully"},
                {"queueSize",  (int)rideQueue.size()}
            };
            if (newAvg >= 0) res["driverNewRating"] = newAvg;

            return ok(res);
        }
    }
    return err("Ride ID " + std::to_string(rideId) + " not found in active rides");
}

json cmd_get_active_rides(const json&) {
    json list = json::array();
    for (const auto& r : activeRides)
        list.push_back(rideToJson(r));
    return ok({{"rides", list}, {"count", (int)list.size()}});
}

json cmd_get_ride_queue(const json&) {
    if (rideQueue.empty())
        return ok({{"rides", json::array()}, {"count", 0}});

    /* snapshot the front without destroying the queue */
    json front = rideToJson(rideQueue.front());
    return ok({
        {"next",  front},
        {"count", (int)rideQueue.size()}
    });
}

json cmd_get_ride_history(const json&) {
    /* Stack → array (most-recent first) */
    json list = json::array();
    std::stack<Ride> temp = rideHistory;
    while (!temp.empty()) {
        list.push_back(rideToJson(temp.top()));
        temp.pop();
    }
    return ok({{"rides", list}, {"count", (int)list.size()}});
}

json cmd_search_ride(const json& in) {
    if (!in.contains("rideId"))
        return err("Required field: rideId");

    int rideId = in["rideId"];

    /* Check active rides first */
    for (const auto& r : activeRides)
        if (r.getRideId() == rideId)
            return ok({{"ride", rideToJson(r)}, {"source", "active"}});

    /* Then ride history stack */
    std::stack<Ride> temp = rideHistory;
    while (!temp.empty()) {
        if (temp.top().getRideId() == rideId)
            return ok({{"ride", rideToJson(temp.top())}, {"source", "history"}});
        temp.pop();
    }

    return err("Ride " + std::to_string(rideId) + " not found");
}

/* ── Admin ───────────────────────────────────────────────────────────── */

json cmd_admin_dashboard(const json&) {
    int   availableDrivers = 0;
    float maxRating        = -1.0f;
    std::string bestDriver = "None";

    for (const auto& d : drivers) {
        if (d.isAvailable()) availableDrivers++;
        if (d.getRating() > maxRating) {
            maxRating  = d.getRating();
            bestDriver = d.getName();
        }
    }

    int totalRevenue = 0;
    std::stack<Ride> temp = rideHistory;
    while (!temp.empty()) {
        if (temp.top().getStatus() == "Completed")
            totalRevenue += temp.top().getFare();
        temp.pop();
    }

    return ok({
        {"totalUsers",        (int)users.size()},
        {"totalDrivers",      (int)drivers.size()},
        {"availableDrivers",  availableDrivers},
        {"activeRides",       (int)activeRides.size()},
        {"ridesInQueue",      (int)rideQueue.size()},
        {"completedRides",    (int)rideHistory.size()},
        {"totalRevenue",      totalRevenue},
        {"topRatedDriver",    bestDriver},
        {"topRating",         maxRating >= 0 ? maxRating : 0.0f}
    });
}

/* ═══════════════════════════════════════════════════════════════════════
   DISPATCH TABLE  —  maps command strings → handler functions
   ═══════════════════════════════════════════════════════════════════════ */

using Handler = json(*)(const json&);

static const std::unordered_map<std::string, Handler> COMMANDS = {
    /* system */
    {"ping",                cmd_ping},
    /* map */
    {"get_map",             cmd_get_map},
    {"add_road",            cmd_add_road},
    {"shortest_path",       cmd_shortest_path},
    /* users */
    {"register_user",       cmd_register_user},
    {"get_users",           cmd_get_users},
    {"search_user",         cmd_search_user},
    /* drivers */
    {"add_driver",          cmd_add_driver},
    {"get_drivers",         cmd_get_drivers},
    {"get_all_drivers",     cmd_get_all_drivers},
    {"nearby_drivers",      cmd_nearby_drivers},
    {"search_driver_id",    cmd_search_driver_id},
    {"search_driver_name",  cmd_search_driver_name},
    /* rides */
    {"book_ride",           cmd_book_ride},
    {"cancel_ride",         cmd_cancel_ride},
    {"complete_ride",       cmd_complete_ride},
    {"get_active_rides",    cmd_get_active_rides},
    {"get_ride_queue",      cmd_get_ride_queue},
    {"get_ride_history",    cmd_get_ride_history},
    {"search_ride",         cmd_search_ride},
    /* admin */
    {"admin_dashboard",     cmd_admin_dashboard},
};

/* ═══════════════════════════════════════════════════════════════════════
   ENTRY POINT
   ═══════════════════════════════════════════════════════════════════════ */

int main(int argc, char* argv[]) {
    /* Load persisted state exactly like the console app does */
    loadUsersFromFile();
    loadDriversFromFile();
    loadRidesFromFile();
    createCityMap();

    if (argc < 2) {
        std::cout << err("Usage: engine <command> [json_payload]").dump() << "\n";
        return 1;
    }

    std::string command = argv[1];

    /* Parse payload (default to empty object if omitted) */
    json payload = json::object();
    if (argc >= 3) {
        try {
            payload = json::parse(argv[2]);
        } catch (const json::parse_error& e) {
            std::cout << err(std::string("Invalid JSON payload: ") + e.what()).dump() << "\n";
            return 1;
        }
    }

    /* Dispatch */
    auto it = COMMANDS.find(command);
    if (it == COMMANDS.end()) {
        json available = json::array();
        for (const auto& [k, _] : COMMANDS) available.push_back(k);
        std::cout << json({
            {"success",  false},
            {"error",    "Unknown command: " + command},
            {"commands", available}
        }).dump() << "\n";
        return 1;
    }

    try {
        json result = it->second(payload);
        std::cout << result.dump() << "\n";
    } catch (const std::exception& e) {
        std::cout << err(std::string("Engine error: ") + e.what()).dump() << "\n";
        return 1;
    }

    return 0;
}
