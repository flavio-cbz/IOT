from django.conf import settings
from mongoengine import connect

_connected = False


def connect_mongo():
    global _connected
    if _connected:
        return
    connect(host=settings.MONGODB_URI, alias=settings.MONGODB_ALIAS)
    _connected = True
