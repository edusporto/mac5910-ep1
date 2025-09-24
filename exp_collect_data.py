import psutil
import time
import statistics
import argparse
import pandas as pd
from typing import List, Dict, Any

def find_target_processes(names: List[str]) -> List[psutil.Process]:
    """Finds running processes that match any of the given names."""
    target_procs = []
    for proc in psutil.process_iter(['pid', 'name']):
        if proc.info['name'] in names:
            target_procs.append(proc)
    return target_procs

def run_monitor(process_names: List[str], duration: int, interval: int) -> List[Dict[str, Any]]:
    """
    Monitors CPU and Network usage for a given set of processes over a specified duration.

    Args:
        process_names (List[str]): A list of process names to monitor.
        duration (int): The total time to monitor in seconds.
        interval (int): The sampling interval in seconds.

    Returns:
        List[Dict[str, Any]]: A list of dictionaries, where each dictionary
                              represents a second of collected performance data.
    """
    print(f"Monitoring for {duration} seconds, with a {interval}-second interval.")
    
    collected_data = []
    
    # Initialize network counters and monotonic time for accurate rate calculation
    last_net_io = psutil.net_io_counters()
    last_time = time.monotonic()
    start_time = time.monotonic()

    time_step = 0
    try:
        while time.monotonic() - start_time < duration:
            time.sleep(interval)
            time_step += 1
            
            # --- CPU Monitoring ---
            target_procs = find_target_processes(process_names)
            if not target_procs:
                print(f"No target processes found ({', '.join(process_names)}). Waiting...", end='\r')
                continue
            
            total_cpu = 0
            for proc in target_procs:
                try:
                    # Get CPU percent over the interval for more accurate reading
                    total_cpu += proc.cpu_percent()
                    # Include CPU usage of child processes
                    for child in proc.children(recursive=True):
                        total_cpu += child.cpu_percent()
                except (psutil.NoSuchProcess, psutil.AccessDenied):
                    # Ignore processes that have terminated or we can't access
                    continue
            
            # --- Network Monitoring (System-Wide) ---
            current_net_io = psutil.net_io_counters()
            current_time = time.monotonic()
            elapsed_time = current_time - last_time

            sent_rate_kbs = 0
            recv_rate_kbs = 0
            if elapsed_time > 0:
                # Calculate rates in KB/s
                sent_rate_kbs = (current_net_io.bytes_sent - last_net_io.bytes_sent) / elapsed_time / 1024
                recv_rate_kbs = (current_net_io.bytes_recv - last_net_io.bytes_recv) / elapsed_time / 1024

            last_net_io = current_net_io
            last_time = current_time
            
            # Create a dictionary for the current time step's data
            current_record = {
                'Time': time_step,
                'Active Procs': len(target_procs),
                'CPU (%)': round(total_cpu, 2),
                'Net Sent (KB/s)': round(sent_rate_kbs, 2),
                'Net Recv (KB/s)': round(recv_rate_kbs, 2)
            }
            collected_data.append(current_record)

            print(f"Time: {time_step}s, Active Procs: {len(target_procs)}, CPU: {total_cpu:.2f}%, "
                  f"Net Sent: {sent_rate_kbs:.2f} KB/s, "
                  f"Net Recv: {recv_rate_kbs:.2f} KB/s")

    except KeyboardInterrupt:
        print("\n🛑 Monitor stopped manually.")
    
    return collected_data

def main():
    """
    Main function to orchestrate the entire experiment workflow.
    """
    parser = argparse.ArgumentParser(
        description="Run a series of monitoring experiments for MQTT brokers and save results to CSV.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument(
        '--duration', type=int, default=30, help="How long to monitor each run in seconds."
    )
    parser.add_argument(
        '--interval', type=int, default=1, help="How often to sample data in seconds."
    )
    args = parser.parse_args()

    # --- Experiment Configuration ---
    experiments = {
        1: "Low Load Test",
        2: "Medium Load Test",
        3: "High Load Test"
    }
    brokers = {
        'Mine': ['server'],  # Process name for your solution
        'MQTT': ['mosquitto'] # Process name for Mosquitto
    }
    observation_runs = 3
    all_results = []

    print("🚀 Starting MQTT Broker Performance Experiment Suite 🚀")

    try:
        for exp_num, exp_name in experiments.items():
            print(f"\n=======================================================")
            print(f"                PREPARING EXPERIMENT {exp_num}")
            print(f"                  ({exp_name})")
            print(f"=======================================================")

            for broker_name, process_names in brokers.items():
                print(f"\n--- Broker: {broker_name} ---")

                run_num = 1
                while run_num <= observation_runs:
                    input(f">>> Ready for Observation Run {run_num}/{observation_runs} for '{broker_name}'. Press Enter to begin...")
                    print(f"--- Starting Observation Run {run_num}/{observation_runs} for '{broker_name}' ---")
                    
                    run_data = run_monitor(process_names, args.duration, args.interval)
                    print(f"--- Observation Run {run_num} for '{broker_name}' COMPLETE. ---")

                    # --- Confirmation Step ---
                    while True:
                        confirmation = input(">> Was this run successful and should be saved? (y/n): ").lower().strip()
                        if confirmation in ['y', 'yes']:
                            # Add experiment metadata to each record
                            for record in run_data:
                                record['ObservationSet'] = run_num
                                record['Experiment'] = exp_num
                                record['Broker'] = broker_name
                                all_results.append(record)
                            print("✅ Data saved. Proceeding to the next run.")
                            run_num += 1
                            break
                        elif confirmation in ['n', 'no']:
                            print("❌ Data discarded. Repeating the current observation run.")
                            break
                        else:
                            print("Invalid input. Please enter 'y' for yes or 'n' for no.")

    except KeyboardInterrupt:
        print("\n🛑 Experiment suite aborted by user.")
    finally:
        if not all_results:
            print("\nNo data was collected. Exiting without generating a CSV.")
            return

        print("\n\n=======================================================")
        print("✅ All experiments complete. Generating final CSV report...")
        
        # Convert the list of dictionaries to a Pandas DataFrame
        results_df = pd.DataFrame(all_results)
        
        # Ensure columns are in the desired order
        column_order = [
            'ObservationSet', 'Experiment', 'Broker', 'Time', 
            'Active Procs', 'CPU (%)', 'Net Sent (KB/s)', 'Net Recv (KB/s)'
        ]
        results_df = results_df[column_order]
        
        # Save the DataFrame to a CSV file
        output_filename = 'final_experiment_results.csv'
        results_df.to_csv(output_filename, index=False)
        
        print(f"📊 Success! Results for all runs saved to '{output_filename}'")
        print("=======================================================")


if __name__ == "__main__":
    main()

