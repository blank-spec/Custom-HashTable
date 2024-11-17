# Custom-HashTable
## Description
That header file provides implementation of Hash-table. 

## Performance
I conducted several performance tests to compare my hash table with `std::unordered_map` from the C++ Standard Library. The results show that my hash table is only 1.5 times slower than `unordered_map`.


### Requirements
- C++ 20 or later
- Install Custom-Vector
- Install Simple-Forward-List

## Examples
```cpp
int main() {
    HashTable<int, int> table = {{1, 2}, {4, 5}}; // crearing a table
    table.insert({10, 20}); // inserting value by the key
    table.erase(10); // removing element by key

    if (table.contains(1))
        std::cout << table[1] << std::endl; // access bu the key
    else 
        std::cout << "Not found" << std::endl;

    for (const auto& [key, value] : table)
        std::cout << key << ' ' << value << std::endl;
}
