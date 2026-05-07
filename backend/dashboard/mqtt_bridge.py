import json
import logging
import threading
from time import sleep

import paho.mqtt.client as mqtt
from asgiref.sync import async_to_sync
from channels.layers import get_channel_layer
from django.conf import settings

from iot_station.mongo import connect_mongo

from .forms import reading_from_payload
from .rendering import render_live_fragment

logger = logging.getLogger(__name__)
_bridge_lock = threading.Lock()
_bridge_started = False


def _publish_to_websocket(reading):
    channel_layer = get_channel_layer()
    if channel_layer is None:
        logger.warning("No channel layer available; live update skipped")
        return
    async_to_sync(channel_layer.group_send)(
        "station_live",
        {
            "type": "station.update",
            "html": render_live_fragment(reading),
            "payload": reading.to_payload(),
        },
    )


def _build_client():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=settings.MQTT_CLIENT_ID)

    def on_connect(client, _userdata, _flags, reason_code, _properties):
        code = getattr(reason_code, "value", reason_code)
        if code == 0:
            logger.info("MQTT bridge connected to %s:%s", settings.MQTT_HOST, settings.MQTT_PORT)
            client.subscribe(settings.MQTT_TOPIC, qos=1)
        else:
            logger.error("MQTT bridge connection failed: %s", reason_code)

    def on_message(_client, _userdata, message):
        try:
            payload = json.loads(message.payload.decode("utf-8"))
            reading = reading_from_payload(payload)
            reading.save()
            _publish_to_websocket(reading)
            logger.info("Stored reading for station %s", reading.station_id)
        except Exception:
            logger.exception("MQTT payload rejected on topic %s", message.topic)

    def on_disconnect(_client, _userdata, _flags, reason_code, _properties):
        logger.warning("MQTT bridge disconnected: %s", reason_code)

    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    return client


def _run_bridge():
    connect_mongo()
    while True:
        client = _build_client()
        try:
            client.connect(settings.MQTT_HOST, settings.MQTT_PORT, keepalive=60)
            client.loop_forever(retry_first_connection=True)
        except Exception:
            logger.exception("MQTT bridge crashed; retrying soon")
            sleep(5)
        finally:
            try:
                client.disconnect()
            except Exception:
                pass


def start_mqtt_bridge_once():
    global _bridge_started
    with _bridge_lock:
        if _bridge_started:
            return
        _bridge_started = True
        thread = threading.Thread(target=_run_bridge, name="mqtt-bridge", daemon=True)
        thread.start()
        logger.info("MQTT bridge thread started")
