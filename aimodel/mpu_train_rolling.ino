// Place this in a separate Arduino sketch named, e.g., firmware.ino

#include <Wire.h>
#include "MPU6050.h"

MPU6050 mpu;

void setup() {
  Serial.begin(115200);    // Edge Impulse CLI 推荐 115200
  Wire.begin();
  mpu.initialize();

  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed");
    while (1);
  }
}

void loop() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  // 转换为 g 单位
  float x = ax / 16384.0;
  float y = ay / 16384.0;
  float z = az / 16384.0;

  // 按 CSV 格式输出，Edge Impulse Data Forwarder 可识别
  Serial.print(x, 4);
  Serial.print(",");
  Serial.print(y, 4);
  Serial.print(",");
  Serial.println(z, 4);

  delay(10);  // 对应采样频率 100Hz
}
