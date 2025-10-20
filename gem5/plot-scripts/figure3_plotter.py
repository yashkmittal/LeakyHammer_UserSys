import pandas as pd
import matplotlib.pyplot as plt
import warnings

# get file name from argument
import sys
if len(sys.argv) > 2:
    file_name = sys.argv[1]
    figure_name = sys.argv[2]
else:
    print("Usage: python figure3_plotter.py <input_file_name> <figure_name>")
    sys.exit(1)

# Suppress warnings
warnings.filterwarnings("ignore")

with open(file_name, "r", encoding="utf-8") as f:
    lines = f.readlines()
  
df_receiver = pd.DataFrame(columns=['index', 'recv'])
df_sender = pd.DataFrame(columns=['index', 'value'])

# get string in line "[SEND] Binary: <string>"
for line in lines:
    if "[SEND] Binary:" in line:
        sender = (line.split("[SEND] Binary:")[1].strip())
        for i in range(len(sender)):
            # insert the bit to the dataframe
            df_sender = pd.concat([df_sender, pd.DataFrame([{'index': i, 'value': int(sender[i])}])], ignore_index=True)
    elif "[RECV] Binary:" in line:
        receiver = (line.split("[RECV] Binary:")[1].strip())
        for i in range(len(receiver)):
            # insert the bit to the dataframe
            df_receiver = pd.concat([df_receiver, pd.DataFrame([{'index': i, 'recv': int(receiver[i])}])], ignore_index=True)

# Merge the two dataframes on 'index'
df = pd.merge(df_sender, df_receiver, on='index', how='outer')

# Plot setup
fig, ax = plt.subplots(figsize=(9, 4))

# Increase font size
plt.rcParams.update({'font.size': 14})

# Define background colors
for i, value in enumerate(df["value"]):
    color = "sandybrown" if value == 0 else "cornflowerblue"
    ax.axvspan(i - 0.5, i + 0.5, color=color, alpha=0.3, label="0" if value == 0 else "1")

# Plot the RFM values
ax.plot(df["index"], df["recv"], marker="o", color="black", label="recv")

# Set x-ticks to align with bold lines
xticks_positions = [(i - 1) + 0.5 for i in range(0, len(df) + 1, 8)]
ax.set_xticks(xticks_positions)
ax.set_xticklabels([str(int(pos + 0.5)) for pos in xticks_positions], fontsize=12)

# Labeling
ax.set_xlabel("Transmission Window", fontsize=14)
ax.set_ylabel("Back-Off Detected by Receiver", fontsize=14)

# make yticks size 12
plt.yticks(fontsize=12)

# Add bold vertical lines for clarity
for i in range(0, len(df) + 1, 8):
    ax.axvline((i - 1) + 0.5, color="black", linewidth=1)

# Add letter "I" under the first 8 values
x_pos = (xticks_positions[0] + xticks_positions[1]) / 2  # Center of the first region
ax.text(x_pos, -0.17, "(M)", ha='center', va='top', fontsize=16, color='red')

x_pos = (xticks_positions[1] + xticks_positions[2]) / 2  # Center of the first region
ax.text(x_pos, -0.17, "(I)", ha='center', va='top', fontsize=16, color='red')

x_pos = (xticks_positions[2] + xticks_positions[3]) / 2  # Center of the first region
ax.text(x_pos, -0.17, "(C)", ha='center', va='top', fontsize=16, color='red')

x_pos = (xticks_positions[3] + xticks_positions[4]) / 2  # Center of the first region
ax.text(x_pos, -0.17, "(R)", ha='center', va='top', fontsize=16, color='red')

x_pos = (xticks_positions[4] + xticks_positions[5]) / 2  # Center of the first region
ax.text(x_pos, -0.17, "(O)", ha='center', va='top', fontsize=16, color='red')

# Add legend with title inline
handles, labels = ax.get_legend_handles_labels()
by_label = dict(zip(labels, handles))  # Remove duplicates in the legend
by_label.pop("recv")  # Remove recv from legend
legend_labels = ["Bit Value"] + list(by_label.keys())
legend_handles = [plt.Line2D([0], [0], color="none")] + list(by_label.values())
ax.legend(
    legend_handles,
    legend_labels,
    loc='upper center',
    fontsize=12,
    ncol=len(legend_labels),
    framealpha=1,
    bbox_to_anchor=(0.5, 1.15),
    edgecolor="black",
    borderpad=0.3,
    fancybox=False,
    labelspacing=0.5,
)

# Set y-limits
plt.ylim(-0.15, 1.15)
plt.yticks([0, 1], ["0", "1"], fontsize=12)

# set xlim
plt.xlim(-1, len(df))

# Show plot
plt.tight_layout()
# save to pdf
plt.savefig(figure_name, bbox_inches='tight', dpi=300)