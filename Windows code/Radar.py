from __future__ import annotations

import socket
import sys
from dataclasses import dataclass

from PySide6.QtCore import QObject, QThread, Signal, Slot
from PySide6.QtWidgets import QApplication

from Radar_ui import RadarPoint, RadarWindow


SECTION_SPLIT_THRESHOLD_CM = 1
MAX_DETECT_DISTANCE_CM = 50


@dataclass
class RadarSample:
    angle: int
    distance: int


@dataclass
class RadarSummary:
    nearest_distance: int = 0
    obstacle_count: int = 0


class RadarAnalyzer:
    def __init__(self) -> None:
        self.samples: dict[int, int] = {}

    def clear(self) -> None:
        self.samples.clear()

    def add_sample(self, angle: int, distance: int) -> RadarSummary:
        if angle == 0:
            self.clear()

        self.samples[angle] = distance
        return self.summary()

    def summary(self) -> RadarSummary:
        valid = sorted((angle, distance) for angle, distance in self.samples.items() if 0 < distance <= MAX_DETECT_DISTANCE_CM)
        if not valid:
            return RadarSummary()

        nearest = min(distance for _, distance in valid)
        obstacle_count = 1
        previous_distance = valid[0][1]

        for _, distance in valid[1:]:
            if abs(distance - previous_distance) > SECTION_SPLIT_THRESHOLD_CM:
                obstacle_count += 1
            previous_distance = distance

        return RadarSummary(nearest_distance=nearest, obstacle_count=obstacle_count)

    def points(self) -> list[RadarPoint]:
        return [RadarPoint(angle, distance) for angle, distance in sorted(self.samples.items())]


class TcpRadarServer(QObject):
    sample_received = Signal(int, int)
    client_changed = Signal(str)
    log_message = Signal(str)
    stopped = Signal()

    def __init__(self, port: int) -> None:
        super().__init__()
        self.port = port
        self._running = False
        self._server_socket: socket.socket | None = None
        self._client_socket: socket.socket | None = None

    @Slot()
    def run(self) -> None:
        self._running = True
        try:
            self._server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self._server_socket.bind(("0.0.0.0", self.port))
            self._server_socket.listen(1)
            self._server_socket.settimeout(0.5)
            self.log_message.emit(f"TCP服务已启动，监听 0.0.0.0:{self.port}")

            while self._running:
                try:
                    client, address = self._server_socket.accept()
                except socket.timeout:
                    continue

                with client:
                    self._client_socket = client
                    client.settimeout(0.5)
                    self.client_changed.emit(f"{address[0]}:{address[1]}")
                    self.log_message.emit(f"客户端已连接：{address[0]}:{address[1]}")
                    self._serve_client(client)
                    self.client_changed.emit("无")
                    self.log_message.emit("客户端已断开")
                    self._client_socket = None
        except OSError as exc:
            self.log_message.emit(f"服务错误：{exc}")
        finally:
            self.stop()
            self.stopped.emit()

    def stop(self) -> None:
        self._running = False
        for sock in (self._client_socket, self._server_socket):
            if sock is not None:
                try:
                    sock.close()
                except OSError:
                    pass
        self._client_socket = None
        self._server_socket = None

    def send_feedback(self, summary: RadarSummary) -> None:
        client = self._client_socket
        if client is None:
            return

        payload = f"R,{summary.nearest_distance},{summary.obstacle_count}\r\n".encode("ascii")
        try:
            client.sendall(payload)
            self.log_message.emit(f"发送 {payload.decode('ascii').strip()}")
        except OSError as exc:
            self.log_message.emit(f"发送失败：{exc}")

    def _serve_client(self, client: socket.socket) -> None:
        buffer = ""
        while self._running:
            try:
                data = client.recv(1024)
            except socket.timeout:
                continue
            except OSError:
                break

            if not data:
                break

            buffer += data.decode("ascii", errors="ignore")
            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)
                line = line.strip()
                if line:
                    self._handle_line(line)

    def _handle_line(self, line: str) -> None:
        self.log_message.emit(f"接收 {line}")
        sample = parse_sample(line)
        if sample is None:
            self.log_message.emit(f"忽略异常数据帧：{line}")
            return

        self.sample_received.emit(sample.angle, sample.distance)


def parse_sample(line: str) -> RadarSample | None:
    parts = line.split(",")
    if len(parts) < 3 or parts[0] != "P":
        return None

    try:
        angle = int(parts[1])
        distance = int(parts[2])
    except ValueError:
        return None

    if angle < 0 or angle > 180 or distance < 0:
        return None

    if distance > MAX_DETECT_DISTANCE_CM:
        distance = 0

    return RadarSample(angle=angle, distance=distance)


class RadarController(QObject):
    def __init__(self, window: RadarWindow) -> None:
        super().__init__()
        self.window = window
        self.analyzer = RadarAnalyzer()
        self.server: TcpRadarServer | None = None
        self.thread: QThread | None = None

        self.window.start_button.clicked.connect(self.start_server)
        self.window.stop_button.clicked.connect(self.stop_server)
        self.window.clear_button.clicked.connect(self.clear)
        self.window.set_server_running(False)

    @Slot()
    def start_server(self) -> None:
        if self.thread is not None:
            return

        port = self.window.port_spin.value()
        self.server = TcpRadarServer(port)
        self.thread = QThread()
        self.server.moveToThread(self.thread)

        self.thread.started.connect(self.server.run)
        self.server.sample_received.connect(self.on_sample_received)
        self.server.client_changed.connect(self.window.set_client)
        self.server.log_message.connect(self.window.append_log)
        self.server.stopped.connect(self.on_server_stopped)
        self.server.stopped.connect(self.thread.quit)
        self.thread.finished.connect(self.thread.deleteLater)

        self.thread.start()
        self.window.set_server_running(True, port)

    @Slot()
    def stop_server(self) -> None:
        if self.server is not None:
            self.server.stop()

    @Slot()
    def on_server_stopped(self) -> None:
        self.window.set_server_running(False)
        self.window.set_client(None)
        self.server = None
        self.thread = None

    @Slot(int, int)
    def on_sample_received(self, angle: int, distance: int) -> None:
        summary = self.analyzer.add_sample(angle, distance)
        self.window.set_points(self.analyzer.points())
        self.window.update_status(angle, distance, summary.nearest_distance, summary.obstacle_count)

        if self.server is not None:
            self.server.send_feedback(summary)

    @Slot()
    def clear(self) -> None:
        self.analyzer.clear()
        self.window.clear_display()


def main() -> int:
    app = QApplication(sys.argv)
    window = RadarWindow()
    controller = RadarController(window)
    window.radar_controller = controller
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
