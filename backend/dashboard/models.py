from mongoengine import DateTimeField, Document, FloatField, IntField, StringField

from django.conf import settings


class SensorReading(Document):
    station_id = StringField(required=True, max_length=80)
    timestamp = DateTimeField(required=True)
    temperature = FloatField(required=True)
    pressure = FloatField(required=True)
    soil_humidity = FloatField(required=True)
    light_level = FloatField(required=True)
    happiness = FloatField(required=True, min_value=0.0, max_value=5.0)
    problem_code = IntField(required=True, min_value=0, max_value=5)

    meta = {
        "collection": "sensor_reading",
        "indexes": [
            "station_id",
            "-timestamp",
            {
                "fields": ["timestamp"],
                "expireAfterSeconds": settings.SENSOR_TTL_SECONDS,
            },
            {
                "fields": ["station_id", "-timestamp"],
            },
        ],
        "ordering": ["-timestamp"],
    }

    def to_payload(self):
        ts_str = self.timestamp.isoformat()
        if "+" not in ts_str and not ts_str.endswith("Z"):
            ts_str += "Z"
        return {
            "station_id": self.station_id,
            "timestamp": ts_str,
            "temperature": self.temperature,
            "pressure": self.pressure,
            "soil_humidity": self.soil_humidity,
            "light_level": self.light_level,
            "happiness": self.happiness,
            "problem_code": self.problem_code,
        }
