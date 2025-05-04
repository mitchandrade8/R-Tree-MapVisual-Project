#ifndef RTREE_H
#define RTREE_H

#include <vector>
#include <memory>  // For std::unique_ptr
#include <cstddef> // For size_t
#include <string>
// #include <iosfwd> // Replaced by <iostream>
#include <iostream> // Include full iostream for std::ostream and std::cout definitions

// --- Basic Geometric Structures ---

struct Point
{
    double x = 0.0;
    double y = 0.0;

    Point(double x_ = 0.0, double y_ = 0.0) : x(x_), y(y_) {}
};

struct Rectangle
{
    Point min_corner;
    Point max_corner;

    Rectangle(); // Default constructor
    Rectangle(Point min_c, Point max_c);
    Rectangle(double min_x, double min_y, double max_x, double max_y);

    // --- Inline Methods ---
    double area() const
    {
        if (max_corner.x < min_corner.x || max_corner.y < min_corner.y)
            return 0.0; // Invalid
        return (max_corner.x - min_corner.x) * (max_corner.y - min_corner.y);
    }

    bool contains(const Point &p) const
    {
        return p.x >= min_corner.x && p.x <= max_corner.x &&
               p.y >= min_corner.y && p.y <= max_corner.y;
    }

    bool contains(const Rectangle &other) const
    {
        return other.min_corner.x >= min_corner.x &&
               other.max_corner.x <= max_corner.x &&
               other.min_corner.y >= min_corner.y &&
               other.max_corner.y <= max_corner.y;
    }

    // --- Methods implemented in rtree.cpp ---
    bool intersects(const Rectangle &other) const;
    void expand(const Rectangle &other);                // Expand this MBR to include another
    double area_increase(const Rectangle &other) const; // Calculate area increase needed

    // --- Static Methods ---
    static Rectangle combine(const Rectangle &r1, const Rectangle &r2); // Combine two MBRs
};

// --- Data Item Structure ---
// Represents the actual data stored in the leaf nodes.
// Includes the spatial extent (rectangle) and associated attributes.
struct DataItem
{
    int id;           // Unique identifier for the data item
    std::string name; // Name (e.g., city name, region code)
    long population;  // Associated data (e.g., population count)
    Rectangle bounds; // The spatial bounding box of this item

    DataItem(int id_ = 0, std::string name_ = "", long pop_ = 0, Rectangle b_ = Rectangle())
        : id(id_), name(std::move(name_)), population(pop_), bounds(b_) {}
};

// --- R-Tree Node Structure ---

// Forward declaration
class RTree;

struct RTreeNode
{
    using NodePtr = std::unique_ptr<RTreeNode>;

    Rectangle mbr; // Minimum Bounding Rectangle enclosing all entries/children in this node
    bool is_leaf = true;
    RTreeNode *parent = nullptr; // Non-owning pointer to parent

    // Data stored in the node
    std::vector<DataItem> data_entries; // Used only if is_leaf is true
    std::vector<NodePtr> children;      // Used only if is_leaf is false

    explicit RTreeNode(bool leaf = true); // Constructor

    // --- Methods implemented in rtree.cpp ---
    void update_mbr(); // Recalculate MBR based on contents
    bool is_full(size_t max_entries) const;
    size_t size() const;
};

// --- R-Tree Class ---

class RTree
{
public:
    using NodePtr = RTreeNode::NodePtr;

    // Constructor: Sets min/max entries per node
    explicit RTree(size_t min_entries = 2, size_t max_entries = 4);

    // --- Rule of Five/Zero ---
    // R-Trees with unique_ptr children are complex to copy/move correctly.
    // Deleting them prevents accidental slicing or double-frees.
    RTree(const RTree &) = delete;
    RTree &operator=(const RTree &) = delete;
    RTree(RTree &&) = delete;            // Could be implemented, but complex
    RTree &operator=(RTree &&) = delete; // Could be implemented, but complex
    ~RTree() = default;                  // Default destructor is sufficient thanks to unique_ptr

    // --- Core Public Methods ---

    // Insert a data item into the tree
    void insert(const DataItem &item);

    // Search for data items whose bounds intersect with a query rectangle
    std::vector<DataItem> search(const Rectangle &query_rect) const;

    // Search for data items intersecting query_rect AND meeting a population criterion
    std::vector<DataItem> search_with_population(const Rectangle &query_rect, long min_population) const;

    // Simple console visualization of the tree structure (for debugging)
    // Now requires <iostream> to be included for std::cout default argument
    void print_structure(std::ostream &os = std::cout) const;

    // Check if the tree is empty
    bool empty() const;

private:
    NodePtr root_;       // Root node of the R-Tree
    size_t min_entries_; // Minimum number of entries per node (except root)
    size_t max_entries_; // Maximum number of entries per node

    // --- Private Helper Methods (Declarations) ---

    // Choose the best subtree to insert into (minimizes MBR enlargement)
    RTreeNode *choose_subtree(RTreeNode *node, const Rectangle &item_bounds) const;

    // Recursive helper for insertion
    NodePtr insert_recursive(RTreeNode *node, const DataItem &item);

    // Splits a full node. Returns the newly created node.
    NodePtr split_node(RTreeNode *node); // Modifies node, returns new node

    // Recursive helper for standard spatial search
    void search_recursive(const RTreeNode *node, const Rectangle &query_rect, std::vector<DataItem> &results) const;

    // Recursive helper for search with population filter
    void search_pop_recursive(const RTreeNode *node, const Rectangle &query_rect, long min_population, std::vector<DataItem> &results) const;

    // Recursive helper for printing the tree structure
    // Requires <iostream> for std::ostream definition
    void print_node(std::ostream &os, const RTreeNode *node, int indent) const;
};

#endif // RTREE_H

