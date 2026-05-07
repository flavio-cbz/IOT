PROBLEM_STATES = {
    0: {
        "label": "Je me sens parfaitement bien",
        "mood": "Parfait",
        "hint": "Ma terre, ma lumière et mon air sont en harmonie.",
        "class": "plant-ok",
        "accent": "#34d399",
    },
    1: {
        "label": "J'ai soif !",
        "mood": "Soif",
        "hint": "Ma terre se craquelle, j'aurais besoin d'un peu d'eau.",
        "class": "plant-thirsty",
        "accent": "#fb923c",
    },
    2: {
        "label": "Je me noie !",
        "mood": "Trop d'eau",
        "hint": "Mes racines baignent dans l'eau, laisse ma terre respirer.",
        "class": "plant-drowned",
        "accent": "#38bdf8",
    },
    3: {
        "label": "Je suis dans le noir...",
        "mood": "Sombre",
        "hint": "Je cherche la lumière, rapproche-moi d'une fenêtre.",
        "class": "plant-dark",
        "accent": "#a78bfa",
    },
    4: {
        "label": "J'ai froid !",
        "mood": "Froid",
        "hint": "Mes feuilles se crispent, l'air est trop frais pour moi.",
        "class": "plant-cold",
        "accent": "#67e8f9",
    },
    5: {
        "label": "J'ai trop chaud !",
        "mood": "Chaud",
        "hint": "Je fatigue sous la chaleur, un coin plus doux m'aiderait.",
        "class": "plant-hot",
        "accent": "#f97316",
    },
}


def problem_state(code):
    return PROBLEM_STATES.get(int(code or 0), PROBLEM_STATES[0])


def clamp(value, minimum, maximum):
    return max(minimum, min(maximum, value))


def vitality_state(happiness):
    score = clamp(float(happiness or 0), 0, 5)
    filled = int(round(score))
    if score >= 4.4:
        label = "Parfait"
        tone = "good"
    elif score >= 3.2:
        label = "Stable"
        tone = "good"
    elif score >= 2.2:
        label = "Fragile"
        tone = "warn"
    elif score >= 1.2:
        label = "En stress"
        tone = "bad"
    else:
        label = "Critique"
        tone = "bad"
    return {
        "label": label,
        "tone": tone,
        "segments": [{"filled": index < filled} for index in range(5)],
    }


def soil_gauge(value):
    value = float(value or 0)
    if value < 30:
        label, tone = "Beaucoup trop sec", "bad"
    elif value < 40:
        label, tone = "Un peu sec", "warn"
    elif value <= 70:
        label, tone = "Parfait", "good"
    elif value <= 85:
        label, tone = "Trop humide", "warn"
    else:
        label, tone = "Je me noie", "bad"
    return {
        "name": "Ma terre",
        "value": round(value),
        "unit": "%",
        "label": label,
        "tone": tone,
        "percent": clamp(value, 0, 100),
        "low_label": "Sec",
        "mid_label": "Parfait",
        "high_label": "Humide",
    }


def light_gauge(value):
    value = float(value or 0)
    if value < 50:
        label, tone = "Trop sombre", "bad"
    elif value < 200:
        label, tone = "Lumiere douce", "warn"
    else:
        label, tone = "Parfait", "good"
    return {
        "name": "Ma lumière",
        "value": round(value),
        "unit": "lux",
        "label": label,
        "tone": tone,
        "percent": clamp((value / 500) * 100, 0, 100),
        "low_label": "Sombre",
        "mid_label": "Doux",
        "high_label": "Clair",
    }


def temperature_gauge(value):
    value = float(value or 0)
    if value < 15:
        label, tone = "Trop froid", "bad"
    elif value < 18:
        label, tone = "Un peu frais", "warn"
    elif value <= 26:
        label, tone = "Parfait", "good"
    elif value <= 30:
        label, tone = "Un peu chaud", "warn"
    else:
        label, tone = "Trop chaud", "bad"
    return {
        "name": "Mon air",
        "value": round(value, 1),
        "unit": "°C",
        "label": label,
        "tone": tone,
        "percent": clamp(((value - 10) / 25) * 100, 0, 100),
        "low_label": "Froid",
        "mid_label": "Parfait",
        "high_label": "Chaud",
    }


def pressure_gauge(value):
    value = float(value or 0)
    if value < 990:
        label, tone = "Air bas", "warn"
    elif value > 1030:
        label, tone = "Air haut", "warn"
    else:
        label, tone = "Stable", "good"
    return {
        "name": "Pression",
        "value": round(value),
        "unit": "hPa",
        "label": label,
        "tone": tone,
        "percent": clamp(((value - 960) / 90) * 100, 0, 100),
        "low_label": "Bas",
        "mid_label": "Stable",
        "high_label": "Haut",
    }


def gauges_for(reading):
    gauges = [
        {"key": "soil", **soil_gauge(reading.soil_humidity)},
        {"key": "light", **light_gauge(reading.light_level)},
        {"key": "temperature", **temperature_gauge(reading.temperature)},
        {"key": "pressure", **pressure_gauge(reading.pressure)},
    ]

    active_by_problem = {
        1: "soil",
        2: "soil",
        3: "light",
        4: "temperature",
        5: "temperature",
    }
    active_key = active_by_problem.get(int(reading.problem_code or 0))

    for gauge in gauges:
        gauge["selected"] = gauge["key"] == active_key

    return gauges
