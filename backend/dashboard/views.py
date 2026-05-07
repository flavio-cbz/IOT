from datetime import timedelta
from pathlib import Path

from django.conf import settings
from django.http import JsonResponse
from django.shortcuts import render
from django.utils import timezone
from mongoengine.connection import get_db

from iot_station.mongo import connect_mongo

from .models import SensorReading
from .rendering import reading_context


def _latest_reading(station_id):
    return SensorReading.objects(station_id=station_id).order_by("-timestamp").first()


def dashboard(request):
    connect_mongo()
    station_id = request.GET.get("station_id", settings.DEFAULT_STATION_ID)
    latest = _latest_reading(station_id)
    static_dir = Path(settings.BASE_DIR) / "dashboard" / "static" / "dashboard"
    context = {
        "station_id": station_id,
        "latest": latest,
        "inline_css": (static_dir / "css" / "app.css").read_text(encoding="utf-8"),
        "inline_js": (static_dir / "js" / "dashboard.js").read_text(encoding="utf-8"),
    }
    if latest:
        context.update(reading_context(latest))
    return render(request, "dashboard/index.html", context)


def health(_request):
    try:
        connect_mongo()
        db = get_db()
        db.command("ping")
        mongo = "ok"
    except Exception as exc:
        return JsonResponse({"status": "degraded", "mongo": "error", "detail": str(exc)}, status=503)
    return JsonResponse({"status": "ok", "mongo": mongo})


def history(request):
    connect_mongo()
    station_id = request.GET.get("station_id", settings.DEFAULT_STATION_ID)
    window_key = request.GET.get("window", "4h")
    window_map = {
        "10m": timedelta(minutes=10),
        "1h": timedelta(hours=1),
        "4h": timedelta(hours=4),
        "1d": timedelta(days=1),
    }

    period = window_map.get(window_key)
    if period is None:
        # backward compatibility for older clients still sending ?hours=
        try:
            hours = max(1, min(24, int(request.GET.get("hours", "4"))))
        except ValueError:
            hours = 4
        period = timedelta(hours=hours)
        window_key = f"{hours}h"

    since = timezone.now() - period
    readings = (
        SensorReading.objects(station_id=station_id, timestamp__gte=since)
        .order_by("timestamp")
        .only(
            "station_id",
            "timestamp",
            "temperature",
            "pressure",
            "soil_humidity",
            "light_level",
            "happiness",
            "problem_code",
        )
    )
    return JsonResponse(
        {
            "station_id": station_id,
            "window": window_key,
            "window_seconds": int(period.total_seconds()),
            "readings": [reading.to_payload() for reading in readings],
        }
    )
