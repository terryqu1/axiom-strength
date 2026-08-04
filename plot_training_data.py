import matplotlib.pyplot as plt
import pandas as pd

df = pd.read_csv("2024_training_logs.csv", header=None, names=['Week','Day','Lift','Variation','Weight','Reps','Sets','RPE','Volume','e1rm'])

bench_df = df[df['Lift'].str.contains('Bench', case=False, na=False)]
squat_df = df[df['Lift'].str.contains('Squat', case=False, na=False)]
deadlift_df = df[df['Lift'].str.contains('Deadlift', case=False, na=False)]

print(bench_df.head())
print(squat_df.head())
print(deadlift_df.head())

# plt.title("Training Data")
# plt.xlabel('Weeks')
# plt.ylabel('e1rm')
# plt.grid(True)

# plt.savefig("training_data.png")
# plt.show()