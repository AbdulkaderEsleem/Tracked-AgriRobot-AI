import streamlit as st

st.set_page_config(
    page_title="AgriSoil AI",
    page_icon="🌱",
    layout="wide",
)

st.markdown(
    """
    <style>
    .main {background-color: #f8fafc;}
    .block-container {padding-top: 2rem; padding-bottom: 2rem;}
    .hero {
        background: linear-gradient(135deg, #0f172a, #14532d);
        padding: 34px;
        border-radius: 24px;
        color: white;
        margin-bottom: 22px;
        box-shadow: 0 12px 32px rgba(15, 23, 42, .20);
    }
    .hero h1 {font-size: 36px; margin-bottom: 8px;}
    .hero p {font-size: 17px; color: #d1fae5; max-width: 980px; line-height: 1.65;}
    .card {
        background: white;
        border: 1px solid #e2e8f0;
        border-radius: 20px;
        padding: 22px;
        box-shadow: 0 8px 22px rgba(15, 23, 42, .07);
        min-height: 170px;
    }
    .step-title {font-size: 20px; font-weight: 800; color:#14532d; margin-bottom: 8px;}
    .note-box {
        background:#ecfdf5;
        border:1px solid #bbf7d0;
        border-left:6px solid #16a34a;
        padding:16px 18px;
        border-radius:14px;
        color:#064e3b;
        line-height:1.65;
        font-size:16px;
        margin-top:18px;
    }
    </style>
    """,
    unsafe_allow_html=True,
)

st.markdown(
    """
    <div class="hero">
        <h1>🌱 AgriSoil AI Module</h1>
        <p>
        A local AI decision-support dashboard for the project:
        <b>Intelligent Agricultural Robot for Soil Analysis and Fertilizer Recommendation Using AI</b>.
        The robot collects soil samples and exports them as CSV, then this module analyzes the data,
        generates fertilizer recommendations, and visualizes the sampling points on a geographic map.
        </p>
    </div>
    """,
    unsafe_allow_html=True,
)

col1, col2, col3 = st.columns(3)

with col1:
    st.markdown(
        """
        <div class="card">
            <div class="step-title">1. Robot Data Collection</div>
            <p>The ESP32 robot records GPS coordinates, time, NPK values, pH, moisture, temperature, and EC for each soil sample.</p>
        </div>
        """,
        unsafe_allow_html=True,
    )

with col2:
    st.markdown(
        """
        <div class="card">
            <div class="step-title">2. CSV Upload</div>
            <p>Download the CSV file from the robot dashboard and upload it to the AI Soil Analysis page.</p>
        </div>
        """,
        unsafe_allow_html=True,
    )

with col3:
    st.markdown(
        """
        <div class="card">
            <div class="step-title">3. AI Recommendation</div>
            <p>The module analyzes soil status, calculates a fertility score, creates recommendations, charts, and a geographic map.</p>
        </div>
        """,
        unsafe_allow_html=True,
    )

st.markdown(
    """
    <div class="note-box">
        Open <b>AI Soil Analysis</b> from the sidebar, then upload the CSV exported from the robot dashboard.
    </div>
    """,
    unsafe_allow_html=True,
)
