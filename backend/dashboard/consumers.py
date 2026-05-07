from channels.generic.websocket import AsyncWebsocketConsumer


class StationConsumer(AsyncWebsocketConsumer):
    group_name = "station_live"

    async def connect(self):
        await self.channel_layer.group_add(self.group_name, self.channel_name)
        await self.accept()

    async def disconnect(self, _close_code):
        await self.channel_layer.group_discard(self.group_name, self.channel_name)

    async def station_update(self, event):
        await self.send(text_data=event["html"])
