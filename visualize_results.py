import pandas as pd
import geopandas as gpd
import matplotlib.pyplot as plt
from shapely.geometry import Polygon, Point
import contextily as cx
import os
import locale
from adjustText import adjust_text # Import the adjustText library

# --- Configuration ---
csv_filename = 'results.csv'
output_plot_filename = 'population_map_conditional_labels.png' # Changed output name
# --- Path to the downloaded Natural Earth shapefile ---
natural_earth_shp_path = os.path.join('natural_earth_data', 'ne_110m_admin_0_countries.shp')

# --- Main Script ---

def create_bounding_box(row):
    """Creates a Shapely Polygon geometry from bounding box coordinates."""
    min_x, min_y, max_x, max_y = row['MinX'], row['MinY'], row['MaxX'], row['MaxY']
    return Polygon([(min_x, min_y), (max_x, min_y), (max_x, max_y), (min_x, max_y), (min_x, min_y)])

def format_population(pop_number):
    """Formats population number for display (e.g., 1.2M, 850K)."""
    if pop_number >= 1000000:
        return f"{pop_number / 1000000:.1f}M"
    elif pop_number >= 1000:
         return f"{pop_number / 1000:.0f}K"
    else:
        return str(pop_number)

def plot_map():
    """Reads the CSV, creates geometries, and plots the map with labels only for zoomed views."""

    # Check if the results CSV file exists
    if not os.path.exists(csv_filename):
        print(f"Error: File '{csv_filename}' not found.")
        print("Please run the C++ program first to generate 'results.csv'.")
        return

    # Check if the Natural Earth shapefile exists
    if not os.path.exists(natural_earth_shp_path):
         print(f"Error: Natural Earth shapefile not found at '{natural_earth_shp_path}'.")
         print("Please download the '110m Admin 0 – Countries' shapefile from")
         print("https://www.naturalearthdata.com/downloads/110m-cultural-vectors/")
         print("and extract its contents into a subfolder named 'natural_earth_data'.")
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

        # Load world map data from the local shapefile
        try:
            world = gpd.read_file(natural_earth_shp_path)
            print(f"Loaded world map data from '{natural_earth_shp_path}'.")
        except Exception as e:
            print(f"Error: Could not load world map shapefile. Proceeding without country outlines. Error: {e}")
            world = None # Set world to None if loading fails

        # Check if results dataframe is empty
        if df.empty:
            print("CSV file is empty. No data to plot.")
            # Plot world map context if the file was empty but existed
            if world is not None:
                 fig, ax = plt.subplots(1, 1, figsize=(12, 8))
                 world_crs_string = world.crs.to_string() if world.crs else "EPSG:4326"
                 world.plot(ax=ax, color='lightgray', edgecolor='black')
                 ax.set_title("Query returned no results (World Map Context)")
                 ax.set_xlabel("Longitude")
                 ax.set_ylabel("Latitude")
                 try:
                     cx.add_basemap(ax, crs=world_crs_string, source=cx.providers.CartoDB.Positron)
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

        # Create a GeoDataFrame for the results
        gdf = gpd.GeoDataFrame(df, geometry=geometries, crs="EPSG:4326") # Assume WGS84
        print("Created GeoDataFrame for results.")

        # --- Determine if it's a world view ---
        query_is_world = False
        if not gdf.empty:
             minx_data, miny_data, maxx_data, maxy_data = gdf.total_bounds
             # Heuristic: if longitude span is large (>300) assume world view
             if maxx_data - minx_data > 300:
                 query_is_world = True
                 print("Detected world view based on data extent.")

        # --- Plotting ---
        print("Generating plot...")
        fig, ax = plt.subplots(1, 1, figsize=(15, 10))

        # Plot world map first as a base layer
        if world is not None:
             world_proj = world.to_crs(gdf.crs)
             world_proj.plot(ax=ax, color='#E0E0E0', edgecolor='darkgrey', linewidth=0.5, zorder=1)

        # Plot the bounding boxes from the query results on top
        gdf.plot(ax=ax, edgecolor='red', facecolor='red', alpha=0.4, linewidth=1.5, label='High Population Areas', zorder=3)

        # Add a basemap using contextily
        try:
             cx.add_basemap(ax, crs=gdf.crs.to_string(), source=cx.providers.CartoDB.Positron, zorder=2)
             print("Added CartoDB Positron basemap.")
        except Exception as e:
            print(f"Warning: Could not add basemap. Plotting without it. Error: {e}")

        # --- Conditionally add and adjust labels ---
        if not query_is_world:
            print("Zoomed view detected. Adding and adjusting labels...")
            texts = []
            for idx, row in gdf.iterrows():
                point = row.geometry.representative_point()
                pop_str = format_population(row["Population"])
                label = f'{row["Name"]}\n({pop_str})'
                # Slightly smaller font size
                texts.append(ax.text(point.x, point.y, label, fontsize=5, ha='center', va='bottom', color='black', zorder=5))

            # Adjust text labels to avoid overlap
            print("Adjusting text labels (this may take a moment)...")
            adjust_text(texts,
                        ax=ax,
                        force_text=(0.5, 0.5), # Repulsion force
                        lim=300, # Iteration limit
                        arrowprops=dict(arrowstyle='-', color='black', lw=0.7))
            print("Finished adjusting labels.")
        else:
            print("World view detected. Skipping labels for clarity.")


        # --- Final Touches ---
        min_pop_found = gdf["Population"].min() if not gdf.empty else 0
        ax.set_title(f'High Population Areas (> {min_pop_found:,}) Found by Query')
        ax.set_xlabel("Longitude")
        ax.set_ylabel("Latitude")

        # Adjust plot limits based on the world extent if world query, or data extent otherwise
        if query_is_world:
             ax.set_xlim(-180, 180)
             ax.set_ylim(-70, 90) # Limit vertical extent slightly for better presentation
        elif not gdf.empty:
             minx, miny, maxx, maxy = gdf.total_bounds
             padding_x = (maxx - minx) * 0.05
             padding_y = (maxy - miny) * 0.05
             ax.set_xlim(minx - padding_x, maxx + padding_x)
             ax.set_ylim(miny - padding_y, maxy + padding_y)
        # Else: Keep default limits if gdf is empty but world map was plotted


        plt.tight_layout()
        plt.savefig(output_plot_filename)
        print(f"Successfully saved map to {output_plot_filename}")

    except FileNotFoundError as e:
         if csv_filename in str(e):
             print(f"Error: File '{csv_filename}' not found. Please create it from the C++ program output.")
         else:
             print(f"An unexpected FileNotFoundError occurred: {e}")
    except pd.errors.EmptyDataError:
        print(f"Error: File '{csv_filename}' is empty.")
    except ImportError as e:
         if 'adjustText' in str(e):
             print("\nError: The 'adjustText' library is required but not found.")
             print("Please install it using: pip install adjustText")
         elif 'geopandas' in str(e):
              print("\nError: The 'geopandas' library is required but not found.")
              print("Please install it using: pip install geopandas")
         else:
              print(f"An ImportError occurred: {e}")

    except Exception as e:
        print(f"An unexpected error occurred: {e}")

# --- Run the plotting function ---
if __name__ == "__main__":
    plot_map()
