from django.urls import path

from dashboard.consumers import StationConsumer

websocket_urlpatterns = [
    path("ws/station/", StationConsumer.as_asgi()),
]
