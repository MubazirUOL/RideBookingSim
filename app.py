"""
app.py  —  Flask Web Application for Ride Booking DSA Engine

Routes
──────
GET  /                     → home / dashboard
GET  /map                  → city map + shortest path form
POST /map/path             → run Dijkstra, show result
POST /map/add-road         → add new road to graph

GET  /users                → list all users
GET  /users/register       → register form
POST /users/register       → submit registration
GET  /users/search         → search form
POST /users/search         → search by ID

GET  /drivers              → available drivers (insertion-sorted)
GET  /drivers/all          → all drivers
GET  /drivers/add          → add driver form
POST /drivers/add          → submit new driver
GET  /drivers/nearby       → nearby drivers form
POST /drivers/nearby       → show nearby (selection-sorted)
GET  /drivers/search       → search form
POST /drivers/search       → search by id or name

GET  /rides                → active rides
GET  /rides/book           → booking form
POST /rides/book           → submit booking (runs Dijkstra + scoring)
POST /rides/<id>/cancel    → cancel a ride
GET  /rides/<id>/complete  → complete form (rating)
POST /rides/<id>/complete  → submit completion + rating
GET  /rides/queue          → FIFO queue view
GET  /rides/history        → stack-based history
GET  /rides/search         → search form
POST /rides/search         → search by ID

GET  /admin                → admin dashboard
"""

from flask import (
    Flask, render_template, request,
    redirect, url_for, flash, jsonify, session
)
from functools import wraps
from engine_bridge import engine
from auth import verify_login, register_account, get_all_accounts

app = Flask(__name__)
app.secret_key = "dsa-ride-booking-secret-key-change-in-production"


# ═══════════════════════════════════════════════════════════════════════════════
#   AUTH HELPERS
# ═══════════════════════════════════════════════════════════════════════════════

def login_required(f):
    """Decorator: redirect to login page if the user is not authenticated."""
    @wraps(f)
    def decorated(*args, **kwargs):
        if not session.get("logged_in"):
            flash("Please log in to access this page.", "warning")
            return redirect(url_for("login"))
        return f(*args, **kwargs)
    return decorated


# ═══════════════════════════════════════════════════════════════════════════════
#   LOGIN / LOGOUT / SIGNUP
# ═══════════════════════════════════════════════════════════════════════════════

@app.route("/login", methods=["GET", "POST"])
def login():
    if session.get("logged_in"):
        return redirect(url_for("home"))

    if request.method == "POST":
        username = request.form.get("username", "").strip()
        password = request.form.get("password", "").strip()

        if verify_login(username, password):
            session["logged_in"] = True
            session["username"]   = username
            flash(f"Welcome back, {username}!", "success")
            return redirect(url_for("home"))
        else:
            flash("Invalid username or password.", "danger")
            return render_template("login.html", prefill_username=username)

    return render_template("login.html")


@app.route("/logout")
def logout():
    username = session.get("username", "")
    session.clear()
    flash(f"You have been logged out, {username}.", "success")
    return redirect(url_for("login"))


@app.route("/signup", methods=["GET", "POST"])
def signup():
    if session.get("logged_in"):
        return redirect(url_for("home"))

    if request.method == "POST":
        username         = request.form.get("username", "").strip()
        password         = request.form.get("password", "").strip()
        confirm_password = request.form.get("confirm_password", "").strip()

        if password != confirm_password:
            flash("Passwords do not match.", "danger")
            return render_template("signup.html", prefill_username=username)

        success, message = register_account(username, password)
        if success:
            flash(message + " You can now log in.", "success")
            return redirect(url_for("login"))
        else:
            flash(message, "danger")
            return render_template("signup.html", prefill_username=username)

    return render_template("signup.html")


# ═══════════════════════════════════════════════════════════════════════════════
#   CONTEXT PROCESSORS  —  inject map nodes into every template
# ═══════════════════════════════════════════════════════════════════════════════

@app.context_processor
def inject_map_nodes():
    """Makes `map_nodes` available in every Jinja2 template."""
    result = engine.get_map()
    nodes = sorted(result.get("nodes", [])) if result.get("success") else []
    return {"map_nodes": nodes}


# ═══════════════════════════════════════════════════════════════════════════════
#   HOME
# ═══════════════════════════════════════════════════════════════════════════════

@app.route("/")
@login_required
def home():
    """Landing page — shows admin dashboard stats."""
    stats = engine.admin_dashboard()
    return render_template("index.html", stats=stats)


# ═══════════════════════════════════════════════════════════════════════════════
#   MAP
# ═══════════════════════════════════════════════════════════════════════════════

@app.route("/map")
@login_required
def map_view():
    result = engine.get_map()
    if not result["success"]:
        flash(result["error"], "danger")
        return render_template("map.html", nodes=[], edges=[], path_result=None)

    return render_template(
        "map.html",
        nodes     = sorted(result["nodes"]),
        edges     = result["edges"],
        path_result = None
    )


@app.route("/map/path", methods=["POST"])
@login_required
def map_path():
    from_loc = request.form.get("from", "").strip()
    to_loc   = request.form.get("to",   "").strip()

    if not from_loc or not to_loc:
        flash("Please select both locations.", "warning")
        return redirect(url_for("map_view"))

    path_result = engine.shortest_path(from_loc, to_loc)
    map_data    = engine.get_map()

    if not path_result["success"]:
        flash(path_result["error"], "danger")

    return render_template(
        "map.html",
        nodes       = sorted(map_data.get("nodes", [])),
        edges       = map_data.get("edges", []),
        path_result = path_result,
        from_loc    = from_loc,
        to_loc      = to_loc
    )


@app.route("/map/add-road", methods=["POST"])
@login_required
def map_add_road():
    from_loc = request.form.get("from", "").strip()
    to_loc   = request.form.get("to",   "").strip()
    try:
        distance = int(request.form.get("distance", 0))
    except ValueError:
        flash("Distance must be a number.", "danger")
        return redirect(url_for("map_view"))

    result = engine.add_road(from_loc, to_loc, distance)
    if result["success"]:
        flash(f"Road added: {from_loc} ↔ {to_loc} ({distance} km)", "success")
    else:
        flash(result["error"], "danger")

    return redirect(url_for("map_view"))


# ═══════════════════════════════════════════════════════════════════════════════
#   USERS
# ═══════════════════════════════════════════════════════════════════════════════

@app.route("/users")
@login_required
def users_list():
    result = engine.get_users()
    users  = result.get("users", []) if result["success"] else []
    return render_template("users.html", users=users)


@app.route("/users/register", methods=["GET", "POST"])
@login_required
def users_register():
    if request.method == "GET":
        return render_template("users_register.html")

    try:
        user_id = int(request.form["userId"])
    except (ValueError, KeyError):
        flash("User ID must be a number.", "danger")
        return render_template("users_register.html")

    name   = request.form.get("name", "").strip()
    result = engine.register_user(user_id, name)

    if result["success"]:
        flash(f"User '{name}' registered successfully!", "success")
        return redirect(url_for("users_list"))

    flash(result["error"], "danger")
    return render_template("users_register.html")


@app.route("/users/search", methods=["GET", "POST"])
@login_required
def users_search():
    found = None
    if request.method == "POST":
        try:
            user_id = int(request.form["userId"])
            result  = engine.search_user(user_id)
            if result["success"]:
                found = result["user"]
            else:
                flash(result["error"], "warning")
        except (ValueError, KeyError):
            flash("Please enter a valid numeric ID.", "danger")

    return render_template("users_search.html", found=found)


# ═══════════════════════════════════════════════════════════════════════════════
#   DRIVERS
# ═══════════════════════════════════════════════════════════════════════════════

@app.route("/drivers")
@login_required
def drivers_available():
    result  = engine.get_drivers()
    drivers = result.get("drivers", []) if result["success"] else []
    return render_template("drivers.html", drivers=drivers,
                           title="Available Drivers",
                           subtitle="Sorted by rating · insertion sort (DSA)")


@app.route("/drivers/all")
@login_required
def drivers_all():
    result  = engine.get_all_drivers()
    drivers = result.get("drivers", []) if result["success"] else []
    return render_template("drivers.html", drivers=drivers,
                           title="All Drivers",
                           subtitle="Sorted by rating · insertion sort (DSA)")


@app.route("/drivers/add", methods=["GET", "POST"])
@login_required
def drivers_add():
    if request.method == "GET":
        return render_template("drivers_add.html")

    try:
        driver_id = int(request.form["driverId"])
        rating    = float(request.form["rating"])
    except (ValueError, KeyError):
        flash("ID must be an integer and rating a decimal.", "danger")
        return render_template("drivers_add.html")

    name     = request.form.get("name",     "").strip()
    location = request.form.get("location", "").strip()
    result   = engine.add_driver(driver_id, name, location, rating)

    if result["success"]:
        flash(f"Driver '{name}' added successfully!", "success")
        return redirect(url_for("drivers_available"))

    flash(result["error"], "danger")
    return render_template("drivers_add.html")


@app.route("/drivers/nearby", methods=["GET", "POST"])
@login_required
def drivers_nearby():
    nearby = None
    from_loc = ""
    if request.method == "POST":
        from_loc = request.form.get("location", "").strip()
        result   = engine.nearby_drivers(from_loc)
        if result["success"]:
            nearby = result.get("drivers", [])
        else:
            flash(result["error"], "danger")

    return render_template("drivers_nearby.html", nearby=nearby, from_loc=from_loc)


@app.route("/drivers/search", methods=["GET", "POST"])
@login_required
def drivers_search():
    found = None
    if request.method == "POST":
        search_by = request.form.get("searchBy", "id")
        try:
            if search_by == "id":
                driver_id = int(request.form["query"])
                result    = engine.search_driver_by_id(driver_id)
            else:
                name   = request.form.get("query", "").strip()
                result = engine.search_driver_by_name(name)

            if result["success"]:
                found = result["driver"]
            else:
                flash(result["error"], "warning")
        except (ValueError, KeyError):
            flash("Invalid search input.", "danger")

    return render_template("drivers_search.html", found=found)


# ═══════════════════════════════════════════════════════════════════════════════
#   RIDES
# ═══════════════════════════════════════════════════════════════════════════════

@app.route("/rides")
@login_required
def rides_active():
    result = engine.get_active_rides()
    rides  = result.get("rides", []) if result["success"] else []
    return render_template("rides_active.html", rides=rides)


@app.route("/rides/book", methods=["GET", "POST"])
@login_required
def rides_book():
    if request.method == "GET":
        users_result = engine.get_users()
        users        = users_result.get("users", []) if users_result["success"] else []
        return render_template("rides_book.html", users=users, booking=None)

    # POST — submit booking
    try:
        user_id = int(request.form["userId"])
    except (ValueError, KeyError):
        flash("Please select a valid user.", "danger")
        return redirect(url_for("rides_book"))

    pickup      = request.form.get("pickup",      "").strip()
    destination = request.form.get("destination", "").strip()

    result = engine.book_ride(user_id, pickup, destination)

    if not result["success"]:
        flash(result["error"], "danger")
        return redirect(url_for("rides_book"))

    # Success — show booking confirmation
    users_result = engine.get_users()
    users        = users_result.get("users", []) if users_result["success"] else []
    return render_template("rides_book.html", users=users, booking=result)


@app.route("/rides/<int:ride_id>/cancel", methods=["POST"])
@login_required
def rides_cancel(ride_id):
    result = engine.cancel_ride(ride_id)
    if result["success"]:
        flash(f"Ride #{ride_id} cancelled successfully.", "success")
    else:
        flash(result["error"], "danger")
    return redirect(url_for("rides_active"))


@app.route("/rides/<int:ride_id>/complete", methods=["GET", "POST"])
@login_required
def rides_complete(ride_id):
    if request.method == "GET":
        return render_template("rides_complete.html", ride_id=ride_id)

    rating = None
    raw    = request.form.get("rating", "").strip()
    if raw:
        try:
            rating = float(raw)
        except ValueError:
            flash("Rating must be a number between 1 and 5.", "warning")

    result = engine.complete_ride(ride_id, rating=rating)

    if result["success"]:
        msg = f"Ride #{ride_id} completed!"
        if "driverNewRating" in result:
            msg += f" Driver new rating: {result['driverNewRating']:.2f} ⭐"
        flash(msg, "success")
        return redirect(url_for("rides_history"))

    flash(result["error"], "danger")
    return redirect(url_for("rides_active"))


@app.route("/rides/queue")
@login_required
def rides_queue():
    result = engine.get_ride_queue()
    return render_template("rides_queue.html", queue=result)


@app.route("/rides/history")
@login_required
def rides_history():
    result = engine.get_ride_history()
    rides  = result.get("rides", []) if result["success"] else []
    return render_template("rides_history.html", rides=rides)


@app.route("/rides/search", methods=["GET", "POST"])
@login_required
def rides_search():
    found = None
    if request.method == "POST":
        try:
            ride_id = int(request.form["rideId"])
            result  = engine.search_ride(ride_id)
            if result["success"]:
                found = result
            else:
                flash(result["error"], "warning")
        except (ValueError, KeyError):
            flash("Please enter a valid ride ID.", "danger")

    return render_template("rides_search.html", found=found)


# ═══════════════════════════════════════════════════════════════════════════════
#   ADMIN
# ═══════════════════════════════════════════════════════════════════════════════

@app.route("/admin")
@login_required
def admin():
    result = engine.admin_dashboard()
    return render_template("admin.html", stats=result)


# ═══════════════════════════════════════════════════════════════════════════════
#   API ENDPOINTS  (JSON)  —  for optional AJAX / future mobile use
# ═══════════════════════════════════════════════════════════════════════════════

@app.route("/api/ping")
def api_ping():
    return jsonify(engine.ping())

@app.route("/api/map")
def api_map():
    return jsonify(engine.get_map())

@app.route("/api/drivers")
def api_drivers():
    return jsonify(engine.get_drivers())

@app.route("/api/active-rides")
def api_active_rides():
    return jsonify(engine.get_active_rides())

@app.route("/api/dashboard")
def api_dashboard():
    return jsonify(engine.admin_dashboard())


# ═══════════════════════════════════════════════════════════════════════════════
#   ERROR HANDLERS
# ═══════════════════════════════════════════════════════════════════════════════

@app.errorhandler(404)
def not_found(e):
    return render_template("error.html", code=404,
                           message="Page not found."), 404

@app.errorhandler(500)
def server_error(e):
    return render_template("error.html", code=500,
                           message="Internal server error."), 500


# ═══════════════════════════════════════════════════════════════════════════════
#   RUN
# ═══════════════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    # app.run(debug=True, port=5000) Development Server
    app.run(host='0.0.0.0', port=5000)
