import pandas as pd

df = pd.read_csv("training_log.csv").iloc[::-1]

df.to_csv("reversed_training_log.csv", index=False)