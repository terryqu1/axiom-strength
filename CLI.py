import csv
import os

FILENAME = "2024_training_logs.csv"
HEADERS = ["week_num", "day", "lift", "variation", "weight_lbs", "reps", "sets", "rpe", "tonnage", "e1rm"]

def calculate_e1rm(weight_lbs, reps, rpe):
    """
    Calculates estimated one rep max based on epley formula
    """
    rir = 10 - rpe
    abs_reps = reps + rir
    return weight_lbs * (1 + abs_reps / 30)

def calculate_tonnage(weight_lbs, reps, sets):
    """
    Calculates tonnage
    """
    return weight_lbs * reps * sets


def main():

    with open(FILENAME, mode="a") as file:
        writer = csv.DictWriter(file, fieldnames=HEADERS)

        if not os.path.isfile(FILENAME):
            writer.writeheader()
            print(f"Created new file: {FILENAME}\n")

        print("---2024 Training Data Ingestion CLI---")
        print("Press q any time to save and quit\n")

        while True:

            week = input(f"Enter the week number: ").strip()
            if week.lower() == "q": break

            day = input(f"Enter the training day (1-5): ").strip()
            if day.lower() == "q": break


            lift = input("Lift (e.g., Squat): ").strip()
            if lift.lower() == 'q': break
            
            variation = input("Variation (e.g., Comp, Paused): ").strip()
            if variation.lower() == 'q': break
            
            try:
                weight = float(input("Weight (lbs): "))
                reps = int(input("Reps: "))
                sets = int(input("Sets: "))
                rpe = float(input("RPE: "))
            except ValueError:
                print("Invalid number. Please try entering this set again.\n")
                continue
            
            tonnage = calculate_tonnage(weight, reps, sets)
            e1rm = calculate_e1rm(weight, reps, rpe)
            
            row = {
                "week_num": week,
                "day": day,
                "lift": lift.capitalize(),
                "variation": variation.capitalize(),
                "weight_lbs": weight,
                "reps": reps,
                "sets": sets,
                "rpe": rpe,
                "tonnage": tonnage,
                "e1rm": e1rm
            }
            
            writer.writerow(row)
            file.flush()  # Forces write to disk immediately. Prevents data loss if the script crashes.
            
            print(f"-> Saved: {lift} | {weight}x{reps}x{sets} | e1RM: {e1rm}\n")


if __name__ == "__main__":
    main()