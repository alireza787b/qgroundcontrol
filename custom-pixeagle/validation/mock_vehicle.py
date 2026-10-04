#!/usr/bin/env python3
"""Loopback-only baseline mock with an explicit, synthetic aircraft identity."""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.simulation.mock_vehicle import MockVehicle, mavlink


class IdentifiedMockVehicle(MockVehicle):
    def __init__(self, system_id: int, uid: int):
        super().__init__(system_id=system_id)
        self.uid = uid

    def handle_command_long(self, msg):
        if (
            msg.command == mavlink.MAV_CMD_REQUEST_MESSAGE
            and int(msg.param1) == mavlink.MAVLINK_MSG_ID_AUTOPILOT_VERSION
        ):
            self.send_autopilot_version()
            self.send_command_ack(msg.command, mavlink.MAV_RESULT_ACCEPTED)
        else:
            super().handle_command_long(msg)

    def send_autopilot_version(self):
        self.connection.mav.autopilot_version_send(
            mavlink.MAV_PROTOCOL_CAPABILITY_MISSION_FLOAT
            | mavlink.MAV_PROTOCOL_CAPABILITY_PARAM_FLOAT
            | mavlink.MAV_PROTOCOL_CAPABILITY_COMMAND_INT,
            0x04000000,
            0,
            0,
            0,
            bytes(8),
            bytes(8),
            bytes(8),
            0,
            0,
            self.uid,
        )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--system-id", type=int, required=True)
    parser.add_argument("--uid", type=int, required=True)
    parser.add_argument("--port", type=int, default=14550)
    args = parser.parse_args()
    if not 1 <= args.system_id <= 255 or not 1 <= args.uid < 2**64:
        parser.error("Use a nonzero MAVLink system ID and an unsigned 64-bit UID.")
    vehicle = IdentifiedMockVehicle(args.system_id, args.uid)
    vehicle.state.lon += args.system_id * 0.0002
    vehicle.connect_udp("127.0.0.1", args.port)
    vehicle.run()


if __name__ == "__main__":
    main()
