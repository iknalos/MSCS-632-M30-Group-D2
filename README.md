# ExpenseTracker — Deliverable 2: Core Functionality

**MSCS-632-M30 Advanced Programming Language · Group Project (Option 1: Expense Tracker Application)**
**Languages:** Python and C++ · **Instructor:** Jay Thom · **University of the Cumberlands**

| Team member | Role |
|---|---|
| Rahul Solanki | Python implementation |
| Krinal Soni | C++ implementation |
| Bijay Raj KC | Specification and documentation |

This repository holds the **Saturday** deliverable: core functionality working
end to end in both languages. Date-range filtering, keyword search, percentage
shares, delete, the interactive prompt, the benchmark and the unit tests arrive
in the Day 3 repository.

## What works at this stage

- Expense records with id, date, amount, category and description (requirement R1)
- A seeded dataset of nine expenses listed oldest first
- Category filtering through an index rather than a scan (part of requirement R2)
- Totals by category and overall (requirement R3)

Both implementations report the same totals: **$511.94** across nine records,
and **$546.19** after a tenth is added.

## Layout

```
python/expense_tracker.py     model, store and driver (single file at this stage)
cpp/expense_tracker.cpp       model, store and driver (single file at this stage)
```

## Build and run

Both were built and run in WSL2 Ubuntu 24.04 (Python 3.12.3, g++ 13.3.0).

```bash
cd python && python3 expense_tracker.py

cd cpp && g++ -std=c++20 -Wall -Wextra -O2 expense_tracker.cpp -o expense_tracker && ./expense_tracker
```

## Language-specific features demonstrated

| Language | Feature | Where |
|---|---|---|
| Python | `dict` keyed by id for storage; `defaultdict` category index | `ExpenseStore.__init__` |
| Python | Dynamic typing: the amount accepts a float or a numeric string | `add()`, and the final line of `main()` |
| Python | `datetime.strptime` for parsing and `date` for comparison | `add()` |
| Python | Comprehensions, generator expressions and f-string formatting | `total()`, `render()` |
| C++ | `struct Expense` with declared, fixed fields | `struct Expense` |
| C++ | `std::vector` owning the records; `reserve` to avoid reallocation | `ExpenseStore` |
| C++ | `std::unordered_map` category index; `std::map` for ordered totals | `by_category_`, `total_by_category()` |
| C++ | `std::move` to hand strings to the container rather than copy them | `add()` |
| C++ | RAII: containers free their buffers, so no `new` or `delete` appears | whole file |

## Known limitations at this stage

- No date-range filter, keyword search, percentage shares or delete yet (Day 3).
- No interactive prompt, benchmark or automated tests yet (Day 3).
- Amounts use `double`, so values that are not exactly representable in binary
  floating point are rounded at display time. Both languages round identically.
