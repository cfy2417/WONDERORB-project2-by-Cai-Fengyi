
#include <PressureSensorClassification_inferencing.h>  // Edge Impulse 自动生成

#define SENSOR_PIN 36
#define LED_PIN 13

float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  // 模拟采样 10s 的传感器数据（假设 10Hz 采样率）
  for (int i = 0; i < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; i++) {
    features[i] = analogRead(SENSOR_PIN); // 模拟 FSR 床感器
    delay(100); // 每100ms采样一次
  }

  // 构建 signal 结构体
  signal_t signal;
  signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
  signal.get_data = [](size_t offset, size_t length, float *out_ptr) -> int {
    memcpy(out_ptr, features + offset, length * sizeof(float));
    return 0;
  };

  // 推理模型
  ei_impulse_result_t result;
  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

  // 显示推理结果
  Serial.println("推理结果：");
  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    Serial.print(result.classification[i].label);
    Serial.print(": ");
    Serial.println(result.classification[i].value);
  }

  // 如果判断为“child_playing”且概率较高，则亮灯
  if (strcmp(result.classification[0].label, "child_playing") == 0 &&
      result.classification[0].value > 0.8) {
    digitalWrite(LED_PIN, HIGH); // 灯亮
  } else {
    digitalWrite(LED_PIN, LOW);  // 灭灯
  }

  delay(500); // 每次循环间隔
}