#include "rtree.h"
#include <iostream>
#include <vector>
#include <string>
#include <limits>    // Required for numeric_limits
#include <map>       // For country name to bounding box lookup
#include <string>    // For std::tolower, std::getline
#include <algorithm> // For std::transform
#include <ios>       // Required for std::ws
#include <fstream>   // Required for file input/output (ifstream, ofstream)
#include <sstream>   // Required for parsing lines (stringstream)
#include <stdexcept> // For runtime_error

// --- Configuration ---
const std::string input_data_filename = "input_data.csv"; // Input data file
const std::string output_csv_filename = "results.csv";    // Output results file

// --- Country Bounding Box Lookup ---
// Added "world" option
std::map<std::string, Rectangle> country_bounds = {
    {"united states", Rectangle(-125, 24, -66, 50)}, // Approx. continental US
    {"usa", Rectangle(-125, 24, -66, 50)},           // Alias
    {"canada", Rectangle(-141, 41, -52, 84)},        // Approx. Canada
    {"mexico", Rectangle(-118, 14, -97, 33)},        // Approx. Mexico
    {"china", Rectangle(73, 18, 135, 54)},           // Approx. China
    {"russia", Rectangle(19, 41, 180, 82)},          // Approx. Russia (very large)
    {"germany", Rectangle(5, 47, 16, 55)},           // Approx. Germany
    {"brazil", Rectangle(-74, -34, -34, 6)},         // Approx. Brazil
    {"world", Rectangle(-180, -90, 180, 90)}         // Entire world
};

// --- Function to Load Data from CSV ---
// Loads data, skips header, comments (#), and malformed lines.
void load_data_from_csv(const std::string &filename, RTree &tree)
{
    std::ifstream input_file(filename);
    std::string line;
    int line_number = 0;
    int items_loaded = 0;
    int items_skipped = 0;

    if (!input_file.is_open())
    {
        std::cerr << "ERROR: Could not open input data file: '" << filename << "'" << std::endl;
        throw std::runtime_error("Could not open input data file: " + filename);
    }
    std::cout << "\nLoading data from '" << filename << "'..." << std::endl; // Changed from DEBUG
    while (std::getline(input_file, line))
    {
        line_number++;
        // Skip header row
        if (line_number == 1)
        {
            if (line != "ID,Name,Population,MinX,MinY,MaxX,MaxY")
            {
                std::cerr << "Warning: CSV header mismatch. Expected 'ID,Name,Population,MinX,MinY,MaxX,MaxY', found '" << line << "'" << std::endl;
            }
            continue;
        }
        // Skip empty lines or comment lines
        if (line.empty() || line.find_first_not_of(" \t\n\v\f\r") == std::string::npos || line[0] == '#')
        {
            items_skipped++;
            continue;
        }
        // Parse comma-separated values
        std::stringstream ss(line);
        std::string segment;
        std::vector<std::string> parts;
        while (std::getline(ss, segment, ','))
        {
            // Trim whitespace
            segment.erase(0, segment.find_first_not_of(" \t\n\v\f\r"));
            segment.erase(segment.find_last_not_of(" \t\n\v\f\r") + 1);
            parts.push_back(segment);
        }
        // Validate column count
        if (parts.size() != 7)
        {
            std::cerr << "Warning: Skipping malformed line " << line_number << " (expected 7 columns, found " << parts.size() << "): " << line << std::endl;
            items_skipped++;
            continue;
        }
        // Convert and validate data
        try
        {
            int id = std::stoi(parts[0]);
            std::string name = parts[1];
            long population = std::stol(parts[2]);
            double min_x = std::stod(parts[3]);
            double min_y = std::stod(parts[4]);
            double max_x = std::stod(parts[5]);
            double max_y = std::stod(parts[6]);
            // Validate bounds and population
            if (min_x > max_x || min_y > max_y)
            {
                std::cerr << "Warning: Skipping line " << line_number << " (ID=" << id << ") due to invalid bounds (min > max)." << std::endl;
                items_skipped++;
                continue;
            }
            if (population < 0)
            {
                std::cerr << "Warning: Skipping line " << line_number << " (ID=" << id << ") due to negative population." << std::endl;
                items_skipped++;
                continue;
            }
            // Insert valid data into R-Tree
            tree.insert(DataItem(id, name, population, Rectangle(min_x, min_y, max_x, max_y)));
            items_loaded++;
        }
        catch (const std::invalid_argument &e)
        {
            std::cerr << "Warning: Skipping line " << line_number << " due to conversion error (invalid number format): " << e.what() << " - Line: " << line << std::endl;
            items_skipped++;
        }
        catch (const std::out_of_range &e)
        {
            std::cerr << "Warning: Skipping line " << line_number << " due to conversion error (number out of range): " << e.what() << " - Line: " << line << std::endl;
            items_skipped++;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Warning: Skipping line " << line_number << " due to error during processing/insertion: " << e.what() << std::endl;
            items_skipped++;
        }
        catch (...)
        {
            std::cerr << "Warning: Skipping line " << line_number << " due to an unknown error during processing." << std::endl;
            items_skipped++;
        }
    }
    input_file.close();
    // Print loading summary
    std::cout << "\nFinished loading data from '" << filename << "'." << std::endl;
    std::cout << "  Total lines processed: " << line_number << std::endl;
    std::cout << "  Items loaded successfully: " << items_loaded << std::endl;
    std::cout << "  Items skipped (comments/errors/empty): " << items_skipped << std::endl;
    if (items_loaded == 0 && line_number > 1)
    {
        std::cerr << "ERROR: No valid data items were loaded from the input file!" << std::endl;
    }
    else if (items_loaded == 0 && line_number <= 1)
    {
        std::cerr << "ERROR: Input file seems empty or contains only a header." << std::endl;
    }
}

// --- Input Functions ---
// Gets query bounds either via country name lookup (including "world") or manual input.
Rectangle get_query_rectangle_for_country()
{
    std::string country_name;
    // Updated prompt to include "World" and "manual"
    std::cout << "\nEnter country name (e.g., United States, China, World) or type 'manual' for coordinates: " << std::flush;
    std::getline(std::cin >> std::ws, country_name); // Read full line, skip leading whitespace
    // Convert to lowercase
    std::transform(country_name.begin(), country_name.end(), country_name.begin(),
                   [](unsigned char c)
                   { return std::tolower(c); });

    // Handle manual input request
    if (country_name == "manual")
    {
        std::cout << "Enter bounds manually.\n";
        double min_x, min_y, max_x, max_y;
        // Helper lambda for robust double input
        auto get_double = [](const std::string &prompt)
        {
            double value;
            while (true)
            {
                std::cout << prompt << std::flush;
                if (std::cin >> value)
                {
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Consume newline
                    return value;
                }
                else
                {
                    std::cout << "Invalid input. Please enter a number.\n";
                    std::cin.clear();
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                }
            }
        };
        // Get coordinates
        min_x = get_double("  Min X (e.g., longitude): ");
        min_y = get_double("  Min Y (e.g., latitude): ");
        max_x = get_double("  Max X (e.g., longitude): ");
        max_y = get_double("  Max Y (e.g., latitude): ");
        // Validate coordinates
        if (min_x > max_x || min_y > max_y)
        {
            std::cerr << "Warning: Invalid rectangle coordinates (min > max). Using as entered.\n";
        }
        return Rectangle(min_x, min_y, max_x, max_y);
    }

    // Look up country/world name in the map
    auto it = country_bounds.find(country_name);
    if (it != country_bounds.end())
    {
        // Found in map
        std::cout << "Found bounds for '" << country_name << "': ("
                  << it->second.min_corner.x << "," << it->second.min_corner.y << ")-("
                  << it->second.max_corner.x << "," << it->second.max_corner.y << ")\n";
        return it->second; // Return the corresponding rectangle
    }
    else
    {
        // Not found and not "manual"
        std::cout << "Input '" << country_name << "' not recognized as a predefined country or 'manual'. Please try again.\n";
        return get_query_rectangle_for_country(); // Ask again recursively
    }
}

// Gets the minimum population threshold from the user.
long get_population_threshold_from_user()
{
    long threshold;
    std::cout << "\nEnter minimum population threshold (e.g., 1000000): " << std::flush;
    while (true)
    {
        if (std::cin >> threshold && threshold >= 0)
        {                                                                       // Validate input
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Consume newline
            return threshold;
        }
        else
        {
            std::cout << "Invalid input. Please enter a non-negative integer.\n";
            std::cin.clear();                                                                  // Clear error flags
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');                // Discard bad input
            std::cout << "Enter minimum population threshold (e.g., 1000000): " << std::flush; // Re-prompt
        }
    }
}

// --- Main Function ---
int main()
{
    std::cout << "===== R-Tree Spatial Query Application =====\n";

    // 1. Create the R-Tree
    RTree spatial_index;

    // 2. Load Data (handle potential errors)
    try
    {
        load_data_from_csv(input_data_filename, spatial_index);
        // Exit if no data could be loaded
        if (spatial_index.empty())
        {
            std::cerr << "Error: R-Tree is empty after attempting to load data. Cannot perform query." << std::endl;
            return 1; // Indicate error
        }
    }
    catch (const std::runtime_error &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "An unexpected standard exception occurred during data loading: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "An unknown error occurred during data loading." << std::endl;
        return 1;
    }

    // 3. Get Query Parameters
    std::cout << "\n--- Define Query ---" << std::endl;
    Rectangle query_bounds = get_query_rectangle_for_country();
    long min_population = get_population_threshold_from_user();

    // 4. Perform Query
    std::cout << "\n--- Performing Query ---" << std::endl;
    std::cout << "Searching within bounds: ("
              << query_bounds.min_corner.x << "," << query_bounds.min_corner.y << ")-("
              << query_bounds.max_corner.x << "," << query_bounds.max_corner.y << ")"
              << " for population >= " << min_population << "\n";
    std::vector<DataItem> results = spatial_index.search_with_population(query_bounds, min_population);

    // 5. Write Results to CSV
    std::cout << "\n--- Writing Results to CSV File ---" << std::endl;
    std::ofstream output_file(output_csv_filename);
    if (!output_file.is_open())
    {
        std::cerr << "Error: Could not open file '" << output_csv_filename << "' for writing!" << std::endl;
        return 1;
    }
    // Write header
    output_file << "ID,Name,Population,MinX,MinY,MaxX,MaxY\n";
    // Write data rows
    if (results.empty())
    {
        std::cout << "No areas found matching the criteria. CSV file contains only the header.\n";
    }
    else
    {
        std::cout << "Found " << results.size() << " area(s) matching the criteria.\n";
        for (const auto &item : results)
        {
            output_file << item.id << "," << "\"" << item.name << "\"," << item.population << ","
                        << item.bounds.min_corner.x << "," << item.bounds.min_corner.y << ","
                        << item.bounds.max_corner.x << "," << item.bounds.max_corner.y << "\n";
        }
        std::cout << "Successfully wrote results to '" << output_csv_filename << "'." << std::endl;
    }
    output_file.close();

    return 0; // Indicate success
}