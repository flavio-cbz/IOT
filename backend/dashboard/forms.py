from datetime import datetime, timezone

from .models import SensorReading

REQUIRED_FLOAT_FIELDS = (
    "temperature",
    "pressure",
    "soil_humidity",
    "light_level",
    "happiness",
)


def _as_float(payload, key):
    value = payload.get(key)
    if value is None:
        raise ValueError(f"Missing field: {key}")
    try:
        return float(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"Invalid float field: {key}") from exc


def _as_problem_code(payload):
    value = payload.get("problem_code")
    if value is None:
        raise ValueError("Missing field: problem_code")
    try:
        code = int(value)
    except (TypeError, ValueError) as exc:
        raise ValueError("Invalid problem_code") from exc
    if code < 0 or code > 5:
        raise ValueError("problem_code must be between 0 and 5")
    return code


def reading_from_payload(payload):
    station_id = str(payload.get("station_id") or "").strip()
    if not station_id:
        raise ValueError("Missing field: station_id")

    values = {field: _as_float(payload, field) for field in REQUIRED_FLOAT_FIELDS}
    return SensorReading(
        station_id=station_id,
        timestamp=datetime.now(timezone.utc),
        problem_code=_as_problem_code(payload),
        **values,
    )
