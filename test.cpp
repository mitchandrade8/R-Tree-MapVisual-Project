#include "rtree.h"
#include <cassert> // For basic assertions
#include <vector>
#include <iostream>
#include <algorithm> // For std::any_of

// --- Helper Functions for Tests ---

// Check if two points are approximately equal (due to floating point)
bool points_equal(const Point &p1, const Point &p2, double tolerance = 1e-9)
{
    return std::abs(p1.x - p2.x) < tolerance && std::abs(p1.y - p2.y) < tolerance;
}

// Check if two rectangles are approximately equal
bool rects_equal(const Rectangle &r1, const Rectangle &r2, double tolerance = 1e-9)
{
    return points_equal(r1.min_corner, r2.min_corner, tolerance) &&
           points_equal(r1.max_corner, r2.max_corner, tolerance);
}

// Check if a specific DataItem (by ID) exists in a vector of DataItems
bool contains_item_id(const std::vector<DataItem> &items, int target_id)
{
    return std::any_of(items.begin(), items.end(),
                       [target_id](const DataItem &item)
                       { return item.id == target_id; });
}

// --- Test Functions ---

void test_rectangle_operations()
{
    std::cout << "Running Rectangle Operations Tests...\n";
    Rectangle r1(0, 0, 2, 2);
    Rectangle r2(1, 1, 3, 3);
    Rectangle r3(4, 4, 5, 5);
    Rectangle r4(0.5, 0.5, 1.5, 1.5); // Contained within r1
    Rectangle r_invalid(5, 5, 4, 4);  // Invalid rectangle

    // Area
    assert(std::abs(r1.area() - 4.0) < 1e-9);
    assert(std::abs(r2.area() - 4.0) < 1e-9);
    assert(std::abs(r3.area() - 1.0) < 1e-9);
    assert(std::abs(r_invalid.area() - 0.0) < 1e-9);

    // Contains Point
    assert(r1.contains(Point(1, 1)));
    assert(!r1.contains(Point(3, 1)));
    assert(r1.contains(Point(0, 0)));
    assert(r1.contains(Point(2, 2)));

    // Contains Rectangle
    assert(r1.contains(r4));
    assert(!r4.contains(r1));
    assert(!r1.contains(r2));

    // Intersects
    assert(r1.intersects(r2));
    assert(r2.intersects(r1));
    assert(!r1.intersects(r3));
    assert(!r3.intersects(r1));
    assert(r1.intersects(r4));
    assert(r4.intersects(r1));
    assert(!r1.intersects(r_invalid)); // Intersection with invalid is false
    assert(!r_invalid.intersects(r1));

    // Expand
    Rectangle r_expand = r1;
    r_expand.expand(r3); // Expand r1 to include r3
    assert(rects_equal(r_expand, Rectangle(0, 0, 5, 5)));
    r_expand = r1;
    r_expand.expand(r2); // Expand r1 to include r2
    assert(rects_equal(r_expand, Rectangle(0, 0, 3, 3)));
    r_expand = r1;
    r_expand.expand(r_invalid); // Expanding by invalid should not change
    assert(rects_equal(r_expand, r1));
    Rectangle r_empty;
    r_empty.expand(r1); // Expanding empty by valid makes it valid
    assert(rects_equal(r_empty, r1));

    // Combine
    Rectangle combined12 = Rectangle::combine(r1, r2);
    assert(rects_equal(combined12, Rectangle(0, 0, 3, 3)));
    Rectangle combined13 = Rectangle::combine(r1, r3);
    assert(rects_equal(combined13, Rectangle(0, 0, 5, 5)));
    Rectangle combined_invalid = Rectangle::combine(r1, r_invalid);
    assert(rects_equal(combined_invalid, r1)); // Combine with invalid returns the valid one
    Rectangle combined_both_invalid = Rectangle::combine(r_invalid, r_invalid);
    assert(combined_both_invalid.area() == 0.0); // Combining two invalids gives invalid

    // Area Increase
    assert(std::abs(r1.area_increase(r2) - (combined12.area() - r1.area())) < 1e-9); // 9-4=5
    assert(std::abs(r1.area_increase(r3) - (combined13.area() - r1.area())) < 1e-9); // 25-4=21
    assert(std::abs(r1.area_increase(r4) - 0.0) < 1e-9);                             // r4 is contained, no increase
    assert(std::abs(r1.area_increase(r_invalid) - 0.0) < 1e-9);                      // Increase by invalid is 0
    Rectangle r_empty2;
    assert(std::abs(r_empty2.area_increase(r1) - r1.area()) < 1e-9); // Increase for empty is area of other

    std::cout << "Rectangle Operations Tests Passed!\n";
}

void test_rtree_basic_operations()
{
    std::cout << "Running RTree Basic Operations Tests...\n";
    // Use M=3 to force splits earlier for testing
    RTree tree(2, 3);

    // Test empty tree
    assert(tree.empty());
    assert(tree.search(Rectangle(0, 0, 1, 1)).empty());

    // Insert first item
    DataItem item1(1, "CityA", 500000, Rectangle(1, 1, 3, 3));
    tree.insert(item1);
    assert(!tree.empty());

    // Search for the first item
    std::vector<DataItem> results = tree.search(Rectangle(0, 0, 2, 2)); // Intersects item1
    assert(results.size() == 1);
    assert(results[0].id == 1);

    results = tree.search(Rectangle(4, 4, 5, 5)); // Does not intersect
    assert(results.empty());

    results = tree.search(Rectangle(1.5, 1.5, 2.5, 2.5)); // Fully contained query
    assert(results.size() == 1);
    assert(results[0].id == 1);

    // Insert more items to potentially cause splits
    DataItem item2(2, "CityB", 1200000, Rectangle(5, 5, 7, 7));
    DataItem item3(3, "CityC", 800000, Rectangle(0, 5, 2, 7));
    DataItem item4(4, "CityD", 200000, Rectangle(6, 1, 8, 3));  // Should cause split (M=3)
    DataItem item5(5, "CityE", 1500000, Rectangle(3, 3, 4, 4)); // Near item1

    tree.insert(item2);
    tree.insert(item3);
    tree.insert(item4); // Potential split here
    tree.insert(item5); // Potential split/reorg here

    std::cout << "Tree structure after insertions (M=3):\n";
    tree.print_structure(std::cout); // Visually inspect structure (optional)

    // Test searches after splits
    results = tree.search(Rectangle(0, 0, 10, 10)); // Query encompassing all
    assert(results.size() == 5);
    assert(contains_item_id(results, 1));
    assert(contains_item_id(results, 2));
    assert(contains_item_id(results, 3));
    assert(contains_item_id(results, 4));
    assert(contains_item_id(results, 5));

    results = tree.search(Rectangle(0, 0, 4, 4)); // Should find items 1 and 5
    assert(results.size() == 2);
    assert(contains_item_id(results, 1));
    assert(contains_item_id(results, 5));

    results = tree.search(Rectangle(5.5, 0, 7.5, 4)); // Should find item 4
    assert(results.size() == 1);
    assert(contains_item_id(results, 4));

    results = tree.search(Rectangle(0, 4, 3, 8)); // Should find item 3
    assert(results.size() == 1);
    assert(contains_item_id(results, 3));

    results = tree.search(Rectangle(6, 6, 9, 9)); // Should find item 2
    assert(results.size() == 1);
    assert(contains_item_id(results, 2));

    std::cout << "RTree Basic Operations Tests Passed!\n";
}

void test_rtree_population_query()
{
    std::cout << "Running RTree Population Query (Integration) Test...\n";
    RTree tree(2, 4); // Use default M=4

    // Sample Data (Simplified US Cities/Regions)
    // Coordinates are arbitrary, not real lat/lon for simplicity
    tree.insert(DataItem(1, "New York Area", 8500000, Rectangle(70, 40, 75, 42)));      // High pop
    tree.insert(DataItem(2, "Los Angeles Area", 4000000, Rectangle(115, 33, 120, 35))); // High pop
    tree.insert(DataItem(3, "Chicago Area", 2700000, Rectangle(85, 41, 90, 43)));       // High pop
    tree.insert(DataItem(4, "Denver Area", 700000, Rectangle(100, 38, 105, 40)));       // Low pop
    tree.insert(DataItem(5, "Seattle Area", 750000, Rectangle(120, 47, 123, 49)));      // Low pop
    tree.insert(DataItem(6, "Houston Area", 2300000, Rectangle(94, 29, 97, 31)));       // High pop
    tree.insert(DataItem(7, "Rural Midwest", 50000, Rectangle(90, 44, 95, 46)));        // Low pop, outside major cities
    tree.insert(DataItem(8, "Phoenix Area", 1700000, Rectangle(110, 33, 113, 34)));     // High pop

    // Define a bounding box roughly representing the continental US
    Rectangle us_bounds(65, 25, 125, 50);
    long population_threshold = 1000000; // 1 Million

    std::cout << "Querying within US bounds ("
              << us_bounds.min_corner.x << "," << us_bounds.min_corner.y << ")-("
              << us_bounds.max_corner.x << "," << us_bounds.max_corner.y << ")"
              << " for population >= " << population_threshold << "\n";

    std::vector<DataItem> results = tree.search_with_population(us_bounds, population_threshold);

    std::cout << "Found " << results.size() << " items matching criteria:\n";
    for (const auto &item : results)
    {
        std::cout << "  - ID: " << item.id << ", Name: " << item.name << ", Pop: " << item.population << "\n";
    }

    // Assertions for the expected results
    assert(results.size() == 5);           // NY, LA, Chicago, Houston, Phoenix
    assert(contains_item_id(results, 1));  // NY
    assert(contains_item_id(results, 2));  // LA
    assert(contains_item_id(results, 3));  // Chicago
    assert(contains_item_id(results, 6));  // Houston
    assert(contains_item_id(results, 8));  // Phoenix
    assert(!contains_item_id(results, 4)); // Denver (pop too low)
    assert(!contains_item_id(results, 5)); // Seattle (pop too low)
    assert(!contains_item_id(results, 7)); // Rural (pop too low)

    // Test a smaller query region (e.g., West Coast)
    Rectangle west_coast_bounds(110, 30, 125, 50);
    std::cout << "\nQuerying within West Coast bounds for population >= " << population_threshold << "\n";
    results = tree.search_with_population(west_coast_bounds, population_threshold);
    std::cout << "Found " << results.size() << " items matching criteria:\n";
    for (const auto &item : results)
    {
        std::cout << "  - ID: " << item.id << ", Name: " << item.name << ", Pop: " << item.population << "\n";
    }
    assert(results.size() == 2);           // LA, Phoenix
    assert(contains_item_id(results, 2));  // LA
    assert(contains_item_id(results, 8));  // Phoenix
    assert(!contains_item_id(results, 5)); // Seattle (pop too low)

    std::cout << "RTree Population Query Test Passed!\n";
}

int main()
{
    std::cout << "===== Starting R-Tree Tests =====\n"
              << std::endl;

    test_rectangle_operations();
    std::cout << "\n---------------------------------\n"
              << std::endl;
    test_rtree_basic_operations();
    std::cout << "\n---------------------------------\n"
              << std::endl;
    test_rtree_population_query();

    std::cout << "\n===== All R-Tree Tests Completed Successfully! =====\n"
              << std::endl;
    return 0;
}