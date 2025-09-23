import psutil
import time
import statistics
import argparse

def find_target_processes(names=['mosquitto', 'server']):
    """Finds running processes that match the given names."""
    target_procs = []
    for proc in psutil.process_iter(['pid', 'name']):
        if proc.info['name'] in names:
            target_procs.append(proc)
    return target_procs

def main():
    parser = argparse.ArgumentParser(
        description="Monitor CPU and Network usage for specific broker processes.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument(
        '--processes',
        nargs='+',
        default=['mosquitto', 'server'],
        help="Names of the broker processes to monitor (e.g., server mosquitto)."
    )
    parser.add_argument(
        '--duration',
        type=int,
        default=30,
        help="How long to monitor in seconds."
    )
    parser.add_argument(
        '--interval',
        type=int,
        default=1,
        help="How often to sample data in seconds."
    )
    args = parser.parse_args()

    print(f"🕵️  Starting resource monitor for {args.processes}...")
    print(f"Monitoring for {args.duration} seconds, with a {args.interval}-second interval.")
    
    cpu_percentages = []
    net_sent_rates = []
    net_recv_rates = []

    # Get initial network counters and time
    last_net_io = psutil.net_io_counters()
    # --- CHANGE: Use time.monotonic() for portable timing ---
    last_time = time.monotonic() 
    start_time = time.time()

    try:
        # We'll run the loop a bit longer to ensure we capture the full duration
        while time.time() - start_time < args.duration:
            time.sleep(args.interval)
            
            # --- CPU Monitoring ---
            target_procs = find_target_processes(args.processes)
            if not target_procs:
                print(f"No target processes found. Waiting...", end='\r')
                continue
            
            total_cpu = 0
            for proc in target_procs:
                try:
                    total_cpu += proc.cpu_percent()
                    for child in proc.children(recursive=True):
                        total_cpu += child.cpu_percent()
                except psutil.NoSuchProcess:
                    continue
            
            cpu_percentages.append(total_cpu)

            # --- Network Monitoring (System-Wide) ---
            current_net_io = psutil.net_io_counters()
            # --- CHANGE: Calculate elapsed time manually ---
            current_time = time.monotonic()
            elapsed_time = current_time - last_time

            if elapsed_time > 0:
                sent_rate = (current_net_io.bytes_sent - last_net_io.bytes_sent) / elapsed_time
                recv_rate = (current_net_io.bytes_recv - last_net_io.bytes_recv) / elapsed_time
                net_sent_rates.append(sent_rate)
                net_recv_rates.append(recv_rate)

            # --- CHANGE: Update last time and last net io count ---
            last_net_io = current_net_io
            last_time = current_time
            
            # Use a default value if rates list is empty to prevent index error
            last_sent_rate = net_sent_rates[-1] if net_sent_rates else 0
            last_recv_rate = net_recv_rates[-1] if net_recv_rates else 0

            print(f"Active Procs: {len(target_procs)}, CPU: {total_cpu:.2f}%, "
                  f"Net Sent: {last_sent_rate/1024:.2f} KB/s, "
                  f"Net Recv: {last_recv_rate/1024:.2f} KB/s", end='\r')

    except KeyboardInterrupt:
        print("\n🛑 Monitor stopped manually.")
    finally:
        print("\n\n--- 📊 Final Results ---")
        if cpu_percentages:
            avg_cpu = statistics.mean(cpu_percentages)
            std_cpu = statistics.stdev(cpu_percentages) if len(cpu_percentages) > 1 else 0
            print(f"CPU Usage (%):      Avg: {avg_cpu:.2f}  | StdDev: {std_cpu:.2f}")
        else:
            print("No CPU data collected. Was the broker running?")

        if net_sent_rates:
            avg_sent = statistics.mean(net_sent_rates)
            std_sent = statistics.stdev(net_sent_rates) if len(net_sent_rates) > 1 else 0
            print(f"Net Sent (KB/s):    Avg: {avg_sent/1024:.2f} | StdDev: {std_sent/1024:.2f}")
            
            avg_recv = statistics.mean(net_recv_rates)
            std_recv = statistics.stdev(net_recv_rates) if len(net_recv_rates) > 1 else 0
            print(f"Net Received (KB/s): Avg: {avg_recv/1024:.2f} | StdDev: {std_recv/1024:.2f}")
        else:
            print("No Network data collected.")

if __name__ == "__main__":
    main()