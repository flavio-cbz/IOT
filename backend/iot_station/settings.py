import os
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent

SECRET_KEY = os.getenv("DJANGO_SECRET_KEY", "dev-station-plante-change-me")
DEBUG = os.getenv("DJANGO_DEBUG", "0") == "1"
ALLOWED_HOSTS = [host.strip() for host in os.getenv("DJANGO_ALLOWED_HOSTS", "localhost,127.0.0.1,iot.youcloud.ovh,192.168.1.96").split(",")]
CSRF_TRUSTED_ORIGINS = [origin.strip() for origin in os.getenv("DJANGO_CSRF_TRUSTED_ORIGINS", "https://iot.youcloud.ovh").split(",")]

INSTALLED_APPS = [
    "daphne",
    "django.contrib.staticfiles",
    "channels",
    "dashboard",
]

MIDDLEWARE = [
    "django.middleware.security.SecurityMiddleware",
    "whitenoise.middleware.WhiteNoiseMiddleware",
    "django.middleware.common.CommonMiddleware",
    "django.middleware.csrf.CsrfViewMiddleware",
    "django.middleware.clickjacking.XFrameOptionsMiddleware",
]

ROOT_URLCONF = "iot_station.urls"
ASGI_APPLICATION = "iot_station.asgi.application"

TEMPLATES = [
    {
        "BACKEND": "django.template.backends.django.DjangoTemplates",
        "DIRS": [],
        "APP_DIRS": True,
        "OPTIONS": {
            "context_processors": [
                "django.template.context_processors.request",
            ],
        },
    }
]

DATABASES = {
    "default": {
        "ENGINE": "django.db.backends.dummy",
    }
}

CHANNEL_LAYERS = {
    "default": {
        "BACKEND": "channels.layers.InMemoryChannelLayer",
    }
}

STATIC_URL = "/assets/"
STATIC_ROOT = BASE_DIR / "staticfiles"
STATICFILES_STORAGE = "whitenoise.storage.CompressedManifestStaticFilesStorage"

DEFAULT_AUTO_FIELD = "django.db.models.BigAutoField"
USE_TZ = True
TIME_ZONE = "Europe/Paris"

MONGODB_URI = os.getenv("MONGODB_URI", "mongodb://mongodb:27017/station_iot")
MONGODB_ALIAS = "default"
SENSOR_TTL_SECONDS = int(os.getenv("SENSOR_TTL_SECONDS", "86400"))
DEFAULT_STATION_ID = os.getenv("DEFAULT_STATION_ID", "adi_4")

MQTT_ENABLED = os.getenv("ENABLE_MQTT_BRIDGE", "0") == "1"
MQTT_HOST = os.getenv("MQTT_HOST", "mqtt-broker")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_TOPIC = os.getenv("MQTT_TOPIC", "station/plantes/data")
MQTT_CLIENT_ID = os.getenv("MQTT_CLIENT_ID", "station-iot-django-bridge")
