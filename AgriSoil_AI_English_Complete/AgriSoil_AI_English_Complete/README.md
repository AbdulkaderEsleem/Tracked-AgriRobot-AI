# AgriSoil AI — Complete English Version

A local Streamlit decision-support module for the graduation project:

**Intelligent Agricultural Robot for Soil Analysis and Fertilizer Recommendation Using AI**

## Main functions

- Imports the CSV file exported from the ESP32 robot dashboard.
- Reads sample ID, time, GPS coordinates, N, P, K, pH, moisture, temperature, and EC.
- Applies a rule-based expert system to each sample.
- Calculates a preliminary fertility score.
- Produces soil and fertilizer recommendations.
- Displays an interactive map and optional heatmap.
- Generates NPK, pH, EC, and fertility charts.
- Exports a processed CSV file and a text report.

## Recommended Windows setup

1. Extract the ZIP file to a simple path, for example:

   `C:\AgriSoil_AI_English`

2. Do not run the project from inside the ZIP file.
3. Double-click **install_and_run.bat** the first time.
4. Wait until the packages are installed and the browser opens.
5. In later runs, double-click **run_app.bat**.

The local address is usually:

`http://localhost:8501`

## Manual setup

Open CMD inside the project folder and run:

```bat
py -m venv .venv
.venv\Scripts\activate.bat
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
python -m streamlit run app.py
```

## CSV test file

Use `sample_soil_samples.csv` to test the interface before importing the real robot data.

## Scientific note

The current module is a **rule-based expert system**. The thresholds are preliminary demonstration values and should be calibrated according to crop type, soil type, local agronomic standards, laboratory results, and specialist recommendations before real fertilizer application.
