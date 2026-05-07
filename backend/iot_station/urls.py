from django.urls import path

from dashboard import views

urlpatterns = [
    path("", views.dashboard, name="dashboard"),
    path("health/", views.health, name="health"),
    path("api/history/", views.history, name="history"),
]
