#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

void print_numbers(const char* label, const std::vector<int>& values) {
    std::cout << label;
    for (const int value : values) {
        std::cout << ' ' << value;
    }
    std::cout << '\n';
}

} // namespace

int main() {
    // 1. The simplest bulk-removal form.
    std::vector<int> scores{-3, 5, 0, -1};
    const auto removed = std::erase_if(scores, [](int value) {
        return value < 0;
    });
    assert(removed == 2);
    assert((scores == std::vector<int>{5, 0}));
    std::cout << "erase_if removed: " << removed << '\n';
    print_numbers("erase_if:", scores);

    // 2. Iterator deletion when each removal needs custom handling.
    std::vector<int> values{1, 2, 3, 4, 5, 6};
    for (auto it = values.begin(); it != values.end();) {
        if (*it % 2 == 0) {
            it = values.erase(it);
        } else {
            ++it;
        }
    }
    assert((values == std::vector<int>{1, 3, 5}));
    print_numbers("iterator erase:", values);

    // 3. Collect keys first when a later phase should perform the mutation.
    std::map<std::string, int> stock{
        {"apple", 0},
        {"pear", 4},
        {"kiwi", 0},
    };
    std::vector<std::string> sold_out;
    for (const auto& [name, count] : stock) {
        if (count == 0) {
            sold_out.push_back(name);
        }
    }
    for (const std::string& name : sold_out) {
        stock.erase(name);
    }
    assert(stock.size() == 1);
    std::cout << "collect keys:";
    for (const auto& [name, count] : stock) {
        std::cout << ' ' << name << '=' << count;
    }
    std::cout << '\n';

    // 4. Add after traversal so the source container is not mutated mid-loop.
    std::vector<int> source{1, 2, 3};
    std::vector<int> additions;
    for (const int value : source) {
        if (value % 2 != 0) {
            additions.push_back(value * 10);
        }
    }
    source.insert(source.end(), additions.begin(), additions.end());
    assert((source == std::vector<int>{1, 2, 3, 10, 30}));
    print_numbers("insert after loop:", source);

    // 5. remove_if compacts values but does not erase the container tail.
    std::vector<int> compact{3, -1, 5, -2};
    const auto new_end = std::remove_if(
        compact.begin(), compact.end(),
        [](int value) { return value < 0; });
    assert(new_end != compact.end());
    compact.erase(new_end, compact.end());
    assert((compact == std::vector<int>{3, 5}));
    print_numbers("remove_if + erase:", compact);
}
