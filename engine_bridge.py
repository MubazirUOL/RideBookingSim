"""
engine_bridge.py  —  Flask ↔ C++ DSA Engine communication layer

Every Flask route calls one function from this file.
This module handles:
  - Locating and validating the compiled binary
  - Serialising Python dicts → JSON argv
  - Running the subprocess with a timeout
  - Deserialising stdout JSON → Python dicts
  - Uniform error wrapping so Flask routes never crash

Usage in app.py:
    from engine_bridge import engine

    result = engine.ping()
    result = engine.book_ride(userId=1, pickup="A", destination="D")
    result = engine.get_drivers()
"""

import subprocess
import json
import os
import logging
from pathlib import Path
from typing import Any

# ─── Configuration ────────────────────────────────────────────────────────────

# Path to the compiled C++ binary — adjust if your layout differs
_BASE_DIR  = Path(__file__).resolve().parent
ENGINE_BIN = os.environ.get(
    "ENGINE_BIN",
    str(_BASE_DIR / "engine" / "build" / "engine.exe")
)

TIMEOUT_SECONDS = 10   # max time per C++ call

logger = logging.getLogger(__name__)


# ─── Core subprocess caller ────────────────────────────────────────────────────

def _call(command: str, payload: dict | None = None) -> dict:
    """
    Run: ./engine <command> '<json>'
    Returns the parsed JSON dict from stdout.
    On any failure returns: {"success": False, "error": "<reason>"}
    """
    if not os.path.isfile(ENGINE_BIN):
        logger.error("Engine binary not found at: %s", ENGINE_BIN)
        return {
            "success": False,
            "error": f"Engine binary not found. Expected: {ENGINE_BIN}"
        }

    args = [ENGINE_BIN, command]
    if payload:
        args.append(json.dumps(payload))

    logger.debug("Engine call → %s %s", command, payload or {})

    try:
        result = subprocess.run(
            args,
            capture_output=True,
            text=True,
            timeout=TIMEOUT_SECONDS,
            # Run from base dir so flat-file I/O (users.txt etc.) lands there
            cwd=str(_BASE_DIR)
        )

        if result.returncode != 0 and not result.stdout.strip():
            # Binary crashed before printing anything
            return {
                "success": False,
                "error": f"Engine exited with code {result.returncode}",
                "stderr": result.stderr.strip()
            }

        response = json.loads(result.stdout.strip())
        logger.debug("Engine response ← %s", response)
        return response

    except subprocess.TimeoutExpired:
        logger.error("Engine timed out on command: %s", command)
        return {"success": False, "error": "Engine timed out"}

    except json.JSONDecodeError as e:
        logger.error("Engine returned invalid JSON for %s: %s", command, e)
        return {
            "success": False,
            "error": "Engine returned invalid JSON",
            "raw": result.stdout[:200] if result else ""
        }

    except Exception as e:
        logger.exception("Unexpected error calling engine command: %s", command)
        return {"success": False, "error": str(e)}


# ─── Engine API class ──────────────────────────────────────────────────────────

class RideEngine:
    """
    Typed wrapper around every CLI command.
    All methods return a dict with at least {"success": bool}.
    On success extra keys carry the data.
    On failure {"success": False, "error": "..."}.
    """

    # ── System ────────────────────────────────────────────────────────────────

    def ping(self) -> dict:
        """Health-check the engine binary."""
        return _call("ping")

    # ── Map ───────────────────────────────────────────────────────────────────

    def get_map(self) -> dict:
        """
        Returns:
            nodes: list[str]
            edges: list[{from, to, distance}]
        """
        return _call("get_map")

    def add_road(self, from_loc: str, to_loc: str, distance: int) -> dict:
        """
        Add a bidirectional road to the city graph at runtime.
        Returns the updated map on success.
        """
        return _call("add_road", {
            "from":     from_loc,
            "to":       to_loc,
            "distance": distance
        })

    def shortest_path(self, from_loc: str, to_loc: str) -> dict:
        """
        Dijkstra shortest path between two map nodes.
        Returns:
            distance: int (km)
            path:     list[str]
            fare:     int (Rs.)
        """
        return _call("shortest_path", {"from": from_loc, "to": to_loc})

    # ── Users ──────────────────────────────────────────────────────────────────

    def register_user(self, user_id: int, name: str) -> dict:
        """
        Register a new user.
        Returns: {user: {id, name}}
        """
        return _call("register_user", {"id": user_id, "name": name})

    def get_users(self) -> dict:
        """
        Returns:
            users: list[{id, name}]
            count: int
        """
        return _call("get_users")

    def search_user(self, user_id: int) -> dict:
        """
        Returns: {user: {id, name}}
        """
        return _call("search_user", {"id": user_id})

    # ── Drivers ────────────────────────────────────────────────────────────────

    def add_driver(self, driver_id: int, name: str,
                   location: str, rating: float) -> dict:
        """
        Register a new driver.
        Returns: {driver: {id, name, location, rating, totalRatings, available}}
        """
        return _call("add_driver", {
            "id":       driver_id,
            "name":     name,
            "location": location,
            "rating":   rating
        })

    def get_drivers(self) -> dict:
        """
        Available drivers only, sorted by rating (insertion sort in C++).
        Returns:
            drivers:  list[driver]
            count:    int
            sortedBy: str
        """
        return _call("get_drivers")

    def get_all_drivers(self) -> dict:
        """
        All drivers (available + on-ride), sorted by rating.
        Returns same shape as get_drivers.
        """
        return _call("get_all_drivers")

    def nearby_drivers(self, location: str) -> dict:
        """
        Available drivers reachable from `location`, sorted by distance
        (selection sort in C++).
        Returns:
            drivers:  list[driver + distanceKm]
            count:    int
            from:     str
            sortedBy: str
        """
        return _call("nearby_drivers", {"location": location})

    def search_driver_by_id(self, driver_id: int) -> dict:
        """Returns: {driver: {...}}"""
        return _call("search_driver_id", {"id": driver_id})

    def search_driver_by_name(self, name: str) -> dict:
        """Returns: {driver: {...}}"""
        return _call("search_driver_name", {"name": name})

    # ── Rides ──────────────────────────────────────────────────────────────────

    def book_ride(self, user_id: int, pickup: str, destination: str) -> dict:
        """
        Book a ride. Runs Dijkstra + best-driver scoring in C++.
        Returns:
            ride:        {rideId, user, driver, pickup, destination, fare, status}
            route:       list[str]
            distanceKm:  int
            fare:        int
            driver:      driver object
            driverEtaKm: int
            queueSize:   int
        """
        return _call("book_ride", {
            "userId":      user_id,
            "pickup":      pickup,
            "destination": destination
        })

    def cancel_ride(self, ride_id: int) -> dict:
        """
        Cancel an active ride and free the driver.
        Returns: {ride: {...}, message: str}
        """
        return _call("cancel_ride", {"rideId": ride_id})

    def complete_ride(self, ride_id: int, rating: float | None = None) -> dict:
        """
        Mark a ride as completed and optionally rate the driver (1.0–5.0).
        Returns:
            ride:            {...}
            message:         str
            queueSize:       int
            driverNewRating: float (only if rating was given)
        """
        payload: dict[str, Any] = {"rideId": ride_id}
        if rating is not None:
            payload["rating"] = rating
        return _call("complete_ride", payload)

    def get_active_rides(self) -> dict:
        """
        Returns:
            rides: list[ride]
            count: int
        """
        return _call("get_active_rides")

    def get_ride_queue(self) -> dict:
        """
        FIFO dispatch queue state.
        Returns:
            next:  ride (front of queue)
            count: int
        """
        return _call("get_ride_queue")

    def get_ride_history(self) -> dict:
        """
        Completed/cancelled rides, most-recent first (stack in C++).
        Returns:
            rides: list[ride]
            count: int
        """
        return _call("get_ride_history")

    def search_ride(self, ride_id: int) -> dict:
        """
        Search active rides then history.
        Returns: {ride: {...}, source: "active"|"history"}
        """
        return _call("search_ride", {"rideId": ride_id})

    # ── Admin ──────────────────────────────────────────────────────────────────

    def admin_dashboard(self) -> dict:
        """
        Aggregated stats snapshot.
        Returns:
            totalUsers, totalDrivers, availableDrivers,
            activeRides, ridesInQueue, completedRides,
            totalRevenue, topRatedDriver, topRating
        """
        return _call("admin_dashboard")


# ─── Singleton used by all Flask routes ───────────────────────────────────────

engine = RideEngine()


# ─── Standalone test (run: python engine_bridge.py) ───────────────────────────

if __name__ == "__main__":
    import sys

    logging.basicConfig(level=logging.DEBUG)

    tests = [
        ("ping",              engine.ping),
        ("get_map",           engine.get_map),
        ("shortest_path A→E", lambda: engine.shortest_path("A", "E")),
        ("register_user",     lambda: engine.register_user(99, "Test User")),
        ("get_users",         engine.get_users),
        ("add_driver",        lambda: engine.add_driver(99, "Test Driver", "A", 4.2)),
        ("get_drivers",       engine.get_drivers),
        ("nearby_drivers B",  lambda: engine.nearby_drivers("B")),
        ("book_ride",         lambda: engine.book_ride(99, "A", "D")),
        ("get_active_rides",  engine.get_active_rides),
        ("get_ride_queue",    engine.get_ride_queue),
        ("admin_dashboard",   engine.admin_dashboard),
        ("complete_ride",     lambda: engine.complete_ride(1, rating=4.5)),
        ("get_ride_history",  engine.get_ride_history),
    ]

    passed = failed = 0
    for name, fn in tests:
        result = fn()
        status = "✓" if result.get("success") else "✗"
        if result.get("success"):
            passed += 1
        else:
            failed += 1
        print(f"  {status} {name}")
        if not result.get("success"):
            print(f"      ERROR: {result.get('error')}")

    print(f"\n{passed} passed, {failed} failed")
    sys.exit(0 if failed == 0 else 1)
