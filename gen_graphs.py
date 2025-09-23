import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

def plot_broker_comparisons(csv_filepath):
    """
    Reads MQTT broker performance data from a CSV file and plots
    comparison graphs for CPU, Net Sent, and Net Recv.

    Args:
        csv_filepath (str): The path to the CSV file.
    """
    try:
        # Read the CSV data into a pandas DataFrame
        df = pd.read_csv(csv_filepath)
    except FileNotFoundError:
        print(f"Error: The file '{csv_filepath}' was not found.")
        return

    # Get the unique experiment numbers
    experiments = df['Experiment'].unique()

    # Set the visual style of the plots
    sns.set(style="darkgrid")

    # Define the metrics to plot
    metrics = ['CPU (%)', 'Net Sent (KB/s)', 'Net Recv (KB/s)']
    
    # Create a separate plot for each experiment
    for experiment in experiments:
        df_exp = df[df['Experiment'] == experiment]
        
        # Create a figure with 3 subplots (one for each metric)
        fig, axes = plt.subplots(len(metrics), 1, figsize=(12, 18), sharex=True)
        fig.suptitle(f'Broker Performance Comparison - Experiment {experiment}', fontsize=16)

        # Plot each metric in a separate subplot
        for i, metric in enumerate(metrics):
            ax = axes[i]
            sns.lineplot(data=df_exp, x='Time', y=metric, hue='Broker', ax=ax, marker='o')
            ax.set_title(f'{metric} over Time')
            ax.set_ylabel(metric)
            if i == len(metrics) - 1: # Only show x-label on the bottom plot
                ax.set_xlabel('Time (seconds)')
            else:
                ax.set_xlabel('')

        # Improve layout and display the plots
        plt.tight_layout(rect=[0, 0, 1, 0.96])
        plt.show()

if __name__ == '__main__':
    # The name of the CSV file containing the data
    csv_file = 'mqtt_results.csv'
    plot_broker_comparisons(csv_file)
