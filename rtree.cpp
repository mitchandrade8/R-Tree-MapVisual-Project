#include "rtree.h" // Include the header declarations

// --- Added missing include ---
#include <iostream> // For std::cout, std::cerr, std::ostream (needed for print methods and error messages)
// ---------------------------

#include <cmath>     // For std::abs, std::min, std::max
#include <limits>    // For std::numeric_limits
#include <algorithm> // For std::min, std::max, std::find_if, std::sort (potentially useful for better split)
#include <stdexcept> // For std::invalid_argument, std::runtime_error
#include <vector>
#include <memory>
#include <iterator> // For std::make_move_iterator
#include <utility>  // For std::move

// --- Rectangle Method Implementations ---

Rectangle::Rectangle() = default; // Use default implementation

Rectangle::Rectangle(Point min_c, Point max_c) : min_corner(min_c), max_corner(max_c) {}

Rectangle::Rectangle(double min_x, double min_y, double max_x, double max_y)
    : min_corner(min_x, min_y), max_corner(max_x, max_y) {}

// Check if this rectangle intersects another rectangle
bool Rectangle::intersects(const Rectangle &other) const
{
    // Check for no overlap (the separating axis theorem)
    if (max_corner.x < other.min_corner.x || min_corner.x > other.max_corner.x ||
        max_corner.y < other.min_corner.y || min_corner.y > other.max_corner.y)
    {
        return false; // No intersection
    }
    return true; // They overlap
}

// Expand this rectangle's MBR to minimally enclose another rectangle
void Rectangle::expand(const Rectangle &other)
{
    // If the other rectangle is invalid, do nothing
    if (other.min_corner.x > other.max_corner.x || other.min_corner.y > other.max_corner.y)
    {
        return;
    }
    // If this rectangle is currently invalid (e.g., default constructed), just become the other one
    if (min_corner.x > max_corner.x || min_corner.y > max_corner.y)
    {
        *this = other;
        return;
    }
    // Otherwise, expand bounds
    min_corner.x = std::min(min_corner.x, other.min_corner.x);
    min_corner.y = std::min(min_corner.y, other.min_corner.y);
    max_corner.x = std::max(max_corner.x, other.max_corner.x);
    max_corner.y = std::max(max_corner.y, other.max_corner.y);
}

// Calculate the minimal bounding rectangle enclosing two rectangles
Rectangle Rectangle::combine(const Rectangle &r1, const Rectangle &r2)
{
    // Handle cases where one or both rectangles might be invalid
    bool r1_invalid = r1.min_corner.x > r1.max_corner.x || r1.min_corner.y > r1.max_corner.y;
    bool r2_invalid = r2.min_corner.x > r2.max_corner.x || r2.min_corner.y > r2.max_corner.y;

    if (r1_invalid && r2_invalid)
        return Rectangle(); // Both invalid
    if (r1_invalid)
        return r2; // Only r1 invalid
    if (r2_invalid)
        return r1; // Only r2 invalid

    // Both are valid, combine them
    return Rectangle(
        std::min(r1.min_corner.x, r2.min_corner.x),
        std::min(r1.min_corner.y, r2.min_corner.y),
        std::max(r1.max_corner.x, r2.max_corner.x),
        std::max(r1.max_corner.y, r2.max_corner.y));
}

// Calculate the increase in area needed for this MBR to include another rectangle
double Rectangle::area_increase(const Rectangle &other) const
{
    // If the other rectangle is invalid, the increase is 0
    if (other.min_corner.x > other.max_corner.x || other.min_corner.y > other.max_corner.y)
    {
        return 0.0;
    }
    // If this rectangle is invalid, the increase is the area of the other rectangle
    if (min_corner.x > max_corner.x || min_corner.y > max_corner.y)
    {
        return other.area();
    }
    // Calculate the area of the combined rectangle
    Rectangle combined = combine(*this, other);
    // Increase is the combined area minus the current area
    return combined.area() - this->area();
}

// --- RTreeNode Method Implementations ---

RTreeNode::RTreeNode(bool leaf) : is_leaf(leaf), parent(nullptr)
{
    // MBR is default initialized (invalid state) until updated
}

// Recalculate the MBR for this node based on its children or data entries
void RTreeNode::update_mbr()
{
    if (is_leaf)
    {
        if (data_entries.empty())
        {
            mbr = Rectangle(); // Reset MBR to invalid state for empty leaf
            return;
        }
        // Calculate MBR from data entries
        mbr = data_entries[0].bounds;
        for (size_t i = 1; i < data_entries.size(); ++i)
        {
            mbr.expand(data_entries[i].bounds);
        }
    }
    else
    { // Internal node
        if (children.empty())
        {
            mbr = Rectangle(); // Reset MBR to invalid state for empty internal node
            return;
        }
        // Calculate MBR from children's MBRs
        // Ensure the first child exists before accessing its MBR
        size_t first_valid_child = 0;
        while (first_valid_child < children.size() && !children[first_valid_child])
        {
            first_valid_child++;
        }

        if (first_valid_child >= children.size())
        { // No valid children found
            mbr = Rectangle();
            return;
        }

        mbr = children[first_valid_child]->mbr; // Start with the first valid child's MBR
        for (size_t i = first_valid_child + 1; i < children.size(); ++i)
        {
            // Ensure child pointer and its MBR are valid before expanding
            if (children[i])
            { // Basic null check for child pointer
                mbr.expand(children[i]->mbr);
            }
        }
    }
}

// Check if the node has reached its maximum capacity
bool RTreeNode::is_full(size_t max_entries) const
{
    return size() >= max_entries;
}

// Get the current number of entries (for leaf) or children (for internal)
size_t RTreeNode::size() const
{
    return is_leaf ? data_entries.size() : children.size();
}

// --- RTree Method Implementations ---

RTree::RTree(size_t min_entries, size_t max_entries)
    : min_entries_(std::max((size_t)2, min_entries)),                    // Ensure min is reasonable
      max_entries_(std::max({(size_t)3, min_entries_ * 2, max_entries})) // Ensure max >= 3 and >= 2*min
{
    // Optional: Add warning if min > max/2, as it affects some split algorithms
    if (min_entries_ > max_entries_ / 2 && min_entries_ != 2)
    {
        // Use std::cerr for warnings/errors now that <iostream> is included
        std::cerr << "Warning: RTree Min entries (" << min_entries_
                  << ") > Max entries (" << max_entries_
                  << ") / 2. This might affect performance with certain split heuristics.\n";
    }
    // Start with an empty leaf node as the root
    root_ = std::make_unique<RTreeNode>(true); // Requires C++14 or later
}

// Insert a DataItem into the R-Tree
void RTree::insert(const DataItem &item)
{
    if (!root_)
    { // Should not happen with current constructor, but defensive check
        root_ = std::make_unique<RTreeNode>(true);
    }
    // Start recursive insertion from the root
    NodePtr split_node = insert_recursive(root_.get(), item);

    // Check if the root node was split during insertion
    if (split_node)
    {
        // Create a new root node (which will be an internal node)
        auto new_root = std::make_unique<RTreeNode>(false); // New root is internal

        // Set parent pointers of the old root and the new node from the split
        root_->parent = new_root.get();
        split_node->parent = new_root.get();

        // Calculate the MBR for the new root based on its two children
        new_root->mbr = Rectangle::combine(root_->mbr, split_node->mbr);

        // Add the old root and the new node as children of the new root
        new_root->children.push_back(std::move(root_));
        new_root->children.push_back(std::move(split_node));

        // Update the tree's root pointer
        root_ = std::move(new_root);
    }
}

// Public search method: Find items intersecting a query rectangle
std::vector<DataItem> RTree::search(const Rectangle &query_rect) const
{
    std::vector<DataItem> results;
    if (root_ && root_->mbr.intersects(query_rect))
    { // Check intersection with root MBR first
        search_recursive(root_.get(), query_rect, results);
    }
    return results;
}

// Public search method: Find items intersecting query_rect with minimum population
std::vector<DataItem> RTree::search_with_population(const Rectangle &query_rect, long min_population) const
{
    std::vector<DataItem> results;
    if (root_ && root_->mbr.intersects(query_rect))
    { // Check intersection with root MBR first
        search_pop_recursive(root_.get(), query_rect, min_population, results);
    }
    return results;
}

// Print the tree structure to an output stream (e.g., std::cout)
void RTree::print_structure(std::ostream &os) const
{
    // Now uses 'os' parameter correctly
    os << "--- R-Tree Structure ---\n";
    if (!root_ || (root_->is_leaf && root_->data_entries.empty() && root_->children.empty()))
    {
        os << "(Empty Tree)\n";
    }
    else
    {
        print_node(os, root_.get(), 0);
    }
    os << "------------------------\n";
}

// Check if the tree is empty
bool RTree::empty() const
{
    return !root_ || root_->size() == 0;
}

// --- RTree Private Helper Method Implementations ---

// Choose the best subtree to insert into (minimizes MBR area increase)
RTreeNode *RTree::choose_subtree(RTreeNode *node, const Rectangle &item_bounds) const
{
    // Precondition: node is guaranteed to be an internal node.
    if (node->children.empty())
    {
        // This indicates an error state (internal node with no children)
        throw std::runtime_error("Internal RTree node has no children during choose_subtree.");
    }

    RTreeNode *best_child = nullptr;
    double min_increase = std::numeric_limits<double>::max();
    double min_area = std::numeric_limits<double>::max();

    // Iterate through the children of the current node
    for (auto &child_ptr : node->children)
    {
        if (!child_ptr)
            continue; // Safety check for null pointers

        double current_area = child_ptr->mbr.area();
        // Calculate how much the child's MBR would need to increase to include the new item
        double increase = child_ptr->mbr.area_increase(item_bounds);

        // Primary criterion: Choose the child requiring the minimum area increase
        if (increase < min_increase)
        {
            min_increase = increase;
            min_area = current_area; // Store area for tie-breaking
            best_child = child_ptr.get();
        }
        // Tie-breaking criterion: If increases are equal, choose the child with the smallest current MBR area
        else if (increase == min_increase)
        {
            // Ensure best_child is not null before comparing areas
            if (best_child == nullptr || current_area < min_area)
            {
                min_area = current_area;
                best_child = child_ptr.get();
            }
        }
    }

    // Fallback: If all children resulted in the same (possibly max) increase,
    // or if only one child existed, best_child might still be null after the loop.
    // Choose the first valid child in such cases.
    if (!best_child)
    {
        for (auto &child_ptr : node->children)
        {
            if (child_ptr)
            {
                best_child = child_ptr.get();
                break;
            }
        }
    }

    // If still no best child found (e.g., all children were nullptrs), it's an error
    if (!best_child)
    {
        throw std::runtime_error("Failed to find any valid child node in choose_subtree.");
    }

    return best_child;
}

// Recursive helper function for inserting a DataItem
RTree::NodePtr RTree::insert_recursive(RTreeNode *node, const DataItem &item)
{
    // Expand the node's MBR on the way down *before* choosing a subtree or inserting
    // This ensures parent MBRs always contain their children/entries.
    node->mbr.expand(item.bounds);

    if (node->is_leaf)
    {
        // Add the data item to the leaf node
        node->data_entries.push_back(item);
        // MBR was already expanded. Check if the node is now full.
        if (node->is_full(max_entries_))
        {
            // Split the node and return the new node resulting from the split
            return split_node(node);
        }
        // Node is not full, no split occurred
        return nullptr;
    }
    else
    { // Internal node
        // Choose the best child node to descend into
        RTreeNode *subtree_to_insert = choose_subtree(node, item.bounds);

        // Recursively insert the item into the chosen subtree
        NodePtr potential_split_node = insert_recursive(subtree_to_insert, item);

        // Check if the recursive call resulted in a split of the child node
        if (potential_split_node)
        {
            // A split occurred below. Add the new node (returned by the recursive call)
            // as a child of the *current* node.
            potential_split_node->parent = node; // Set parent pointer of the new node
            node->children.push_back(std::move(potential_split_node));

            // MBR was already expanded at the start. Check if the *current* node is now full.
            if (node->is_full(max_entries_))
            {
                // Split the current node and propagate the split upwards
                return split_node(node);
            }
        }
        // No split occurred below, or if it did, the current node didn't become full.
        return nullptr; // No split needs to be propagated upwards from this node.
    }
}

// Splits a full node (either leaf or internal) into two nodes.
// Returns a unique_ptr to the newly created node.
// Modifies the original node ('node') to contain the first half of the entries/children.
// Note: This uses a very simple linear split (dividing items in half).
//       Real R-Trees use more sophisticated algorithms (Linear, Quadratic, R*)
//       to minimize overlap and area for better query performance. This is a placeholder.
RTree::NodePtr RTree::split_node(RTreeNode *node)
{
    size_t total_size = node->size();

    // Determine the split point. Aim for roughly half, but respect min_entries_.
    // This simple split doesn't guarantee min_entries_ for both resulting nodes,
    // which is a limitation compared to proper R-Tree split algorithms.
    size_t split_index = std::max(min_entries_, total_size / 2);
    // Ensure the original node keeps at least min_entries_ if possible
    // and the new node also gets at least min_entries_ if total_size allows
    if (total_size > min_entries_ * 2)
    { // Only adjust if there's enough to satisfy min on both sides
        if (total_size - split_index < min_entries_)
        {
            split_index = total_size - min_entries_;
        }
        if (split_index < min_entries_)
        { // Ensure original node keeps min_entries_
            split_index = min_entries_;
        }
    }
    else
    {                                       // Not enough items to guarantee min_entries on both, just split near middle
        split_index = (total_size + 1) / 2; // Ensure first node gets slightly more if odd
    }

    // Clamp split_index to valid range [1, total_size - 1] if possible
    split_index = std::max((size_t)1, std::min(split_index, total_size > 1 ? total_size - 1 : 1));

    // Create the new sibling node (same type: leaf or internal)
    auto new_node = std::make_unique<RTreeNode>(node->is_leaf);
    new_node->parent = node->parent; // Initially shares the same parent

    if (node->is_leaf)
    {
        // Move the second half of data entries to the new node
        if (split_index < node->data_entries.size())
        {
            new_node->data_entries.assign(
                std::make_move_iterator(node->data_entries.begin() + split_index),
                std::make_move_iterator(node->data_entries.end()));
            // Erase the moved entries from the original node
            node->data_entries.erase(node->data_entries.begin() + split_index, node->data_entries.end());
        }
        else if (!node->data_entries.empty() && split_index == node->data_entries.size())
        {
            // Edge case: trying to move 0 elements when split_index == size
            // This might happen if logic results in split_index being size, just means new node is empty initially.
        }
        else if (!node->data_entries.empty())
        {
            std::cerr << "Warning: Split index issue during leaf node split. Size: " << total_size << ", Index: " << split_index << std::endl;
        }
    }
    else
    {   // Internal node
        // Move the second half of child pointers to the new node
        if (split_index < node->children.size())
        {
            new_node->children.assign(
                std::make_move_iterator(node->children.begin() + split_index),
                std::make_move_iterator(node->children.end()));
            // Erase the moved children from the original node
            node->children.erase(node->children.begin() + split_index, node->children.end());

            // IMPORTANT: Update the parent pointers of the moved children to point to the new node
            for (auto &child : new_node->children)
            {
                if (child)
                    child->parent = new_node.get();
            }
        }
        else if (!node->children.empty() && split_index == node->children.size())
        {
            // Edge case: trying to move 0 elements when split_index == size
        }
        else if (!node->children.empty())
        {
            std::cerr << "Warning: Split index issue during internal node split. Size: " << total_size << ", Index: " << split_index << std::endl;
        }
    }

    // Update the MBRs for both the original node and the new node, as their contents have changed
    node->update_mbr();
    new_node->update_mbr();

    // Return the pointer to the newly created node
    return new_node;
}

// Recursive helper for standard spatial search (find items intersecting query_rect)
void RTree::search_recursive(const RTreeNode *node, const Rectangle &query_rect, std::vector<DataItem> &results) const
{
    if (!node)
        return; // Safety check

    if (node->is_leaf)
    {
        // Leaf node: Check each data item's bounds against the query rectangle
        for (const auto &item : node->data_entries)
        {
            if (item.bounds.intersects(query_rect))
            {
                results.push_back(item); // Add item to results if it intersects
            }
        }
    }
    else
    { // Internal node
        // Internal node: Check which children's MBRs intersect the query rectangle
        for (const auto &child_ptr : node->children)
        {
            // If child exists and its MBR intersects the query, descend into that child
            if (child_ptr && child_ptr->mbr.intersects(query_rect))
            {
                search_recursive(child_ptr.get(), query_rect, results);
            }
        }
    }
}

// Recursive helper for spatial search with population filter
void RTree::search_pop_recursive(const RTreeNode *node, const Rectangle &query_rect, long min_population, std::vector<DataItem> &results) const
{
    if (!node)
        return; // Safety check

    if (node->is_leaf)
    {
        // Leaf node: Check each data item
        for (const auto &item : node->data_entries)
        {
            // Check BOTH intersection AND population criteria
            if (item.population >= min_population && item.bounds.intersects(query_rect))
            {
                results.push_back(item);
            }
        }
    }
    else
    { // Internal node
        // Internal node: Check which children's MBRs intersect the query rectangle
        // No population check needed here, as internal nodes don't store population directly.
        // We must descend into any relevant child to check its leaves eventually.
        for (const auto &child_ptr : node->children)
        {
            if (child_ptr && child_ptr->mbr.intersects(query_rect))
            {
                search_pop_recursive(child_ptr.get(), query_rect, min_population, results);
            }
        }
    }
}

// Recursive helper for printing the tree structure
// Uses the 'os' parameter passed down from print_structure
void RTree::print_node(std::ostream &os, const RTreeNode *node, int indent) const
{
    if (!node)
        return;

    std::string indent_str(indent * 2, ' '); // Create indentation string

    // Print node type, memory address (for debugging splits), MBR, and size
    os << indent_str << "[" << (node->is_leaf ? "LEAF" : "INTERNAL")
       << " @ " << static_cast<const void *>(node) // Print node address safely
       << "] MBR: ("
       << node->mbr.min_corner.x << "," << node->mbr.min_corner.y << ")-("
       << node->mbr.max_corner.x << "," << node->mbr.max_corner.y << ") "
       << "Size: " << node->size() << "\n";

    // Print contents based on node type
    if (node->is_leaf)
    {
        // Leaf node: Print details of each data item
        for (const auto &item : node->data_entries)
        {
            os << indent_str << "  - Item ID: " << item.id << ", Name: " << item.name
               << ", Pop: " << item.population << ", Bounds: ("
               << item.bounds.min_corner.x << "," << item.bounds.min_corner.y << ")-("
               << item.bounds.max_corner.x << "," << item.bounds.max_corner.y << ")\n";
        }
    }
    else
    {
        // Internal node: Recursively print each child node
        for (const auto &child : node->children)
        {
            if (child)
            {
                print_node(os, child.get(), indent + 1); // Increase indent for children
            }
            else
            {
                os << indent_str << "  (null child pointer)\n"; // Should ideally not happen
            }
        }
    }
}
