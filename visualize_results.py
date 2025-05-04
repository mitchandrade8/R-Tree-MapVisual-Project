import pandas as pd
import geopandas as gpd
import matplotlib.pyplot as plt
from shapely.geometry import Polygon, Point # Added Point for centroid calculation
import contextily as cx # For adding basemaps
import os # To check if file exists
import locale # For formatting population numbers
from adjustText import adjust_text # Import the adjustText library

# --- Configuration ---
csv_filename = 'results.csv'
output_plot_filename = 'population_map_adjusted_labels_lines.png' # Changed output name

# --- Main Script ---

def create_bounding_box(row):
    """Creates a Shapely Polygon geometry from bounding box coordinates."""
    min_x, min_y, max_x, max_y = row['MinX'], row['MinY'], row['MaxX'], row['MaxY']
    # Create polygon coordinates in counter-clockwise order
    return Polygon([(min_x, min_y), (max_x, min_y), (max_x, max_y), (min_x, max_y), (min_x, min_y)])

def format_population(pop_number):
    """Formats population number for display (e.g., 1.2M, 850K)."""
    if pop_number >= 1000000:
        return f"{pop_number / 1000000:.1f}M" # Display in millions with 1 decimal place
    elif pop_number >= 1000:
         return f"{pop_number / 1000:.0f}K" # Display in thousands
    else:
        return str(pop_number) # Display small numbers as is

def plot_map():
    """Reads the CSV, creates geometries, and plots the map with auto-adjusted labels and clearer lines."""

    # Check if the CSV file exists
    if not os.path.exists(csv_filename):
        print(f"Error: File '{csv_filename}' not found.")
        print("Please run the C++ program first to generate 'results.csv'.")
        return

    try:
        # Read the CSV data using pandas
        df = pd.read_csv(csv_filename)
        print(f"Read {len(df)} records from {csv_filename}")

        # Check if necessary columns exist
        required_cols = ['MinX', 'MinY', 'MaxX', 'MaxY', 'Name', 'Population']
        if not all(col in df.columns for col in required_cols):
            print(f"Error: CSV file must contain columns: {', '.join(required_cols)}")
            return

        # Load world map data for background context
        try:
            world = gpd.read_file(gpd.datasets.get_path('naturalearth_lowres'))
            print("Loaded world map data for context.")
        except Exception as e:
            print(f"Warning: Could not load world map data. Proceeding without country outlines. Error: {e}")
            world = None


        # Check if dataframe is empty after loading world map
        if df.empty:
            print("CSV file is empty. No data to plot.")
            # Plot world map context if the file was empty but existed
            if world is not None:
                 fig, ax = plt.subplots(1, 1, figsize=(12, 8))
                 world.plot(ax=ax, color='lightgray', edgecolor='black')
                 ax.set_title("Query returned no results (World Map Context)")
                 ax.set_xlabel("Longitude")
                 ax.set_ylabel("Latitude")
                 try:
                     cx.add_basemap(ax, crs=world.crs.to_string(), source=cx.providers.CartoDB.Positron)
                 except Exception as e:
                     print(f"Warning: Could not add basemap for empty results plot. Error: {e}")

                 plt.tight_layout()
                 plt.savefig(output_plot_filename)
                 print(f"Saved empty world map context to {output_plot_filename}")
            else:
                print("Cannot plot empty map as world data failed to load.")
            return


        # Create geometry objects (bounding boxes) for each row
        geometries = df.apply(create_bounding_box, axis=1)

        # Create a GeoDataFrame
        gdf = gpd.GeoDataFrame(df, geometry=geometries, crs="EPSG:4326")
        print("Created GeoDataFrame.")

        # --- Plotting ---
        print("Generating plot...")
        fig, ax = plt.subplots(1, 1, figsize=(15, 10))

        # Plot world map first as a base layer
        if world is not None:
             world_proj = world.to_crs(gdf.crs)
             world_proj.plot(ax=ax, color='#E0E0E0', edgecolor='darkgrey', linewidth=0.5, zorder=1)

        # Plot the bounding boxes from the query results on top
        gdf.plot(ax=ax, edgecolor='red', facecolor='red', alpha=0.4, linewidth=1.5, label='High Population Areas', zorder=3)

        # --- Create labels using ax.text but store them for adjustText ---
        texts = []
        for idx, row in gdf.iterrows():
            point = row.geometry.representative_point()
            pop_str = format_population(row["Population"])
            label = f'{row["Name"]}\n({pop_str})'
            # Create text object, store in list 'texts'
            texts.append(ax.text(point.x, point.y, label, fontsize=6, ha='center', va='bottom', color='black', zorder=5))

        # Add a basemap using contextily
        try:
             cx.add_basemap(ax, crs=gdf.crs.to_string(), source=cx.providers.CartoDB.Positron, zorder=2)
             print("Added CartoDB Positron basemap.")
        except Exception as e:
            print(f"Warning: Could not add basemap. Plotting without it. Error: {e}")

        # --- Adjust text labels to avoid overlap ---
        print("Adjusting text labels...")
        # adjust_text will automatically move labels and optionally draw lines/arrows
        adjust_text(texts,
                    ax=ax, # Pass the axes object
                    # Make arrows black and slightly thicker
                    arrowprops=dict(arrowstyle='-', color='black', lw=0.7)) # Changed color and lw
        print("Finished adjusting labels.")

        # --- Final Touches ---
        min_pop_found = gdf["Population"].min()
        ax.set_title(f'High Population Areas (> {min_pop_found:,}) Found by Query')
        ax.set_xlabel("Longitude")
        ax.set_ylabel("Latitude")

        # Adjust plot limits
        minx, miny, maxx, maxy = gdf.total_bounds
        padding_x = (maxx - minx) * 0.05
        padding_y = (maxy - miny) * 0.05
        ax.set_xlim(minx - padding_x, maxx + padding_x)
        ax.set_ylim(miny - padding_y, maxy + padding_y)

        plt.tight_layout()
        plt.savefig(output_plot_filename)
        print(f"Successfully saved map to {output_plot_filename}")

    except FileNotFoundError:
        print(f"Error: File '{csv_filename}' not found. Please create it from the C++ program output.")
    except pd.errors.EmptyDataError:
        print(f"Error: File '{csv_filename}' is empty.")
    except ImportError:
         print("\nError: The 'adjustText' library is required but not found.")
         print("Please install it using: pip install adjustText")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")

# --- Run the plotting function ---
if __name__ == "__main__":
    plot_map()


