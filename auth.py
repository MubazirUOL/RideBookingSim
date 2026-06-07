"""
auth.py — Simple file-based authentication helpers
Stores credentials in accounts.txt as: username,password (one per line)
"""

import os

ACCOUNTS_FILE = os.path.join(os.path.dirname(__file__), "accounts.txt")


def _load_accounts():
    """Read all accounts from accounts.txt → {username: password}"""
    accounts = {}
    if not os.path.exists(ACCOUNTS_FILE):
        return accounts
    with open(ACCOUNTS_FILE, "r") as f:
        for line in f:
            line = line.strip()
            if line and "," in line:
                username, password = line.split(",", 1)
                accounts[username.strip()] = password.strip()
    return accounts


def _save_accounts(accounts):
    """Write all accounts dict back to accounts.txt"""
    with open(ACCOUNTS_FILE, "w") as f:
        for username, password in accounts.items():
            f.write(f"{username},{password}\n")


def verify_login(username, password):
    """Return True if username+password match a stored account."""
    accounts = _load_accounts()
    return accounts.get(username) == password


def register_account(username, password):
    """
    Register a new account. Returns (success: bool, message: str).
    Fails if username already exists or fields are empty.
    """
    username = username.strip()
    password = password.strip()

    if not username or not password:
        return False, "Username and password cannot be empty."

    accounts = _load_accounts()

    if username in accounts:
        return False, f"Username '{username}' is already taken."

    accounts[username] = password
    _save_accounts(accounts)
    return True, f"Account '{username}' created successfully."


def get_all_accounts():
    """Return list of usernames (no passwords) for admin view."""
    return list(_load_accounts().keys())


def account_exists():
    """Return True if at least one account exists (to skip setup)."""
    return len(_load_accounts()) > 0
