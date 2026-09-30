import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

df = pd.read_csv('benchmark.csv')

# Average timings across repeated runs make the trend less choppy.
x = df['file_size_bytes']
plt.plot(x, df['function_time_seconds'], linewidth=2,
         label='C function copy')
plt.plot(x, df['system_call_time_seconds'], linewidth=2,
         label='System-call copy')

plt.xlabel('File Size (Bytes)')
plt.ylabel('Time Taken (Seconds)')

plt.title('Impact of File Size on Copy Performance (5-run averages)')
plt.grid(True, alpha=0.3)
plt.legend()
plt.tight_layout()

plt.savefig('performance_graph.png', dpi=150)
