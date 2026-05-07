import json
from datetime import timezone as datetime_timezone

from django.utils import timezone
from django.template.loader import render_to_string

from .state import gauges_for, problem_state, vitality_state


def _aware_timestamp(timestamp):
    if timestamp is None:
        return None
    if timestamp.tzinfo is None:
        return timestamp.replace(tzinfo=datetime_timezone.utc)
    return timestamp


def freshness_state(reading):
    if not reading:
        return {
            "status": "offline",
            "label": "Station inactive",
            "detail": "Aucune lecture reçue",
            "active": False,
            "age_seconds": None,
        }
    timestamp = _aware_timestamp(reading.timestamp)
    age_seconds = max(0, int((timezone.now() - timestamp).total_seconds()))
    if age_seconds <= 30:
        status = "active"
        label = "Station active"
        detail = "Lecture reçue à l'instant"
    elif age_seconds <= 120:
        status = "stale"
        label = "Station en retard"
        detail = "Dernière lecture il y a moins de 2 min"
    else:
        status = "offline"
        label = "Station inactive"
        minutes = max(2, age_seconds // 60)
        detail = f"Dernière lecture il y a {minutes} min"
    return {
        "status": status,
        "label": label,
        "detail": detail,
        "active": status == "active",
        "age_seconds": age_seconds,
    }


def reading_context(reading):
    payload = reading.to_payload()
    freshness = freshness_state(reading)
    payload["station_status"] = freshness["status"]
    payload["station_active"] = freshness["active"]
    return {
        "reading": reading,
        "payload": payload,
        "payload_json": json.dumps(payload),
        "state": problem_state(reading.problem_code),
        "freshness": freshness,
        "gauges": gauges_for(reading),
        "vitality": vitality_state(reading.happiness),
    }


def render_live_fragment(reading):
    return render_to_string("dashboard/partials/live_update.html", reading_context(reading))
