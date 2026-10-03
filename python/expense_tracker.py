"""ExpenseTracker - Python implementation (Deliverable 2: core functionality).

Design notes for the cross-language comparison:

* Storage is a dictionary keyed by expense id, with a second dictionary acting
  as a category index. Dictionaries are the idiomatic Python container here and
  are what the assignment asks us to demonstrate.
* Types are not declared. An expense record is a plain dict whose shape exists
  only by convention, and the amount accepts anything that behaves like a
  number until it does not.
* Dates are handled with the standard library datetime module, which parses,
  compares and formats dates with no third-party code.
"""

from collections import defaultdict
from datetime import date, datetime

DATE_FORMAT = "%Y-%m-%d"


class ExpenseStore:
    """Holds every expense and the indexes used to query them."""

    def __init__(self):
        # id -> expense dict. The record has no declared type; its shape is a
        # convention the rest of the file agrees to follow.
        self._expenses = {}
        # category -> list of ids. A defaultdict removes the "first insert"
        # special case entirely.
        self._by_category = defaultdict(list)
        self._next_id = 1

    def add(self, when, amount, category, description):
        """Record one expense and return its id.

        `when` may be a date or a YYYY-MM-DD string; `amount` may be an int,
        a float or a numeric string. Accepting all of them is easy in Python
        and is exactly the flexibility that hides mistakes.
        """
        if isinstance(when, str):
            when = datetime.strptime(when, DATE_FORMAT).date()
        amount = float(amount)
        if amount < 0:
            raise ValueError("amount must not be negative")

        expense_id = self._next_id
        self._next_id += 1

        self._expenses[expense_id] = {
            "id": expense_id,
            "date": when,
            "amount": amount,
            "category": category.lower(),
            "description": description,
        }
        self._by_category[category.lower()].append(expense_id)
        return expense_id

    def all(self):
        """Every expense, oldest first."""
        return sorted(self._expenses.values(), key=lambda e: (e["date"], e["id"]))

    def by_category(self, category):
        """Every expense in one category, using the index rather than a scan."""
        ids = self._by_category.get(category.lower(), [])
        return [self._expenses[i] for i in ids]

    def total(self):
        """Overall total."""
        return sum(e["amount"] for e in self._expenses.values())

    def total_by_category(self):
        """Per-category totals, largest first."""
        totals = defaultdict(float)
        for e in self._expenses.values():
            totals[e["category"]] += e["amount"]
        return dict(sorted(totals.items(), key=lambda kv: kv[1], reverse=True))

    def __len__(self):
        return len(self._expenses)


def render(expense):
    """Format one expense as a table row."""
    return "  #{id:<3} {date}  {amount:>9}  {category:<14} {description}".format(
        id=expense["id"],
        date=expense["date"].strftime(DATE_FORMAT),
        amount=f"${expense['amount']:,.2f}",
        category=expense["category"],
        description=expense["description"],
    )


SEED = [
    ("2026-09-01", 82.40, "groceries", "weekly shop"),
    ("2026-09-03", 15.00, "transport", "metro card top up"),
    ("2026-09-05", 120.00, "utilities", "electricity bill"),
    ("2026-09-08", 46.75, "dining", "dinner with study group"),
    ("2026-09-12", 91.10, "groceries", "weekly shop"),
    ("2026-09-15", 9.99, "entertainment", "streaming subscription"),
    ("2026-09-19", 63.20, "groceries", "weekly shop and household items"),
    ("2026-09-22", 28.50, "transport", "rideshare to campus"),
    ("2026-09-27", 55.00, "dining", "lunch with classmates"),
]


def main():
    print("=== ExpenseTracker (Python) - core functionality ===\n")

    store = ExpenseStore()
    for when, amount, category, description in SEED:
        store.add(when, amount, category, description)

    print(f"--- all expenses ({len(store)} records) ---")
    for e in store.all():
        print(render(e))

    print("\n--- filter by category: groceries ---")
    for e in store.by_category("groceries"):
        print(render(e))

    print("\n--- totals ---")
    for category, amount in store.total_by_category().items():
        print(f"  {category:<16} ${amount:>9,.2f}")
    print(f"  {'OVERALL':<16} ${store.total():>9,.2f}")

    # Dynamic typing on display: the amount above was given as a float, but a
    # numeric string is accepted just as readily.
    store.add(date(2026, 9, 30), "34.25", "utilities", "water bill")
    print(f"\nafter adding a string amount: {len(store)} records, "
          f"overall ${store.total():,.2f}")


if __name__ == "__main__":
    main()
