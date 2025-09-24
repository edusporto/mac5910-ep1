import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

def plot_mean_std_comparison(csv_filepath):
    """
    Reads MQTT broker performance data, calculates the mean and standard
    deviation across different observation sets, and plots comparison
    graphs with confidence intervals. This version includes logic to
    scale the time axis for incomplete runs.

    Args:
        csv_filepath (str): The path to the CSV file.
    """
    try:
        # Load the dataset
        df = pd.read_csv(csv_filepath)
    except FileNotFoundError:
        print(f"Error: The file '{csv_filepath}' was not found.")
        return

    # Define the metrics to be plotted
    metrics = {
        'CPU (%)': 'CPU Usage (%)',
        'Net Sent (KB/s)': 'Network Sent (KB/s)',
        'Net Recv (KB/s)': 'Network Received (KB/s)'
    }
    
    # Get the unique experiment numbers
    experiments = sorted(df['Experiment'].unique())

    # Set the visual style for the plots
    sns.set(style="darkgrid")
    
    # Generate a plot for each experiment
    for experiment in experiments:
        print(f"Generating plots for Experiment {experiment}...")
        
        df_exp = df[df['Experiment'] == experiment]

        # --- MODIFICATION START: Aggregate data first, then scale time axis ---
        # Calculate statistics for each broker separately before any scaling
        brokers = df_exp['Broker'].unique()
        broker_stats = {}
        for broker in brokers:
            broker_df = df_exp[df_exp['Broker'] == broker]
            # Group by the original time to get the mean performance curve
            stats = broker_df.groupby('Time').agg({
                'CPU (%)': ['mean', 'std'],
                'Net Sent (KB/s)': ['mean', 'std'],
                'Net Recv (KB/s)': ['mean', 'std']
            }).reset_index()
            # Flatten the multi-level column headers
            stats.columns = ['_'.join(col).strip() if col[1] else col[0] for col in stats.columns.values]
            broker_stats[broker] = stats

        # Special handling for Experiment 3: stretch the 'Mine' broker's time axis
        if experiment == 3 and 'Mine' in broker_stats and 'MQTT' in broker_stats:
            print("Applying time scaling for 'Mine' broker in Experiment 3...")
            mine_stats = broker_stats['Mine']
            mqtt_stats = broker_stats['MQTT']

            source_max_time = mine_stats['Time'].max()
            target_max_time = mqtt_stats['Time'].max()

            # Check if stretching is needed and possible
            if source_max_time < target_max_time and source_max_time > 0:
                scaling_factor = target_max_time / source_max_time
                # Apply scaling to the 'Time' column of the aggregated data
                mine_stats['Time'] = mine_stats['Time'] * scaling_factor
                broker_stats['Mine'] = mine_stats  # Update the dictionary with the scaled data
        # --- MODIFICATION END ---
        
        # Create a figure with a subplot for each metric
        fig, axes = plt.subplots(len(metrics), 1, figsize=(14, 20), sharex=True)
        fig.suptitle(f'Mean Performance with Standard Deviation - Experiment {experiment}', fontsize=18, weight='bold')

        palette = sns.color_palette("deep", len(brokers))

        # Plot each metric in its respective subplot
        for i, (metric_col, metric_title) in enumerate(metrics.items()):
            ax = axes[i]
            
            # Plot data for each broker using the pre-calculated stats
            for j, broker in enumerate(brokers):
                # This now uses the potentially time-scaled data for 'Mine' in Exp 3
                broker_data = broker_stats[broker]
                
                # Extract mean and std values, handling potential NaNs by filling with 0
                mean_values = broker_data[f'{metric_col}_mean']
                std_values = broker_data[f'{metric_col}_std'].fillna(0)
                time_values = broker_data['Time']
                
                # Plot the mean line
                ax.plot(time_values, mean_values, label=f'{broker} (Mean)', color=palette[j], marker='o', linestyle='-')
                
                # Add the confidence interval (standard deviation) as a shaded area
                ax.fill_between(time_values, 
                                mean_values - std_values, 
                                mean_values + std_values, 
                                color=palette[j], 
                                alpha=0.2, 
                                label=f'{broker} (Std Dev)')

            ax.set_title(metric_title, fontsize=14)
            ax.set_ylabel(metric_col, fontsize=12)
            
            # Create a combined legend
            handles, labels = ax.get_legend_handles_labels()
            # This logic ensures the legend doesn't have duplicate Std Dev entries
            by_label = dict(zip(labels, handles))
            ax.legend(by_label.values(), by_label.keys(), title="Broker")

        # Set the x-axis label only on the bottom plot
        axes[-1].set_xlabel('Time (seconds)', fontsize=12)
        
        # Adjust layout and display the plot
        plt.tight_layout(rect=[0, 0.03, 1, 0.96])
        plt.show()

if __name__ == '__main__':
    # Specify the path to your new CSV file
    csv_file_path = 'mqtt_results.csv'
    plot_mean_std_comparison(csv_file_path)

