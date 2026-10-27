from typing import Dict, Tuple

import folium
import numpy as np
import pandas as pd
import plotly.express as px
import streamlit as st
from folium.plugins import HeatMap
from streamlit_folium import st_folium

st.set_page_config(page_title="AI Soil Analysis", page_icon="🌱", layout="wide")

# =====================================================
# Styling
# =====================================================
st.markdown(
    """
    <style>
    .block-container {padding-top: 1.4rem; padding-bottom: 2rem;}
    .title-box {
        background: linear-gradient(135deg, #0f172a, #14532d);
        padding: 26px;
        border-radius: 22px;
        color: white;
        margin-bottom: 18px;
        box-shadow: 0 10px 28px rgba(15, 23, 42, .20);
    }
    .title-box h1 {font-size: 32px; margin-bottom: 6px;}
    .title-box p {font-size: 16px; color: #dcfce7; line-height: 1.65;}
    .info-box {
        background: #eff6ff;
        border: 1px solid #bfdbfe;
        border-left: 6px solid #2563eb;
        color: #1e3a8a;
        padding: 14px 18px;
        border-radius: 12px;
        font-size: 16px;
        font-weight: 600;
        margin: 12px 0 18px 0;
        line-height: 1.75;
    }
    .caption-box {
        background:#f8fafc;
        border:1px solid #e2e8f0;
        border-radius:14px;
        padding:14px 16px;
        color:#334155;
        line-height:1.7;
    }
    .good {color: #15803d; font-weight: 800;}
    .moderate {color: #ca8a04; font-weight: 800;}
    .poor {color: #dc2626; font-weight: 800;}
    </style>
    """,
    unsafe_allow_html=True,
)

st.markdown(
    """
    <div class="title-box">
        <h1>🌱 AI Soil Analysis & Fertilizer Recommendation</h1>
        <p>
        Upload the CSV file collected by the Intelligent Agricultural Robot.
        The module will analyze soil condition, calculate fertility scores,
        generate recommendations, and draw a geographic sampling map.
        </p>
    </div>
    """,
    unsafe_allow_html=True,
)

# =====================================================
# Helpers
# =====================================================
COLUMN_ALIASES: Dict[str, str] = {
    "sampleid": "SampleID",
    "sample_id": "SampleID",
    "id": "SampleID",
    "time": "Time",
    "latitude": "Latitude",
    "lat": "Latitude",
    "longitude": "Longitude",
    "lon": "Longitude",
    "lng": "Longitude",
    "n_mgkg": "N_mgkg",
    "n": "N_mgkg",
    "nitrogen": "N_mgkg",
    "p_mgkg": "P_mgkg",
    "p": "P_mgkg",
    "phosphorus": "P_mgkg",
    "k_mgkg": "K_mgkg",
    "k": "K_mgkg",
    "potassium": "K_mgkg",
    "ph": "pH",
    "moisture_percent": "Moisture_percent",
    "moisture": "Moisture_percent",
    "temperature_c": "Temperature_C",
    "temp": "Temperature_C",
    "temperature": "Temperature_C",
    "ec_uscm": "EC_uscm",
    "ec": "EC_uscm",
    "npk_status": "NPK_Status",
    "status": "NPK_Status",
}

REQUIRED_NUMERIC = [
    "N_mgkg",
    "P_mgkg",
    "K_mgkg",
    "pH",
    "Moisture_percent",
    "Temperature_C",
    "EC_uscm",
]


def normalize_columns(df: pd.DataFrame) -> pd.DataFrame:
    new_cols = {}
    for col in df.columns:
        key = str(col).strip().lower().replace(" ", "_").replace("-", "_")
        new_cols[col] = COLUMN_ALIASES.get(key, col)
    return df.rename(columns=new_cols)


def coerce_numeric(df: pd.DataFrame) -> pd.DataFrame:
    for col in ["Latitude", "Longitude"] + REQUIRED_NUMERIC:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")
    return df


def classify_value(value: float, low: float, high: float, label: str) -> Tuple[str, str]:
    if pd.isna(value):
        return "Unknown", f"{label}: missing value."
    if value < low:
        return "Low", f"{label} is low."
    if value > high:
        return "High", f"{label} is high."
    return "Normal", f"{label} is within the acceptable demo range."


def analyze_row(row: pd.Series, crop_type: str = "General crop") -> pd.Series:
    notes = []
    recommendations = []
    score = 100

    # Initial demo thresholds. These can be adjusted by crop type and local soil standards.
    n_status, n_note = classify_value(row.get("N_mgkg"), 40, 120, "Nitrogen")
    p_status, p_note = classify_value(row.get("P_mgkg"), 30, 80, "Phosphorus")
    k_status, k_note = classify_value(row.get("K_mgkg"), 40, 120, "Potassium")

    notes.extend([n_note, p_note, k_note])

    if n_status == "Low":
        score -= 18
        recommendations.append("Add nitrogen-based fertilizer or organic compost according to crop requirements.")
    elif n_status == "High":
        score -= 8
        recommendations.append("Avoid excessive nitrogen fertilization in this zone.")

    if p_status == "Low":
        score -= 14
        recommendations.append("Add phosphorus fertilizer, especially before root development stages.")
    elif p_status == "High":
        score -= 6
        recommendations.append("Avoid adding phosphorus until a confirmatory soil test is performed.")

    if k_status == "Low":
        score -= 14
        recommendations.append("Add potassium fertilizer to improve plant resistance and fruit quality.")
    elif k_status == "High":
        score -= 6
        recommendations.append("Avoid unnecessary potassium fertilization in this zone.")

    ph = row.get("pH")
    if pd.isna(ph):
        score -= 8
        notes.append("pH value is missing.")
    elif ph < 5.5:
        score -= 15
        notes.append("The soil is acidic.")
        recommendations.append("Consider lime treatment after confirmation by a soil specialist.")
    elif ph > 7.5:
        score -= 15
        notes.append("The soil is alkaline.")
        recommendations.append("Use amendments suitable for alkaline soil and select fertilizers carefully.")
    else:
        notes.append("pH is suitable for many crops.")

    moisture = row.get("Moisture_percent")
    if pd.isna(moisture):
        score -= 6
        notes.append("Moisture value is missing.")
    elif moisture < 20:
        score -= 12
        notes.append("Soil moisture is low.")
        recommendations.append("Increase irrigation or check soil water retention.")
    elif moisture > 70:
        score -= 10
        notes.append("Soil moisture is high.")
        recommendations.append("Reduce irrigation and check drainage.")
    else:
        notes.append("Moisture is acceptable.")

    ec = row.get("EC_uscm")
    if pd.isna(ec):
        score -= 6
        notes.append("EC value is missing.")
    elif ec > 2000:
        score -= 18
        notes.append("High EC indicates possible salinity risk.")
        recommendations.append("Check salinity, improve drainage, and avoid over-fertilization.")
    else:
        notes.append("EC is within the initial demo range.")

    score = max(0, min(100, int(round(score))))

    if score >= 75:
        status = "Good"
        color = "green"
    elif score >= 50:
        status = "Moderate"
        color = "orange"
    else:
        status = "Poor"
        color = "red"

    if not recommendations:
        recommendations.append("No urgent fertilizer action is required based on the current demo thresholds.")

    if crop_type != "General crop":
        recommendations.append(f"Selected crop: {crop_type}. Future versions can use crop-specific thresholds.")

    return pd.Series(
        {
            "Nitrogen_Status": n_status,
            "Phosphorus_Status": p_status,
            "Potassium_Status": k_status,
            "Fertility_Score": score,
            "Soil_Status": status,
            "Map_Color": color,
            "Main_Notes": " ".join(notes),
            "Recommendation": " ".join(recommendations),
        }
    )


def create_map(df: pd.DataFrame, value_layer: str = "Fertility_Score", show_heatmap: bool = False):
    valid_geo = df.dropna(subset=["Latitude", "Longitude"]).copy()
    if valid_geo.empty:
        return None

    center = [valid_geo["Latitude"].mean(), valid_geo["Longitude"].mean()]
    fmap = folium.Map(location=center, zoom_start=18, control_scale=True, tiles="OpenStreetMap")

    for _, row in valid_geo.iterrows():
        color = row.get("Map_Color", "blue")
        popup = folium.Popup(
            f"""
            <b>Sample:</b> {row.get('SampleID', '')}<br>
            <b>Time:</b> {row.get('Time', '')}<br>
            <b>N:</b> {row.get('N_mgkg', '')} mg/kg<br>
            <b>P:</b> {row.get('P_mgkg', '')} mg/kg<br>
            <b>K:</b> {row.get('K_mgkg', '')} mg/kg<br>
            <b>pH:</b> {row.get('pH', '')}<br>
            <b>Moisture:</b> {row.get('Moisture_percent', '')}%<br>
            <b>EC:</b> {row.get('EC_uscm', '')} us/cm<br>
            <b>Score:</b> {row.get('Fertility_Score', '')}/100<br>
            <b>Status:</b> {row.get('Soil_Status', '')}<br>
            <b>Recommendation:</b> {row.get('Recommendation', '')}
            """,
            max_width=440,
        )
        folium.CircleMarker(
            location=[row["Latitude"], row["Longitude"]],
            radius=9,
            color=color,
            fill=True,
            fill_color=color,
            fill_opacity=0.75,
            popup=popup,
            tooltip=f"Sample {row.get('SampleID', '')} | {row.get('Soil_Status', '')}",
        ).add_to(fmap)

    if show_heatmap and value_layer in valid_geo.columns:
        heat_df = valid_geo.dropna(subset=[value_layer])
        if not heat_df.empty:
            vals = heat_df[value_layer].astype(float)
            if vals.max() != vals.min():
                intensities = (vals - vals.min()) / (vals.max() - vals.min())
            else:
                intensities = vals * 0 + 0.5
            heat_data = [
                [lat, lon, float(intensity)]
                for lat, lon, intensity in zip(heat_df["Latitude"], heat_df["Longitude"], intensities)
            ]
            HeatMap(heat_data, radius=20, blur=18, min_opacity=0.35).add_to(fmap)

    return fmap


def build_report_text(df: pd.DataFrame) -> str:
    if df.empty:
        return "No data available."

    lines = []
    lines.append("AgriSoil AI - Soil Analysis Report")
    lines.append("Project: Intelligent Agricultural Robot for Soil Analysis and Fertilizer Recommendation Using AI")
    lines.append("")
    lines.append(f"Number of samples: {len(df)}")
    lines.append(f"Average fertility score: {df['Fertility_Score'].mean():.1f}/100")
    lines.append(f"Average N: {df['N_mgkg'].mean():.1f} mg/kg")
    lines.append(f"Average P: {df['P_mgkg'].mean():.1f} mg/kg")
    lines.append(f"Average K: {df['K_mgkg'].mean():.1f} mg/kg")
    lines.append(f"Average pH: {df['pH'].mean():.2f}")
    lines.append(f"Average moisture: {df['Moisture_percent'].mean():.1f}%")
    lines.append(f"Average EC: {df['EC_uscm'].mean():.1f} us/cm")
    lines.append("")
    lines.append("Recommendations by sample:")
    for _, row in df.iterrows():
        lines.append(
            f"- Sample {row.get('SampleID', '')}: "
            f"{row.get('Soil_Status', '')} | "
            f"Score {row.get('Fertility_Score', '')}/100 | "
            f"{row.get('Recommendation', '')}"
        )
    lines.append("")
    lines.append("Disclaimer: These recommendations are preliminary decision-support outputs and should be verified by agricultural specialists before real fertilizer application.")
    return "\n".join(lines)


# =====================================================
# Sidebar
# =====================================================
st.sidebar.header("Settings")
crop_type = st.sidebar.selectbox(
    "Crop type",
    ["General crop", "Wheat", "Tomato", "Potato", "Corn", "Cucumber", "Pepper"],
)
map_layer = st.sidebar.selectbox(
    "Map heat layer",
    ["Fertility_Score", "N_mgkg", "P_mgkg", "K_mgkg", "pH", "Moisture_percent", "EC_uscm"],
)
show_heatmap = st.sidebar.checkbox("Show heatmap layer", value=False)

st.sidebar.markdown("---")
st.sidebar.caption(
    "The current version uses a rule-based expert system. It can later be upgraded "
    "to a machine learning model after collecting a larger labeled dataset."
)

# =====================================================
# Upload
# =====================================================
uploaded_file = st.file_uploader("Upload CSV exported from the robot dashboard", type=["csv"])

if uploaded_file is None:
    st.markdown(
        """
        <div class="info-box">
            Upload the <b>CSV</b> file exported from the robot dashboard to start analysis.
        </div>
        """,
        unsafe_allow_html=True,
    )
    st.stop()

try:
    df = pd.read_csv(uploaded_file)
except Exception as e:
    st.error(f"Could not read CSV file: {e}")
    st.stop()

if df.empty:
    st.error("The uploaded CSV file is empty.")
    st.stop()

df = normalize_columns(df)
df = coerce_numeric(df)

missing_core = [c for c in REQUIRED_NUMERIC if c not in df.columns]
if missing_core:
    st.error("Missing required columns: " + ", ".join(missing_core))
    st.write("Detected columns:", list(df.columns))
    st.stop()

if "SampleID" not in df.columns:
    df["SampleID"] = np.arange(1, len(df) + 1)
if "Time" not in df.columns:
    df["Time"] = ""
if "Latitude" not in df.columns:
    df["Latitude"] = np.nan
if "Longitude" not in df.columns:
    df["Longitude"] = np.nan
if "NPK_Status" not in df.columns:
    df["NPK_Status"] = "Unknown"

analysis_df = df.apply(lambda row: analyze_row(row, crop_type), axis=1)
result = pd.concat([df, analysis_df], axis=1)

# =====================================================
# Summary
# =====================================================
st.subheader("Analysis Summary")

col1, col2, col3, col4 = st.columns(4)
col1.metric("Samples", len(result))
col2.metric("Average Score", f"{result['Fertility_Score'].mean():.1f}/100")
col3.metric("Average pH", f"{result['pH'].mean():.2f}")
col4.metric("Average EC", f"{result['EC_uscm'].mean():.0f} us/cm")

status_counts = result["Soil_Status"].value_counts().to_dict()
st.write(
    f"Good: **{status_counts.get('Good', 0)}** | "
    f"Moderate: **{status_counts.get('Moderate', 0)}** | "
    f"Poor: **{status_counts.get('Poor', 0)}**"
)

st.markdown(
    """
    <div class="caption-box">
        <b>Interpretation:</b> The fertility score is a preliminary decision-support value from 0 to 100.
        Green zones are generally good, orange zones need monitoring or moderate correction,
        and red zones require attention based on the detected soil issues.
    </div>
    """,
    unsafe_allow_html=True,
)

# =====================================================
# Table
# =====================================================
st.subheader("Recommendation Table")
show_cols = [
    "SampleID", "Time", "Latitude", "Longitude", "N_mgkg", "P_mgkg", "K_mgkg",
    "pH", "Moisture_percent", "Temperature_C", "EC_uscm", "Fertility_Score",
    "Soil_Status", "Recommendation"
]
st.dataframe(result[show_cols], use_container_width=True)

# =====================================================
# Map
# =====================================================
st.subheader("Geographic Soil Map")
valid_points = result.dropna(subset=["Latitude", "Longitude"])
if valid_points.empty:
    st.warning("No valid GPS coordinates were found. The map cannot be generated.")
else:
    fmap = create_map(result, value_layer=map_layer, show_heatmap=show_heatmap)
    st_folium(fmap, width=None, height=540)

# =====================================================
# Charts
# =====================================================
st.subheader("Charts")
chart_col1, chart_col2 = st.columns(2)
with chart_col1:
    fig_npk = px.bar(
        result,
        x="SampleID",
        y=["N_mgkg", "P_mgkg", "K_mgkg"],
        barmode="group",
        title="NPK Values by Sample",
    )
    st.plotly_chart(fig_npk, use_container_width=True)

with chart_col2:
    fig_score = px.line(
        result,
        x="SampleID",
        y="Fertility_Score",
        markers=True,
        title="Fertility Score by Sample",
    )
    st.plotly_chart(fig_score, use_container_width=True)

chart_col3, chart_col4 = st.columns(2)
with chart_col3:
    fig_ph = px.line(result, x="SampleID", y="pH", markers=True, title="pH by Sample")
    st.plotly_chart(fig_ph, use_container_width=True)

with chart_col4:
    fig_ec = px.line(result, x="SampleID", y="EC_uscm", markers=True, title="EC by Sample")
    st.plotly_chart(fig_ec, use_container_width=True)

# =====================================================
# Downloads
# =====================================================
st.subheader("Export Results")
processed_csv = result.to_csv(index=False).encode("utf-8-sig")
st.download_button(
    "Download Processed CSV",
    processed_csv,
    file_name="soil_ai_analysis_results.csv",
    mime="text/csv",
)

report_text = build_report_text(result)
st.download_button(
    "Download Text Report",
    report_text.encode("utf-8-sig"),
    file_name="soil_ai_report.txt",
    mime="text/plain",
)

st.caption(
    "Important: Recommendations are preliminary decision-support outputs and should be verified by agricultural specialists before real fertilizer application."
)
