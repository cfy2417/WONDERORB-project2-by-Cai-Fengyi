/*
================= 5分钟快速硬件验证程序 =================
目标：快速验证所有硬件基本功能正常
时间：总共5分钟，每个功能1分钟
输出：简洁明了的PASS/FAIL结果
适用：首次连接硬件后的快速验证
================================================================
*/

#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>

// 简化配置
const int SENSORS[] = {36, 39, 34, 35, 32, 33, 25, 26};
const int LEDS[] = {4, 5, 13, 14, 16, 17, 18, 19};
const int NUM_SENSORS = 8;
const int NUM_LEDS = 8;
const int MPU_ADDR = 0x68;

Adafruit_NeoPixel strips[8] = {
  Adafruit_NeoPixel(5, 4, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(5, 5, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(5, 13, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(5, 14, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(5, 16, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(5, 17, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(5, 18, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(5, 19, NEO_GRB + NEO_KHZ800)
};

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("🚀 ESP32硬件5分钟快速验证开始！");
  Serial.println("====================================");
  
  runQuickTests();
}

void runQuickTests() {
  bool allPassed = true;
  
  // 测试1：ESP32基本功能 (30秒)
  Serial.println("\n[1/5] 测试ESP32基本功能...");
  bool esp32OK = testESP32();
  Serial.printf("结果: %s\n", esp32OK ? "✅ PASS" : "❌ FAIL");
  if (!esp32OK) allPassed = false;
  
  // 测试2：LED灯带 (60秒)
  Serial.println("\n[2/5] 测试LED灯带...");
  bool ledsOK = testLEDs();
  Serial.printf("结果: %s\n", ledsOK ? "✅ PASS" : "❌ FAIL");
  if (!ledsOK) allPassed = false;
  
  // 测试3：压力传感器 (60秒)
  Serial.println("\n[3/5] 测试压力传感器...");
  bool sensorsOK = testSensors();
  Serial.printf("结果: %s\n", sensorsOK ? "✅ PASS" : "❌ FAIL");
  if (!sensorsOK) allPassed = false;
  
  // 测试4：MPU6050陀螺仪 (60秒)
  Serial.println("\n[4/5] 测试MPU6050陀螺仪...");
  bool mpuOK = testMPU6050();
  Serial.printf("结果: %s\n", mpuOK ? "✅ PASS" : "❌ FAIL");
  if (!mpuOK) allPassed = false;
  
  // 测试5：WiFi连接 (30秒)
  Serial.println("\n[5/5] 测试WiFi连接...");
  bool wifiOK = testWiFi();
  Serial.printf("结果: %s\n", wifiOK ? "✅ PASS" : "⚠️ SKIP");
  
  // 最终结果
  Serial.println("\n====================================");
  if (allPassed) {
    Serial.println("🎉 所有硬件测试通过！系统可以正常使用");
    flashSuccess();
  } else {
    Serial.println("⚠️ 部分硬件测试失败，请检查连接");
    flashError();
  }
  Serial.println("====================================");
}

bool testESP32() {
  Serial.println("检查ESP32基本功能...");
  
  // 检查内存
  int freeHeap = ESP.getFreeHeap();
  Serial.printf("可用内存: %d 字节", freeHeap);
  if (freeHeap < 100000) {
    Serial.println(" ❌ 内存不足");
    return false;
  }
  Serial.println(" ✅");
  
  // 检查芯片信息
  Serial.printf("芯片型号: %s ✅\n", ESP.getChipModel());
  Serial.printf("CPU频率: %d MHz ✅\n", ESP.getCpuFreqMHz());
  
  return true;
}

bool testLEDs() {
  Serial.println("初始化LED灯带...");
  
  // 初始化所有LED
  for (int i = 0; i < NUM_LEDS; i++) {
    strips[i].begin();
    strips[i].setBrightness(50);
    strips[i].clear();
    strips[i].show();
  }
  
  Serial.println("逐个点亮LED测试（观察每个灯带是否亮起）:");
  
  for (int i = 0; i < NUM_LEDS; i++) {
    // 点亮当前LED
    for (int j = 0; j < 5; j++) {
      strips[i].setPixelColor(j, strips[i].Color(255, 0, 0));
    }
    strips[i].show();
    
    Serial.printf("LED%d (GPIO%d) 点亮", i+1, LEDS[i]);
    delay(1000);
    
    // 关闭当前LED
    strips[i].clear();
    strips[i].show();
    Serial.println(" → 熄灭 ✅");
  }
  
  // 全部点亮测试
  Serial.println("全部LED点亮测试...");
  for (int i = 0; i < NUM_LEDS; i++) {
    for (int j = 0; j < 5; j++) {
      strips[i].setPixelColor(j, strips[i].Color(0, 255, 0));
    }
    strips[i].show();
  }
  delay(2000);
  
  // 全部关闭
  for (int i = 0; i < NUM_LEDS; i++) {
    strips[i].clear();
    strips[i].show();
  }
  
  return true; // 视觉检查，默认通过
}

bool testSensors() {
  Serial.println("测试压力传感器（请按压传感器）...");
  Serial.println("传感器 | GPIO | 基准值 | 当前值 | 状态");
  Serial.println("-------|------|--------|--------|------");
  
  int baseline[NUM_SENSORS];
  bool sensorWorking[NUM_SENSORS];
  
  // 读取基准值
  for (int i = 0; i < NUM_SENSORS; i++) {
    baseline[i] = analogRead(SENSORS[i]);
    sensorWorking[i] = false;
  }
  
  unsigned long startTime = millis();
  while (millis() - startTime < 15000) { // 15秒测试时间
    for (int i = 0; i < NUM_SENSORS; i++) {
      int current = analogRead(SENSORS[i]);
      bool triggered = (abs(current - baseline[i]) > 200);
      
      if (triggered) {
        sensorWorking[i] = true;
        
        // 点亮对应LED
        if (i < NUM_LEDS) {
          for (int j = 0; j < 5; j++) {
            strips[i].setPixelColor(j, strips[i].Color(0, 255, 255));
          }
          strips[i].show();
        }
      } else {
        // 关闭对应LED
        if (i < NUM_LEDS) {
          strips[i].clear();
          strips[i].show();
        }
      }
      
      // 每2秒更新一次显示
      if ((millis() - startTime) % 2000 < 100) {
        Serial.printf("   %d   | %2d   |  %4d  |  %4d  | %s\n",
                     i+1, SENSORS[i], baseline[i], current,
                     sensorWorking[i] ? "检测到" : "等待中");
      }
    }
    delay(50);
  }
  
  // 关闭所有LED
  for (int i = 0; i < NUM_LEDS; i++) {
    strips[i].clear();
    strips[i].show();
  }
  
  // 检查结果
  int workingSensors = 0;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensorWorking[i]) workingSensors++;
  }
  
  Serial.printf("工作正常的传感器: %d/%d\n", workingSensors, NUM_SENSORS);
  return workingSensors >= NUM_SENSORS / 2; // 至少一半传感器工作正常
}

bool testMPU6050() {
  Serial.println("初始化MPU6050...");
  
  Wire.begin(21, 22); // SDA=21, SCL=22
  
  // 检测设备
  Wire.beginTransmission(MPU_ADDR);
  byte error = Wire.endTransmission();
  
  if (error != 0) {
    Serial.printf("❌ MPU6050未检测到 (错误: %d)\n", error);
    return false;
  }
  
  Serial.println("✅ MPU6050检测到");
  
  // 唤醒设备
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); // PWR_MGMT_1
  Wire.write(0x00); // 唤醒
  Wire.endTransmission();
  
  delay(100);
  
  Serial.println("读取陀螺仪数据（请移动开发板）...");
  
  bool dataValid = false;
  for (int i = 0; i < 20; i++) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B); // ACCEL_XOUT_H
    Wire.endTransmission(false);
    
    if (Wire.requestFrom(MPU_ADDR, 6) == 6) {
      int16_t accelX = (Wire.read() << 8) | Wire.read();
      int16_t accelY = (Wire.read() << 8) | Wire.read();
      int16_t accelZ = (Wire.read() << 8) | Wire.read();
      
      Serial.printf("加速度 X:%6d Y:%6d Z:%6d\n", accelX, accelY, accelZ);
      
      // 检查数据是否合理（不全为0或全为-1）
      if (accelX != 0 || accelY != 0 || accelZ != 0) {
        if (accelX != -1 || accelY != -1 || accelZ != -1) {
          dataValid = true;
        }
      }
    } else {
      Serial.println("❌ 数据读取失败");
    }
    
    delay(200);
  }
  
  return dataValid;
}

bool testWiFi() {
  Serial.println("测试WiFi连接...");
  
  WiFi.begin("Qifei", "88888888");
  Serial.print("连接中");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 15) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n✅ WiFi连接成功: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    Serial.println("\n⚠️ WiFi连接失败（可选功能）");
    return false; // WiFi失败不影响整体评估
  }
}

void flashSuccess() {
  // 成功闪烁：绿色呼吸灯
  for (int cycle = 0; cycle < 3; cycle++) {
    for (int brightness = 0; brightness < 255; brightness += 5) {
      for (int i = 0; i < NUM_LEDS; i++) {
        for (int j = 0; j < 5; j++) {
          strips[i].setPixelColor(j, strips[i].Color(0, brightness, 0));
        }
        strips[i].show();
      }
      delay(10);
    }
    for (int brightness = 255; brightness > 0; brightness -= 5) {
      for (int i = 0; i < NUM_LEDS; i++) {
        for (int j = 0; j < 5; j++) {
          strips[i].setPixelColor(j, strips[i].Color(0, brightness, 0));
        }
        strips[i].show();
      }
      delay(10);
    }
  }
  
  // 关闭所有LED
  for (int i = 0; i < NUM_LEDS; i++) {
    strips[i].clear();
    strips[i].show();
  }
}

void flashError() {
  // 错误闪烁：红色快闪
  for (int cycle = 0; cycle < 5; cycle++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      for (int j = 0; j < 5; j++) {
        strips[i].setPixelColor(j, strips[i].Color(255, 0, 0));
      }
      strips[i].show();
    }
    delay(200);
    
    for (int i = 0; i < NUM_LEDS; i++) {
      strips[i].clear();
      strips[i].show();
    }
    delay(200);
  }
}

void loop() {
  // 测试完成后进入简单监控模式
  static unsigned long lastCheck = 0;
  
  if (millis() - lastCheck > 5000) {
    Serial.println("💡 测试完成，系统监控中... (按复位键重新测试)");
    lastCheck = millis();
    
    // 简单的传感器监控
    for (int i = 0; i < NUM_SENSORS; i++) {
      int value = analogRead(SENSORS[i]);
      if (value > 2000) {
        // 传感器触发时点亮对应LED
        if (i < NUM_LEDS) {
          for (int j = 0; j < 5; j++) {
            strips[i].setPixelColor(j, strips[i].Color(100, 100, 0));
          }
          strips[i].show();
          delay(100);
          strips[i].clear();
          strips[i].show();
        }
        Serial.printf("传感器%d触发！\n", i+1);
      }
    }
  }
  
  delay(100);
}

/*
================= 快速测试使用说明 =================

🎯 使用场景：
- 首次硬件连接后验证
- 故障排除时的快速检查
- 系统维护时的功能验证

⏱️ 测试时间：
- ESP32基本功能: 30秒
- LED灯带测试: 60秒 (观察每个LED是否点亮)
- 压力传感器: 60秒 (按压传感器测试)
- MPU6050陀螺仪: 60秒 (移动开发板)
- WiFi连接: 30秒
- 总计: 约5分钟

📊 判断标准：
✅ PASS: 功能正常工作
❌ FAIL: 功能异常，需要检查
⚠️ SKIP: 可选功能，不影响整体

🔍 观察要点：
1. LED测试时观察每个灯带是否依次点亮
2. 传感器测试时按压传感器看LED响应
3. MPU测试时移动开发板观察数据变化
4. 串口输出显示详细的测试结果

🛠️ 如果测试失败：
- LED不亮: 检查电源和数据线
- 传感器无响应: 检查引脚连接
- MPU6050失败: 检查I2C连接
- WiFi连接失败: 检查网络配置

===============================================
*/