import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

def parser(file_name):
    parse = False
    latencies = []
    with open(file_name, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line == "Frontend":
                parse = False  # Stop parsing when "END of the attack" is found
            if parse:
                tokens = line.split()
                if len(tokens) >= 3 and tokens[2].isdigit():
                    latencies.append(int(tokens[2]))  # Append the third token as an integer
            if line == "Dump Begin":
                parse = True  # Start parsing when "START of the attack" is found
    return latencies

# get file name as argument
import sys
if len(sys.argv) > 2:
    file_name = sys.argv[1]
    figure_name = sys.argv[2]
else:
    print("Usage: python figure2_plotter.py <input_file_name> <figure_name>")
    sys.exit(1)


# Parse and prepare data
data = parser(file_name)
df = pd.DataFrame(data, columns=['Latency']).reset_index()
df.rename(columns={'index': 'Time'}, inplace=True)

# Plot setup
fig, ax = plt.subplots(figsize=(8, 3))

# Add background colors for y-axis ranges
ax.axhspan(1250, 3500, color="#71b9de", alpha=0.3, label="PRAC Back-off")
ax.axhspan(250, 1250, color="#ffd766", alpha=0.3, label="Periodic Refresh")
ax.axhspan(80, 250, color="#83d67a", alpha=0.3, label="Row Buffer Conflict")

# Plot data
sns.lineplot(
    data=df,
    x='Time',
    y='Latency',
    ax=ax,
    color="black",
    label="Latency",
    linewidth=1.5
)
#sns.scatterplot(data=df, x='Time', y='Latency', ax=ax, color="black", label="Latency", linewidth=0, s=25)

# Labeling
ax.set_xlabel('Memory Requests', fontsize=14)
ax.set_ylabel('Memory Request\nLatency (ns)', fontsize=14)

# Add grid lines to the y-axis
ax.grid(axis='y', linestyle='--', alpha=0.7)

# add legend without latency
legend_handles, legend_labels = ax.get_legend_handles_labels()
legend_labels = [label for label in legend_labels if label != 'Latency']



ax.legend(
    legend_handles,
    legend_labels,
    loc='upper center',
    fontsize=12,
    ncol=len(legend_labels),
    framealpha=1,
    bbox_to_anchor=(0.5, 1.26),
    fancybox=False,
    edgecolor="black",
    # add border to handles
    handlelength=1,
    handleheight=1,
    handletextpad=0.5,
)

# Adjust limits and ticks
plt.ylim(0, 1750)
plt.yscale('linear')


plt.xlim(1024, 1024+512)
plt.xticks(fontsize=12)
plt.yticks(fontsize=12)

# add y ticks at 0, 500, 1000, 1250, 1500, 2000
plt.yticks(np.arange(0, 1751, 500))

# add text between x=2300 and x=2500, y=1000
plt.text(1400, 1375, "<------- 255 memory requests ------->", fontsize=12, color="red", ha='center', va='center', style='italic')

# Tight layout for better spacing
plt.tight_layout()
# save to pdf
plt.savefig(figure_name, bbox_inches='tight', dpi=300)
# Show plot
#plt.show()
