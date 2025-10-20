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

# ignore warnings
warnings.filterwarnings("ignore")


df_acts = pd.DataFrame(columns=['index', 'RFM'])
df_bits = pd.DataFrame(columns=['index', 'value'])
act_id = 0

it = 0

with open(file_name, 'r') as f:
    for line in f:
        # if line contains "[RECV] ACTs:" get the line
        if "[RECV] Received" in line:
            try:
                # the format: [RECV] Received: <bit> (<num_rfm> RFMs)
                bit = int(line.split("Received: ")[1].split("(")[0].strip())
                num_rfm = int(line.split("(")[1].split(" ")[0].strip())
                # insert the act to the dataframe
                df_acts = pd.concat([df_acts, pd.DataFrame([{'index': it, 'RFM': num_rfm, 'Bit': bit}])], ignore_index=True)
                it += 1
            except (IndexError, ValueError) as e:
                continue

                
df = df_acts.copy()
# Plot setup
fig, ax = plt.subplots(figsize=(9, 3.6))

# Increase font size
plt.rcParams.update({'font.size': 14})

# Define background colors
for i, value in enumerate(df["Bit"]):
    color = "sandybrown" if value == 0 else "cornflowerblue"
    ax.axvspan(i - 0.5, i + 0.5, color=color, alpha=0.3, label="0" if value == 0 else "1")

# Plot the RFM values
ax.plot(df["index"], df["RFM"], marker="o", color="black", label="RFM")

# Set x-ticks to align with bold lines
xticks_positions = [(i - 1) + 0.5 for i in range(0, len(df) + 1, 8)]
ax.set_xticks(xticks_positions)
ax.set_xticklabels([str(int(pos + 0.5)) for pos in xticks_positions], fontsize=12)

# Labeling
ax.set_xlabel("Transmission Window", fontsize=14)
ax.set_ylabel("Number of RFMs\nMeasured by the Receiver", fontsize=14)

# Add bold vertical lines for clarity
for i in range(0, len(df) + 1, 8):
    ax.axvline((i - 1) + 0.5, color="black", linewidth=1)

# Add letter "I" under the first 8 values
x_pos = (xticks_positions[0] + xticks_positions[1]) / 2  # Center of the first region
ax.text(x_pos, -0.5, "(M)", ha='center', va='top', fontsize=16, color='red')

x_pos = (xticks_positions[1] + xticks_positions[2]) / 2  # Center of the first region
ax.text(x_pos, -0.5, "(I)", ha='center', va='top', fontsize=16, color='red')

x_pos = (xticks_positions[2] + xticks_positions[3]) / 2  # Center of the first region
ax.text(x_pos, -0.5, "(C)", ha='center', va='top', fontsize=16, color='red')

x_pos = (xticks_positions[3] + xticks_positions[4]) / 2  # Center of the first region
ax.text(x_pos, -0.5, "(R)", ha='center', va='top', fontsize=16, color='red')

x_pos = (xticks_positions[4] + xticks_positions[5]) / 2  # Center of the first region
ax.text(x_pos, -0.5, "(O)", ha='center', va='top', fontsize=16, color='red')

ax.axhline(3, color="gray", linewidth=2, linestyle="--")

# Add legend with title inline
handles, labels = ax.get_legend_handles_labels()
by_label = dict(zip(labels, handles))  # Remove duplicates in the legend
by_label.pop("RFM")  # Remove RFM from legend
legend_labels = ["Bit Value"] + list(by_label.keys())
legend_handles = [plt.Line2D([0], [0], color="none")] + list(by_label.values())
ax.legend(
    legend_handles,
    legend_labels,
    loc='upper center',
    fontsize=12,
    ncol=len(legend_labels),
    framealpha=1,
    bbox_to_anchor=(0.5, 1.2),
    edgecolor="black",
    borderpad=0.3,
    fancybox=False,
    labelspacing=0.5,
)
# set xlim
plt.xlim(-1, len(df))


# Show plot
plt.tight_layout()
# save to pdf
plt.savefig(figure_name, bbox_inches='tight', dpi=300)
