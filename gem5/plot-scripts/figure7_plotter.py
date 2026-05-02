import seaborn as sns
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import sys

file_name = ""
figure_name = "" 
MSG_BYTES = 1  # Default value, can be overridden by command line argument
label = "RFM"  # Default label for the printout; pass a 4th arg to override

# get file name as argument
if len(sys.argv) > 3:
    file_name = sys.argv[1]
    figure_name = sys.argv[2]
    MSG_BYTES = int(sys.argv[3])
    if len(sys.argv) > 4:
        label = sys.argv[4]
else:
    print("Usage: python figure2_plotter.py <input_file_name> <figure_name> <number_of_bytes> [label]")
    sys.exit(1)

NUM_BITS = MSG_BYTES * 8


data = pd.read_csv(file_name, index_col=False,
                   dtype={'sent': str, 'received': str})
plot_data = data[(data["rate"] >= 0) & (data["errors"] >= 0)].sort_values(by="rate")
average_errors = plot_data.groupby("rate")["errors"].mean().reset_index()
average_time = plot_data.groupby("rate")["time"].mean().reset_index()

df = average_errors.copy()
df = pd.merge(df, average_time, on="rate", how="inner")
df["rate"] = df["rate"].astype(int)
df = df[df["rate"] > 175]
# RFM 10x is at rate 325 because of the high number of RFMs in benign workloads
df = df[df["rate"] < 350]

df["rate"] = df["rate"] - 175

#make errorprob values that are 0 to 0.0001
df.loc[df["errors"] == 0, "errors"] = 0.0001

df["intensity"] = 100 - (df["rate"].astype(float)- df['rate'].min()) /  (df["rate"].max()-df['rate'].min())  * 100
df["errorprob"] = df["errors"] /  NUM_BITS

# time column is in nanoseconds, so divide by 1e9 to get seconds
df["time_s"] =  df["time"] / 1e9

df['rawbitrate'] = NUM_BITS / df["time_s"] / 1024

df['entropy'] = (-1 * df['errorprob']*np.log2(df['errorprob'])) - ((1-df['errorprob'])*np.log2(1-df['errorprob']))
df['capacity'] = (1 - df['entropy']) * df['rawbitrate']


width = 7
height = 2.25
fig, ax1 = plt.subplots(figsize=(width, height))

# Plot Error Probability on the primary y-axis
line1 = sns.lineplot(data=df, x="intensity", y="errorprob", ax=ax1, color="blue", label="Error Probability", linewidth=2)
ax1.set_xlabel("Noise Intensity (%)", fontsize=14)
ax1.set_ylabel("Error Probability", fontsize=14)
ax1.tick_params(axis='y')
# limit y axis to 0 to 0.5
ax1.set_ylim(-0.01, 0.51)


# Add custom y-ticks for the primary axis
ax1.set_yticks(np.arange(0, 0.51, 0.1))  # Tick range from 0 to 1.0 with 0.2 step

# Plot Channel Capacity on the secondary y-axis
ax2 = ax1.twinx()
line2 = sns.lineplot(data=df, x="intensity", y="capacity", ax=ax2, color="red", label="Channel Capacity", linewidth=2)
ax2.set_ylabel("Channel Capacity\n(Kbps)" , fontsize=14)
ax2.tick_params(axis='y')

ax2.set_ylim(0, 50)

# set y ticks
ax2.set_yticks(np.arange(0, 51, 10))  # Tick range from 0 to 50 with 10 step

# Align the secondary y-axis ticks with the primary y-axis ticks
capacity_max = df["capacity"].max()  # Maximum value for the capacity

# set y axis labels' font size 12
ax1.tick_params(axis='y', labelsize=12)
ax2.tick_params(axis='y', labelsize=12)

# set x axis labels' font size 12
ax1.tick_params(axis='x', labelsize=12)


# Add a single legend for both lines
lines = ax1.get_lines() + ax2.get_lines()  # Combine both line handles
labels = [line.get_label() for line in lines]  # Get labels from the lines
fig.legend(
    lines,
    labels,
    loc="upper center",
    bbox_to_anchor=(0.5, 1.05),
    ncol=2,  # Number of columns in the legend
    frameon=False,  # Remove legend border
    fontsize=12,
)

# add line on x = 1
plt.axvline(x=1, color='darkorange', linestyle='--', linewidth=2)
# add text on orange line that says 10x rotated 90
txt_height = 35
plt.text(2, txt_height, r"10$\times$", color='darkorange', fontsize=12, rotation=90)
# Purple "channel-closed" reference line is meaningful for RFM (where the
# channel collapses around 50% intensity) but not for DREAM, whose channel
# is closed at all intensities. Skip it for the DREAM plot to avoid clutter.
if label.upper() != "DREAM":
    plt.axvline(x=50, color='purple', linestyle='--', linewidth=2)

# set x axis limits
ax1.set_xlim(0, 100)

# remove legend
leg1 = ax1.get_legend()
if leg1: leg1.remove()
leg2 = ax2.get_legend()
if leg2: leg2.remove()

# Tight layout for better spacing
plt.tight_layout()

plt.savefig(figure_name, dpi=300, bbox_inches='tight')


print(f"{label} Noise Results:")
interest_pt = 50
err_entry = df[(df['intensity'] >= interest_pt -1) & (df['intensity'] <= interest_pt +1)]["errorprob"]
#print("Error Probability: " + str(err_entry.values[0]))

channel_cap_entry = df[(df['intensity'] >= interest_pt -1) & (df['intensity'] <= interest_pt +1)]["capacity"]
print("50%-Intensity Channel Capacity: " + str(channel_cap_entry.values[0]))


interest_pt = 1
err_entry = df[(df['intensity'] >= interest_pt -1) & (df['intensity'] <= interest_pt +1)]["errorprob"]
#print("Error Probability: " + str(err_entry.values[0]))

channel_cap_entry = df[(df['intensity'] >= interest_pt -1) & (df['intensity'] <= interest_pt +1)]["capacity"]
print("Lowest-Intensity Channel Capacity: " + str(channel_cap_entry.values[0]))