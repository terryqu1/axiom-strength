import pandas as pd
import numpy as np

def calculate_daily_impulse(input_csv, output_csv):
    df = pd.read_csv(input_csv)
    
    search_terms = ['Bench', 'Spoto', 'Squat', 'Deadlift']
    pattern = "|".join(search_terms)
    df = df[df['exercise type'].str.contains(pattern, case=False, na=False)]

    is_bench = df['exercise type'].str.contains("Bench|Spoto", case=False, na=False)
    is_squat = df['exercise type'].str.contains("Squat", case=False, na=False)
    is_deadlift = df['exercise type'].str.contains("Deadlift", case=False, na=False)

    df['date'] = pd.to_datetime(df['date'])
    df['fit_bench'] = (df['weight'] * df['reps']).where(is_bench, 0)
    df['fit_squat'] = (df['weight'] * df['reps']).where(is_squat, 0)
    df['fit_deadlift'] = (df['weight'] * df['reps']).where(is_deadlift, 0)
    df['fatigue_impulse'] = df['weight'] * df['reps'] * np.exp(df['RPE']/10)   

    daily_df = df.groupby('date')[['fit_bench', 'fit_squat', 'fit_deadlift', 'fatigue_impulse']].sum().reset_index()
    
    full_range = pd.date_range(start=daily_df['date'].min(), end=daily_df['date'].max())
    daily_df = daily_df.set_index('date').reindex(full_range, fill_value=0.0).reset_index()
    daily_df.columns = ['date', 'fit_bench', 'fit_squat', 'fit_deadlift', 'fatigue_impulse']
    
    daily_df.to_csv(output_csv, index=False)
    print(f"Impulse dataset saved to {output_csv}")

if __name__ == "__main__":
    print("Running script...")
    calculate_daily_impulse('training_log.csv', 'impulse.csv')
