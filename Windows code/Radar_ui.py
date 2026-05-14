from __future__ import annotations

import math
from dataclasses import dataclass

from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import QColor, QFont, QPainter, QPen
from PySide6.QtWidgets import (
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QPushButton,
    QPlainTextEdit,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)


@dataclass
class RadarPoint:
    angle: int
    distance: int


class RadarCanvas(QWidget):
    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setMinimumSize(640, 420)
        self._points: list[RadarPoint] = []
        self._current_angle = 0
        self._max_distance_cm = 50
        self._nearest_distance_cm = 0

    def set_points(self, points: list[RadarPoint]) -> None:
        self._points = list(points)
        self.update()

    def set_current_angle(self, angle: int) -> None:
        self._current_angle = max(0, min(180, angle))
        self.update()

    def set_nearest_distance(self, distance_cm: int) -> None:
        self._nearest_distance_cm = max(0, distance_cm)
        self.update()

    def clear(self) -> None:
        self._points.clear()
        self._current_angle = 0
        self._nearest_distance_cm = 0
        self.update()

    def paintEvent(self, event) -> None:  # noqa: N802 - Qt API
        del event
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.fillRect(self.rect(), QColor(8, 13, 22))

        margin = 34
        width = self.width() - margin * 2
        height = self.height() - margin * 2
        radius = min(width / 2, height * 0.88)
        origin = QPointF(self.width() / 2, self.height() - margin)

        self._draw_grid(painter, origin, radius)
        self._draw_points(painter, origin, radius)
        self._draw_scan_line(painter, origin, radius)

    def _draw_grid(self, painter: QPainter, origin: QPointF, radius: float) -> None:
        grid_pen = QPen(QColor(28, 148, 104), 1)
        text_pen = QPen(QColor(132, 236, 190), 1)
        painter.setFont(QFont("Consolas", 9))

        for ratio in (0.25, 0.5, 0.75, 1.0):
            r = radius * ratio
            rect = QRectF(origin.x() - r, origin.y() - r, r * 2, r * 2)
            painter.setPen(grid_pen)
            painter.drawArc(rect, 0, 180 * 16)
            painter.setPen(text_pen)
            painter.drawText(QPointF(origin.x() + 8, origin.y() - r - 4), f"{int(self._max_distance_cm * ratio)}cm")

        for angle in range(0, 181, 30):
            end = self._polar_to_point(origin, radius, angle)
            painter.setPen(grid_pen)
            painter.drawLine(origin, end)
            label = self._polar_to_point(origin, radius + 18, angle)
            painter.setPen(text_pen)
            painter.drawText(label, f"{angle}°")

        painter.setPen(QPen(QColor(46, 205, 138), 2))
        painter.drawLine(QPointF(origin.x() - radius, origin.y()), QPointF(origin.x() + radius, origin.y()))

    def _draw_points(self, painter: QPainter, origin: QPointF, radius: float) -> None:
        for point in self._points:
            if point.distance <= 0:
                continue
            ratio = min(point.distance / self._max_distance_cm, 1.0)
            pos = self._polar_to_point(origin, radius * ratio, point.angle)
            color = QColor(255, 92, 92) if point.distance == self._nearest_distance_cm else QColor(255, 196, 87)
            painter.setPen(Qt.NoPen)
            painter.setBrush(color)
            painter.drawEllipse(pos, 4, 4)

    def _draw_scan_line(self, painter: QPainter, origin: QPointF, radius: float) -> None:
        end = self._polar_to_point(origin, radius, self._current_angle)
        painter.setPen(QPen(QColor(94, 229, 255), 2))
        painter.drawLine(origin, end)

    @staticmethod
    def _polar_to_point(origin: QPointF, radius: float, angle: int) -> QPointF:
        radians = math.radians(angle)
        x = origin.x() - math.cos(radians) * radius
        y = origin.y() - math.sin(radians) * radius
        return QPointF(x, y)


class RadarWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("STM32 超声波雷达上位机")
        self.resize(980, 680)

        central = QWidget(self)
        self.setCentralWidget(central)
        root = QHBoxLayout(central)

        self.canvas = RadarCanvas(self)
        root.addWidget(self.canvas, 1)

        side = QFrame(self)
        side.setFrameShape(QFrame.StyledPanel)
        side.setMinimumWidth(290)
        root.addWidget(side)
        side_layout = QVBoxLayout(side)

        title = QLabel("超声波雷达控制台")
        title.setFont(QFont("Microsoft YaHei", 17, QFont.Bold))
        side_layout.addWidget(title)

        form = QGridLayout()
        side_layout.addLayout(form)

        self.port_spin = QSpinBox()
        self.port_spin.setRange(1, 65535)
        self.port_spin.setValue(8089)
        form.addWidget(QLabel("TCP端口"), 0, 0)
        form.addWidget(self.port_spin, 0, 1)

        self.status_label = QLabel("已停止")
        form.addWidget(QLabel("服务状态"), 1, 0)
        form.addWidget(self.status_label, 1, 1)

        self.client_label = QLabel("无")
        form.addWidget(QLabel("客户端"), 2, 0)
        form.addWidget(self.client_label, 2, 1)

        self.angle_label = QLabel("0 度")
        form.addWidget(QLabel("扫描角度"), 3, 0)
        form.addWidget(self.angle_label, 3, 1)

        self.distance_label = QLabel("0 cm")
        form.addWidget(QLabel("当前距离"), 4, 0)
        form.addWidget(self.distance_label, 4, 1)

        self.nearest_label = QLabel("0 cm")
        form.addWidget(QLabel("最近距离"), 5, 0)
        form.addWidget(self.nearest_label, 5, 1)

        self.obstacle_label = QLabel("0")
        form.addWidget(QLabel("障碍物数"), 6, 0)
        form.addWidget(self.obstacle_label, 6, 1)

        button_row = QHBoxLayout()
        side_layout.addLayout(button_row)
        self.start_button = QPushButton("启动服务")
        self.stop_button = QPushButton("停止")
        self.clear_button = QPushButton("清空")
        button_row.addWidget(self.start_button)
        button_row.addWidget(self.stop_button)
        button_row.addWidget(self.clear_button)

        self.log_edit = QPlainTextEdit()
        self.log_edit.setReadOnly(True)
        self.log_edit.setMaximumBlockCount(500)
        side_layout.addWidget(QLabel("运行日志"))
        side_layout.addWidget(self.log_edit, 1)

        self.signature_label = QLabel("yoitsu")
        self.signature_label.setAlignment(Qt.AlignRight | Qt.AlignBottom)
        self.signature_label.setStyleSheet("color: #9fb8c8; font-size: 13px;")
        side_layout.addWidget(self.signature_label)

    def set_server_running(self, running: bool, port: int | None = None) -> None:
        self.status_label.setText(f"监听中:{port}" if running and port else "已停止")
        self.start_button.setEnabled(not running)
        self.stop_button.setEnabled(running)
        self.port_spin.setEnabled(not running)

    def set_client(self, client: str | None) -> None:
        self.client_label.setText(client or "无")

    def update_status(self, angle: int, distance: int, nearest: int, obstacles: int) -> None:
        self.angle_label.setText(f"{angle} 度")
        self.distance_label.setText(f"{distance} cm")
        self.nearest_label.setText(f"{nearest} cm")
        self.obstacle_label.setText(str(obstacles))
        self.canvas.set_current_angle(angle)
        self.canvas.set_nearest_distance(nearest)

    def set_points(self, points: list[RadarPoint]) -> None:
        self.canvas.set_points(points)

    def append_log(self, text: str) -> None:
        self.log_edit.appendPlainText(text)

    def clear_display(self) -> None:
        self.canvas.clear()
        self.angle_label.setText("0 度")
        self.distance_label.setText("0 cm")
        self.nearest_label.setText("0 cm")
        self.obstacle_label.setText("0")
        self.log_edit.clear()
