// ExpenseTracker - C++ implementation (Deliverable 2: core functionality)
//
// Design notes for the cross-language comparison:
//
//   - An expense is a struct whose fields are declared once and fixed at
//     compile time. Where the Python version carries a dictionary whose shape
//     is a convention, here the shape is checked by the compiler.
//   - Storage uses STL containers: a std::vector owns the records and an
//     unordered_map indexes them by category. The vector owns its buffer and
//     releases it in its destructor, so no delete appears anywhere.
//   - Dates use std::chrono::year_month_day from C++20. Unlike Python's
//     datetime, parsing and formatting are not built in, so the helpers below
//     are code the Python version did not have to write.

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std::chrono;

// ---------------------------------------------------------------- date helpers

// C++ has no equivalent of datetime.strptime in the standard library before
// <chrono>'s parse, and even that is not available in every toolchain, so a
// date is parsed by hand.
year_month_day parse_date(const std::string& text) {
    int y = 0, m = 0, d = 0;
    char dash1 = 0, dash2 = 0;
    std::istringstream in(text);
    in >> y >> dash1 >> m >> dash2 >> d;
    if (!in || dash1 != '-' || dash2 != '-') {
        throw std::invalid_argument("date must be YYYY-MM-DD: " + text);
    }
    year_month_day ymd{year{y}, month{static_cast<unsigned>(m)}, day{static_cast<unsigned>(d)}};
    if (!ymd.ok()) throw std::invalid_argument("not a real date: " + text);
    return ymd;
}

std::string format_date(const year_month_day& ymd) {
    std::ostringstream out;
    out << int(ymd.year()) << '-'
        << std::setw(2) << std::setfill('0') << unsigned(ymd.month()) << '-'
        << std::setw(2) << std::setfill('0') << unsigned(ymd.day());
    return out.str();
}

// ---------------------------------------------------------------- model

// Every field is declared, typed and present on every record.
struct Expense {
    int id{};
    year_month_day when{};
    double amount{};
    std::string category;
    std::string description;
};

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string money(double amount) {
    std::ostringstream out;
    out << '$' << std::fixed << std::setprecision(2) << amount;
    return out.str();
}

std::string render(const Expense& e) {
    std::ostringstream out;
    out << "  #" << std::left << std::setw(4) << e.id
        << format_date(e.when) << "  " << std::right << std::setw(9) << money(e.amount)
        << "  " << std::left << std::setw(14) << e.category << e.description;
    return out.str();
}

// ---------------------------------------------------------------- store

class ExpenseStore {
public:
    // The vector owns the records. Reserving up front avoids reallocation,
    // which is a decision the language exposes and Python does not.
    ExpenseStore() { expenses_.reserve(64); }

    int add(const std::string& when, double amount,
            const std::string& category, const std::string& description) {
        if (amount < 0) throw std::invalid_argument("amount must not be negative");
        Expense e;
        e.id = next_id_++;
        e.when = parse_date(when);
        e.amount = amount;
        e.category = to_lower(category);
        e.description = description;

        expenses_.push_back(std::move(e));          // moved, not copied
        by_category_[expenses_.back().category].push_back(expenses_.size() - 1);
        return expenses_.back().id;
    }

    std::vector<Expense> all() const {
        std::vector<Expense> out = expenses_;
        std::sort(out.begin(), out.end(), [](const Expense& a, const Expense& b) {
            if (a.when != b.when) return a.when < b.when;
            return a.id < b.id;
        });
        return out;
    }

    std::vector<Expense> by_category(const std::string& category) const {
        std::vector<Expense> out;
        auto it = by_category_.find(to_lower(category));
        if (it == by_category_.end()) return out;    // empty, not an error
        for (std::size_t idx : it->second) out.push_back(expenses_[idx]);
        return out;
    }

    double total() const {
        double sum = 0.0;
        for (const Expense& e : expenses_) sum += e.amount;
        return sum;
    }

    // std::map keeps the categories ordered; the caller re-sorts by value.
    std::map<std::string, double> total_by_category() const {
        std::map<std::string, double> totals;
        for (const Expense& e : expenses_) totals[e.category] += e.amount;
        return totals;
    }

    std::size_t size() const { return expenses_.size(); }

private:
    std::vector<Expense> expenses_;
    std::unordered_map<std::string, std::vector<std::size_t>> by_category_;
    int next_id_ = 1;
};

// ---------------------------------------------------------------- driver

int main() {
    std::cout << "=== ExpenseTracker (C++) - core functionality ===\n\n";

    ExpenseStore store;
    struct Seed { const char* when; double amount; const char* category; const char* description; };
    const std::vector<Seed> seed = {
        {"2026-09-01", 82.40, "groceries", "weekly shop"},
        {"2026-09-03", 15.00, "transport", "metro card top up"},
        {"2026-09-05", 120.00, "utilities", "electricity bill"},
        {"2026-09-08", 46.75, "dining", "dinner with study group"},
        {"2026-09-12", 91.10, "groceries", "weekly shop"},
        {"2026-09-15", 9.99, "entertainment", "streaming subscription"},
        {"2026-09-19", 63.20, "groceries", "weekly shop and household items"},
        {"2026-09-22", 28.50, "transport", "rideshare to campus"},
        {"2026-09-27", 55.00, "dining", "lunch with classmates"},
    };
    for (const Seed& s : seed) store.add(s.when, s.amount, s.category, s.description);

    std::cout << "--- all expenses (" << store.size() << " records) ---\n";
    for (const Expense& e : store.all()) std::cout << render(e) << '\n';

    std::cout << "\n--- filter by category: groceries ---\n";
    for (const Expense& e : store.by_category("groceries")) std::cout << render(e) << '\n';

    std::cout << "\n--- totals ---\n";
    // Sort the per-category totals by value, largest first.
    auto totals = store.total_by_category();
    std::vector<std::pair<std::string, double>> ranked(totals.begin(), totals.end());
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    for (const auto& [category, amount] : ranked) {
        std::cout << "  " << std::left << std::setw(16) << category
                  << std::right << std::setw(10) << money(amount) << '\n';
    }
    std::cout << "  " << std::left << std::setw(16) << "OVERALL"
              << std::right << std::setw(10) << money(store.total()) << '\n';

    // A string amount, accepted silently by the Python version, does not
    // compile here: store.add("2026-09-30", "34.25", "utilities", "water bill");
    store.add("2026-09-30", 34.25, "utilities", "water bill");
    std::cout << "\nafter one more record: " << store.size()
              << " records, overall " << money(store.total()) << '\n';
    return 0;
}
