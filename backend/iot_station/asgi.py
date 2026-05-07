import os

os.environ.setdefault("DJANGO_SETTINGS_MODULE", "iot_station.settings")

import django
from channels.routing import ProtocolTypeRouter, URLRouter
from channels.security.websocket import AllowedHostsOriginValidator
from django.conf import settings
from django.core.asgi import get_asgi_application

django.setup()

from iot_station.mongo import connect_mongo
from iot_station.routing import websocket_urlpatterns

connect_mongo()

if settings.MQTT_ENABLED:
    from dashboard.mqtt_bridge import start_mqtt_bridge_once

    start_mqtt_bridge_once()

application = ProtocolTypeRouter(
    {
        "http": get_asgi_application(),
        "websocket": AllowedHostsOriginValidator(URLRouter(websocket_urlpatterns)),
    }
)
