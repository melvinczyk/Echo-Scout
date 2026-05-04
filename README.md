# Echo Scout

A small 2.8" handheld device that detects people and maps out rooms in real time. It doesn't just tell you if someone is nearby, but it shows you where they are. The mmWave radar covers an 80 degree area in front of the user, while the 8x8 ToF sensor maps the surrounding space into a 3D point cloud relative to the user.

---

## Hardware

<table>
  <tr>
    <td align="center" width="25%">
      <img src="https://store.freenove.com/cdn/shop/files/FNK0104B.PT02.jpg?v=1758974333&width=1946" width="180" alt="Freenove ESP32-S3 CYD"/><br/>
      <sub><b>Freenove ESP32-S3 CYD</b><br/>Display &amp; Processor</sub>
    </td>
    <td align="center" width="25%">
      <img src="https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcSMNge-s71oTejt2pHBqrB3LQoQy0v06oFbsw&s" width="180" alt="Ai-Thinker RD-03D"/><br/>
      <sub><b>Ai-Thinker RD-03D</b><br/>mmWave Radar</sub>
    </td>
    <td align="center" width="25%">
      <img src="https://cdn-shop.adafruit.com/970x728/4754-00.jpg" width="180" alt="Adafruit BNO085"/><br/>
      <sub><b>Adafruit BNO085</b><br/>IMU</sub>
    </td>
    <td align="center" width="25%">
      <img src="https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcRa24bJwrctWPU5tK5STVhxd0dh-QMhk89p-g&s" width="180" alt="SparkFun VL53L5CX"/><br/>
      <sub><b>SparkFun VL53L5CX</b><br/>8x8 ToF Sensor</sub>
    </td>
  </tr>
</table>

| Component | Part | Role |
|---|---|---|
| Display & Processor | Freenove ESP32-S3 CYD (2.8") | Main MCU with built-in touchscreen |
| mmWave Radar | Ai-Thinker RD-03D | Detects and tracks people |
| ToF Sensor | SparkFun VL53L5CX | 8x8 depth grid for room mapping |
| IMU | Adafruit BNO085 | Orientation, heading, compass |

---

## What It Does

- **People detection:** mmWave radar picks up and tracks people nearby and through thin walls
- **Room mapping:** the 8x8 ToF sensor builds a depth map of the space in front and around you
- **Orientation:** the IMU tracks heading and tilt and acceleration used for positioning and a compass
- **Touchscreen display:** all data visualized on the 2.8" screen in real time

