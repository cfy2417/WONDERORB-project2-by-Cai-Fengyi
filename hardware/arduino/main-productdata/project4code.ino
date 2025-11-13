
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <DFRobotDFPlayerMini.h>
#include <mpu_inferencing.h>
#include <Wire.h>

/*
================= 超精简版ESP32游戏系统音效架构 =================

🎵 4个超精简音效设计：

【成功音效】(2个)
1. stage_hit      - 阶段1成功 + 阶段2普通成功（合并）
2. stage2_excellent - 阶段2优秀命中（≥3次）

【失败音效】(1个)  
3. miss           - 统一失败反馈（所有阶段通用）

【系统音效】(1个)
4. system_sound   - 开机关机 + 阶段完成 + 系统重置（全合并）

🎮 超精简逻辑：
- 阶段1成功: stage_hit
- 阶段2成功: <3次→stage_hit, ≥3次→stage2_excellent
- 失败: 任何失败→miss（统一反馈）
- 系统事件: 全部→system_sound（万能系统音效）

📁 SD卡文件需求：001.mp3 - 004.mp3 (共4个文件)
=============================================================
*/

// ================= 系统配置 =================
namespace Config {
    // 硬件配置
    const int NUM_SENSORS = 8;
    const int NUM_LEDS = 8;
    const int LEDS_PER_STRIP = 60;
    
    // 传感器阈值
    const int SENSOR_THRESHOLD = 300;
    const int SENSOR_THRESHOLD_STAGE2 = 200;
    
    // 加热器配置
    const unsigned long HEATER_MAX_TIME = 25000;
    const unsigned long HEATER_COOLDOWN = 3000;
    
    // AI配置
    const unsigned long AI_CHECK_INTERVAL = 30000;
    const int MPU_SAMPLE_RATE_MS = 10;
    
    // 陀螺仪判断阈值
    const float GYRO_ROLLING_THRESHOLD = 200.0;
    const float GYRO_PLAYING_THRESHOLD = 50.0;
    
    // 🎵 精简音效配置
    const int DEFAULT_VOLUME = 20;
    const int SUCCESS_VOLUME = 18;
    const int MISS_VOLUME = 15;
    const int SYSTEM_VOLUME = 25;
    const unsigned long MIN_SOUND_INTERVAL = 300;
    
    // WiFi配置
    const char* WIFI_SSID = "Qifei";
    const char* WIFI_PASSWORD = "88888888";
    
    // 引脚定义
    namespace Pins {
        const int SENSORS[NUM_SENSORS] = {36, 39, 34, 35, 32, 33, 25, 26};
        const int LEDS[NUM_LEDS] = {4, 5, 13, 14, 16, 17, 18, 19};
        const int HEATERS[3] = {23, 27, 12};
        const int DFPLAYER_RX = 15;
        const int DFPLAYER_TX = 2;
        const int MPU_SDA = 21;
        const int MPU_SCL = 22;
        const int MPU_ADDR = 0x68;
    }
}

// ================= 精简音效类型定义 =================
enum SoundType {
    SOUND_SUCCESS,    // 成功音效
    SOUND_MISS,       // 失败音效
    SOUND_SYSTEM      // 系统音效
};

struct SoundEffect {
    int trackId;
    SoundType type;
    String name;
    int volume;
    unsigned long duration;
};

// ================= 系统状态定义 =================
enum SystemState {
    STAGE0_STARTUP,
    STAGE1_TIMER1,
    STAGE2_TIMER2,
    SYSTEM_SHUTDOWN
};

// ================= LED颜色管理 =================
class LEDColorManager {
public:
    struct Color { int r, g, b; };
    
    static Color getColor(int ledIndex) {
        if (ledIndex <= 4) return {255, 255, 255};
        else if (ledIndex == 5) return {255, 255, 255};
        else if (ledIndex == 6) return {255, 255, 255};
        else return {255, 255, 255};
    }
};

// ================= 传感器管理模块 =================
class SensorManager {
public:
    void init() {
        for (int i = 0; i < Config::NUM_SENSORS; i++) {
            sensorValues[i] = 0;
            triggerCount[i] = 0;
        }
        Serial.println("[SENSOR] 传感器管理器初始化完成");
    }
    
    void readSensors() {
        for (int i = 0; i < 6; i++) {
            sensorValues[i] = analogRead(Config::Pins::SENSORS[i]);
        }
        readADC2SensorsSimple();
    }
    
    bool isTriggered(int index, bool useStage2Threshold = false) {
        if (index < 0 || index >= Config::NUM_SENSORS) return false;
        int threshold = useStage2Threshold ? Config::SENSOR_THRESHOLD_STAGE2 : Config::SENSOR_THRESHOLD;
        return sensorValues[index] > threshold;
    }
    
    int getValue(int index) {
        if (index < 0 || index >= Config::NUM_SENSORS) return 0;
        return sensorValues[index];
    }
    
    int getTriggerCount(int index) {
        if (index < 0 || index >= Config::NUM_SENSORS) return 0;
        return triggerCount[index];
    }
    
    void incrementTriggerCount(int index) {
        if (index >= 0 && index < Config::NUM_SENSORS) {
            triggerCount[index]++;
        }
    }
    
    void resetTriggerCounts() {
        for (int i = 0; i < Config::NUM_SENSORS; i++) {
            triggerCount[i] = 0;
        }
    }

private:
    int sensorValues[Config::NUM_SENSORS];
    int triggerCount[Config::NUM_SENSORS];
    
    void readADC2SensorsSimple() {
        static unsigned long lastADC2Read = 0;
        if (millis() - lastADC2Read > 100) {
            for (int i = 6; i < Config::NUM_SENSORS; i++) {
                sensorValues[i] = analogRead(Config::Pins::SENSORS[i]);
            }
            lastADC2Read = millis();
        }
    }
};

// ================= LED控制模块 =================
class LEDController {
public:
    void init() {
        for (int i = 0; i < Config::NUM_LEDS; i++) {
            strips[i] = new Adafruit_NeoPixel(Config::LEDS_PER_STRIP,
                                              Config::Pins::LEDS[i],
                                              NEO_GRB + NEO_KHZ800);
            strips[i]->begin();
            strips[i]->setBrightness(80);
            strips[i]->clear();
            strips[i]->show();
        }
        Serial.println("[LED] LED控制器初始化完成");
    }
    
    void setLed(int ledIndex, bool on = true) {
        if (ledIndex < 0 || ledIndex >= Config::NUM_LEDS) return;
        
        if (on) {
            auto color = LEDColorManager::getColor(ledIndex);
            for (int i = 0; i < Config::LEDS_PER_STRIP; i++) {
                strips[ledIndex]->setPixelColor(i, strips[ledIndex]->Color(color.r, color.g, color.b));
            }
        } else {
            strips[ledIndex]->clear();
        }
        strips[ledIndex]->show();
    }
    
    void turnOffAllLeds() {
        for (int i = 0; i < Config::NUM_LEDS; i++) {
            strips[i]->clear();
            strips[i]->show();
        }
    }
    
    void flashLed(int ledIndex, int times = 3) {
        auto color = LEDColorManager::getColor(ledIndex);
        for (int i = 0; i < times; i++) {
            setLed(ledIndex, true);
            delay(150);
            setLed(ledIndex, false);
            delay(150);
        }
    }
    
    void irregularFlash(int startLed, int endLed, unsigned long duration) {
        unsigned long startTime = millis();
        while (millis() - startTime < duration) {
            int numLeds = random(1, 3);
            turnOffAllLeds();
            for (int j = 0; j < numLeds; j++) {
                int ledIndex = random(startLed, endLed + 1);
                setLed(ledIndex, true);
            }
            delay(random(200, 500));
        }
        turnOffAllLeds();
    }

private:
    Adafruit_NeoPixel* strips[Config::NUM_LEDS];
};

// ================= 加热器安全控制模块 =================
class HeaterController {
public:
    void init() {
        for (int i = 0; i < 3; i++) {
            pinMode(Config::Pins::HEATERS[i], OUTPUT);
            digitalWrite(Config::Pins::HEATERS[i], LOW);
            heaterState[i] = {0, false, 0};
        }
        Serial.println("[HEATER] 加热器安全控制初始化完成");
    }
    
    void safeHeaterOn(int heaterNum) {
        if (heaterNum < 1 || heaterNum > 3) return;
        int idx = heaterNum - 1;
        
        if (heaterState[idx].lastOffTime > 0 &&
            (millis() - heaterState[idx].lastOffTime < Config::HEATER_COOLDOWN)) {
            Serial.printf("[SAFETY] 加热模块%d冷却中，拒绝开启\n", heaterNum);
            return;
        }
        
        if (heaterState[idx].isOn) return;
        
        heaterState[idx].startTime = millis();
        heaterState[idx].isOn = true;
        digitalWrite(Config::Pins::HEATERS[idx], HIGH);
        Serial.printf("[HEATER] 开启加热模块%d\n", heaterNum);
    }
    
    void safeHeaterOff(int heaterNum) {
        if (heaterNum < 1 || heaterNum > 3) return;
        int idx = heaterNum - 1;
        
        if (!heaterState[idx].isOn) return;
        
        heaterState[idx].isOn = false;
        heaterState[idx].lastOffTime = millis();
        digitalWrite(Config::Pins::HEATERS[idx], LOW);
        Serial.printf("[HEATER] 关闭加热模块%d\n", heaterNum);
    }
    
    void checkSafety() {
        for (int i = 0; i < 3; i++) {
            if (heaterState[i].isOn &&
                (millis() - heaterState[i].startTime > Config::HEATER_MAX_TIME)) {
                Serial.printf("[SAFETY] 加热模块%d超时，强制关闭！\n", i + 1);
                safeHeaterOff(i + 1);
            }
        }
    }
    
    void turnOffAll() {
        for (int i = 1; i <= 3; i++) {
            safeHeaterOff(i);
        }
    }
    
    bool isHeaterOn(int heaterNum) {
        if (heaterNum < 1 || heaterNum > 3) return false;
        return heaterState[heaterNum - 1].isOn;
    }

private:
    struct HeaterState {
        unsigned long startTime;
        bool isOn;
        unsigned long lastOffTime;
    } heaterState[3];
};

// ================= 精简声音播放系统 =================
class SimplifiedSoundPlayer {
public:
    void init(DFRobotDFPlayerMini* dfplayer, HardwareSerial* serial) {
        this->dfplayer = dfplayer;
        this->mySerial = serial;
        
        // 初始化精简音效库
        initSoundLibrary();
        
        // 状态初始化
        lastPlayTime = 0;
        currentlyPlaying = false;
        successCount = 0;
        missCount = 0;
        systemMuted = false;
        
        Serial.println("[SOUND] 🎵 超精简声音播放系统初始化完成");
        printSoundLibrary();
    }
    
    // ================= 超精简游戏音效接口 =================
    
    // 🎮 成功音效（2个）
    bool playStageHit() {
        return playSound("stage_hit");
    }
    
    bool playStage2Excellent() {
        return playSound("stage2_excellent");
    }
    
    // 🎮 统一失败音效（1个）
    bool playMiss() {
        return playSound("miss");
    }
    
    // 🎮 万能系统音效（1个）
    bool playSystemSound() {
        return playSound("system_sound");
    }
    
    // ================= 兼容性接口（全部指向对应音效） =================
    
    // 阶段成功音效（合并版）
    bool playStage1Hit() { return playStageHit(); }
    bool playStage2Hit() { return playStageHit(); }
    
    // 系统音效（全部合并版）
    bool playStartup() { return playSystemSound(); }
    bool playShutdown() { return playSystemSound(); }
    bool playStageComplete() { return playSystemSound(); }
    bool playSystemReset() { return playSystemSound(); }
    
    // ================= 播放控制 =================
    
    void setVolume(int volume) {
        currentVolume = constrain(volume, 0, 30);
        if (dfplayer) {
            dfplayer->volume(currentVolume);
            delay(50);
        }
        Serial.printf("[SOUND] 🔊 音量设置: %d\n", currentVolume);
    }
    
    void setMute(bool mute) {
        systemMuted = mute;
        Serial.printf("[SOUND] %s\n", mute ? "🔇 静音开启" : "🔊 静音关闭");
    }
    
    bool isPlaying() {
        return currentlyPlaying && (millis() - lastPlayTime < currentDuration + 500);
    }
    
    void update() {
        // 更新播放状态
        if (currentlyPlaying && (millis() - lastPlayTime > currentDuration + 500)) {
            currentlyPlaying = false;
        }
        
        // 定期状态报告
        static unsigned long lastReport = 0;
        if (millis() - lastReport > 60000) {
            printPlayStats();
            lastReport = millis();
        }
    }
    
    // ================= 状态查询 =================
    
    void printPlayStats() {
        Serial.println("\n=== 🎵 超精简音效统计 ===");
        Serial.printf("✅ 成功音效: %d次\n", successCount);
        Serial.printf("❌ 失败音效: %d次\n", missCount);
        Serial.printf("📊 成功率: %.1f%%\n", getTotalPlays() > 0 ? (float)successCount / getTotalPlays() * 100 : 0);
        Serial.printf("🔊 当前音量: %d\n", currentVolume);
        Serial.printf("🔇 静音状态: %s\n", systemMuted ? "是" : "否");
        Serial.printf("▶️ 播放状态: %s\n", isPlaying() ? "播放中" : "空闲");
        Serial.println("========================\n");
    }
    
    int getTotalPlays() {
        return successCount + missCount;
    }
    
    float getSuccessRate() {
        int total = getTotalPlays();
        return total > 0 ? (float)successCount / total * 100.0 : 0.0;
    }

private:
    DFRobotDFPlayerMini* dfplayer;
    HardwareSerial* mySerial;
    
    // 超精简音效库
    SoundEffect soundLibrary[4];
    int librarySize;
    
    // 播放状态
    unsigned long lastPlayTime;
    bool currentlyPlaying;
    unsigned long currentDuration;
    int currentVolume = Config::DEFAULT_VOLUME;
    bool systemMuted;
    
    // 统计信息
    int successCount;
    int missCount;
    
    // ================= 精简音效库初始化 =================
    
    void initSoundLibrary() {
        librarySize = 0;
        
        // 🎮 成功音效（2个）
        addSound({1, SOUND_SUCCESS, "stage_hit", Config::SUCCESS_VOLUME, 1200});
        addSound({2, SOUND_SUCCESS, "stage2_excellent", Config::SUCCESS_VOLUME + 2, 1800});
        
        // 🎮 统一失败音效（1个）
        addSound({3, SOUND_MISS, "miss", Config::MISS_VOLUME, 1000});
        
        // 🎯 万能系统音效（1个）
        addSound({4, SOUND_SYSTEM, "system_sound", Config::SYSTEM_VOLUME, 2500});
        
        Serial.printf("[SOUND] 🎵 超精简音效库加载完成，共 %d 个音效\n", librarySize);
    }
    
    void addSound(const SoundEffect& effect) {
        if (librarySize < 4) {
            soundLibrary[librarySize++] = effect;
        }
    }
    
    SoundEffect* findSound(const String& name) {
        for (int i = 0; i < librarySize; i++) {
            if (soundLibrary[i].name == name) {
                return &soundLibrary[i];
            }
        }
        return nullptr;
    }
    
    void printSoundLibrary() {
        Serial.println("\n=== 🎵 超精简音效库列表 ===");
        const char* typeNames[] = {"成功", "失败", "系统"};
        
        for (int i = 0; i < librarySize; i++) {
            const SoundEffect& sound = soundLibrary[i];
            Serial.printf("🎵 %s (轨道%d) - %s - 音量%d - %lums\n",
                         sound.name.c_str(), sound.trackId,
                         typeNames[sound.type], sound.volume, sound.duration);
        }
        Serial.println("========================\n");
    }
    
    // ================= 核心播放逻辑 =================
    
    bool playSound(const String& soundName) {
        SoundEffect* sound = findSound(soundName);
        if (!sound) {
            Serial.printf("[SOUND] ❌ 音效未找到: %s\n", soundName.c_str());
            return false;
        }
        
        // 静音检查（系统音效除外）
        if (systemMuted && sound->type != SOUND_SYSTEM) {
            Serial.printf("[SOUND] 🔇 静音模式，跳过: %s\n", soundName.c_str());
            return false;
        }
        
        // 播放间隔检查
        if (millis() - lastPlayTime < Config::MIN_SOUND_INTERVAL) {
            Serial.printf("[SOUND] ⏸️ 播放间隔过短，跳过: %s\n", soundName.c_str());
            return false;
        }
        
        // 当前播放检查
        if (isPlaying() && sound->type != SOUND_SYSTEM) {
            Serial.printf("[SOUND] 🚫 播放冲突，跳过: %s\n", soundName.c_str());
            return false;
        }
        
        // 设置音量
        if (sound->volume != currentVolume) {
            dfplayer->volume(sound->volume);
            delay(30);
        }
        
        // 执行播放
        Serial.printf("[SOUND] 🎵 播放: %s (轨道%d, 音量%d)\n",
                     soundName.c_str(), sound->trackId, sound->volume);
        
        dfplayer->play(sound->trackId);
        delay(50);
        
        // 更新播放状态
        lastPlayTime = millis();
        currentlyPlaying = true;
        currentDuration = sound->duration;
        
        // 更新统计
        if (sound->type == SOUND_SUCCESS) {
            successCount++;
        } else if (sound->type == SOUND_MISS) {
            missCount++;
        }
        
        // 恢复标准音量
        if (sound->volume != currentVolume) {
            delay(50);
            dfplayer->volume(currentVolume);
        }
        
        return true;
    }
};

// ================= 精简MP3播放器 =================
class MP3Player {
public:
    void init() {
        pinMode(Config::Pins::DFPLAYER_TX, OUTPUT);
        digitalWrite(Config::Pins::DFPLAYER_TX, LOW);
        delay(200);
        
        mySerial.begin(9600, SERIAL_8N1, Config::Pins::DFPLAYER_RX, Config::Pins::DFPLAYER_TX);
        mySerial.flush();
        delay(1500);
        
        Serial.println("[MP3] DFPlayer初始化中...");
        
        initSuccess = performInit();
        
        if (initSuccess) {
            Serial.println("[MP3] ✅ DFPlayer初始化成功");
            
            // 🎵 初始化超精简声音播放系统
            soundPlayer.init(&dfplayer, &mySerial);
            soundPlayer.setVolume(Config::DEFAULT_VOLUME);
            
        } else {
            Serial.println("[MP3] ❌ DFPlayer初始化失败");
        }
    }
    
    // ================= 超精简游戏音效接口 =================
    
    // 成功音效（合并版）
    bool playStage1Success() { return soundPlayer.playStageHit(); }
    bool playStage2Success() { return soundPlayer.playStageHit(); }
    bool playStage2Excellent() { return soundPlayer.playStage2Excellent(); }
    
    // 统一失败音效
    bool playMiss() { return soundPlayer.playMiss(); }
    
    // 万能系统音效（合并版）
    bool playStartup() { return soundPlayer.playSystemSound(); }
    bool playShutdown() { return soundPlayer.playSystemSound(); }
    bool playStageComplete() { return soundPlayer.playSystemSound(); }
    bool playSystemReset() { return soundPlayer.playSystemSound(); }
    
    // 基础控制
    void setVolume(int vol) { soundPlayer.setVolume(vol); }
    void setMute(bool mute) { soundPlayer.setMute(mute); }
    bool isPlaying() { return soundPlayer.isPlaying(); }
    
    // 状态查询
    void printStatus() { soundPlayer.printPlayStats(); }
    bool isAvailable() { return initSuccess; }
    float getSuccessRate() { return soundPlayer.getSuccessRate(); }
    
    void checkStatus() {
        soundPlayer.update();
        
        // 简化的DFPlayer消息处理
        static unsigned long lastCheck = 0;
        if (millis() - lastCheck > 3000) {
            while (dfplayer.available()) {
                uint8_t type = dfplayer.readType();
                int value = dfplayer.read();
                
                if (type == DFPlayerCardRemoved) {
                    Serial.println("[MP3] ⚠️ SD卡被移除");
                } else if (type == DFPlayerCardOnline) {
                    Serial.println("[MP3] ✅ SD卡重新插入");
                }
            }
            lastCheck = millis();
        }
    }

private:
    HardwareSerial mySerial{2};
    DFRobotDFPlayerMini dfplayer;
    SimplifiedSoundPlayer soundPlayer;
    bool initSuccess = false;
    
    bool performInit() {
        for (int attempt = 1; attempt <= 3; attempt++) {
            Serial.printf("[MP3] 初始化尝试 %d/3\n", attempt);
            if (dfplayer.begin(mySerial, true, false)) {
                delay(500);
                dfplayer.volume(Config::DEFAULT_VOLUME);
                delay(100);
                return true;
            }
            delay(1000);
        }
        return false;
    }
};

// ================= AI推理模块 =================
class AIInference {
public:
    void init() {
        inference_buffer_idx = 0;
        buffer_ready = false;
        last_ai_check_time = 0;
        mpu_initialized = false;
        
        Wire.begin(Config::Pins::MPU_SDA, Config::Pins::MPU_SCL);
        Wire.beginTransmission(Config::Pins::MPU_ADDR);
        if (Wire.endTransmission() == 0) {
            Wire.beginTransmission(Config::Pins::MPU_ADDR);
            Wire.write(0x6B);
            Wire.write(0x00);
            Wire.endTransmission();
            
            mpu_initialized = true;
            Serial.println("[AI] MPU6050初始化成功");
        } else {
            Serial.println("[AI] MPU6050初始化失败");
        }
        
        Serial.println("[AI] AI推理模块初始化完成");
    }
    
    void collectMPUData() {
        if (!mpu_initialized) return;
        
        static unsigned long lastSample = 0;
        
        if (millis() - last_ai_check_time >= Config::AI_CHECK_INTERVAL) {
            if (!buffer_ready) {
                inference_buffer_idx = 0;
                last_ai_check_time = millis();
                Serial.println("[AI] 开始新的30秒陀螺仪数据收集");
            }
        }
        
        if (millis() - last_ai_check_time < Config::AI_CHECK_INTERVAL &&
            millis() - lastSample >= Config::MPU_SAMPLE_RATE_MS) {
            
            lastSample = millis();
            MPUData mpuData = readMPU6050();
            
            if (inference_buffer_idx + 2 < 999) {
                inference_buffer[inference_buffer_idx++] = mpuData.gyroX;
                inference_buffer[inference_buffer_idx++] = mpuData.gyroY;
                inference_buffer[inference_buffer_idx++] = mpuData.gyroZ;
            }
        }
        
        if (millis() - last_ai_check_time >= Config::AI_CHECK_INTERVAL && !buffer_ready) {
            buffer_ready = true;
            Serial.printf("[AI] 陀螺仪数据收集完成 (%d数据点)\n", inference_buffer_idx);
            
            while (inference_buffer_idx < 999) {
                inference_buffer[inference_buffer_idx++] = 0.0f;
            }
        }
    }
    
    bool isReadyFor30sCheck() {
        return buffer_ready;
    }
    
    String runInference() {
        if (!buffer_ready || !mpu_initialized) return "no_data";
        
        Serial.println("[AI] 执行陀螺仪数据推理...");
        
        float avgGyroMagnitude = 0;
        float maxGyroMagnitude = 0;
        int sampleCount = inference_buffer_idx / 3;
        
        for (int i = 0; i < sampleCount; i++) {
            float gx = inference_buffer[i * 3];
            float gy = inference_buffer[i * 3 + 1];
            float gz = inference_buffer[i * 3 + 2];
            float magnitude = sqrt(gx*gx + gy*gy + gz*gz);
            
            avgGyroMagnitude += magnitude;
            if (magnitude > maxGyroMagnitude) {
                maxGyroMagnitude = magnitude;
            }
        }
        
        if (sampleCount > 0) {
            avgGyroMagnitude /= sampleCount;
        }
        
        Serial.printf("[AI] 分析结果: 平均%.2f°/s, 最大%.2f°/s\n", 
                     avgGyroMagnitude, maxGyroMagnitude);
        
        String predictedLabel;
        if (maxGyroMagnitude > Config::GYRO_ROLLING_THRESHOLD) {
            predictedLabel = "rolling";
        } else if (avgGyroMagnitude > Config::GYRO_PLAYING_THRESHOLD) {
            predictedLabel = "playing";
        } else {
            predictedLabel = "stop";
        }
        
        Serial.printf("[AI] 最终结果: %s\n", predictedLabel.c_str());
        
        buffer_ready = false;
        inference_buffer_idx = 0;
        
        return predictedLabel;
    }
    
    bool isMPUAvailable() {
        return mpu_initialized;
    }

private:
    float inference_buffer[1000];
    uint32_t inference_buffer_idx;
    bool buffer_ready;
    unsigned long last_ai_check_time;
    bool mpu_initialized;
    
    struct MPUData {
        float accelX, accelY, accelZ;
        float gyroX, gyroY, gyroZ;
    };
    
    MPUData readMPU6050() {
        MPUData data = {0, 0, 0, 0, 0, 0};
        
        Wire.beginTransmission(Config::Pins::MPU_ADDR);
        Wire.write(0x3B);
        Wire.endTransmission(false);
        if (Wire.requestFrom(Config::Pins::MPU_ADDR, 6) == 6) {
            int16_t ax = (Wire.read() << 8) | Wire.read();
            int16_t ay = (Wire.read() << 8) | Wire.read();
            int16_t az = (Wire.read() << 8) | Wire.read();
            data.accelX = ax / 16384.0;
            data.accelY = ay / 16384.0;
            data.accelZ = az / 16384.0;
        }
        
        Wire.beginTransmission(Config::Pins::MPU_ADDR);
        Wire.write(0x43);
        Wire.endTransmission(false);
        if (Wire.requestFrom(Config::Pins::MPU_ADDR, 6) == 6) {
            int16_t gx = (Wire.read() << 8) | Wire.read();
            int16_t gy = (Wire.read() << 8) | Wire.read();
            int16_t gz = (Wire.read() << 8) | Wire.read();
            data.gyroX = gx / 131.0;
            data.gyroY = gy / 131.0;
            data.gyroZ = gz / 131.0;
        }
        
        return data;
    }
};

// ================= 精简状态机管理 =================
class StateMachine {
public:
    StateMachine() : currentState(STAGE0_STARTUP), stateStartTime(0), successCount(0) {}
    
    void init(SensorManager& sensorMgr, LEDController& ledCtrl, HeaterController& heaterCtrl,
              MP3Player& mp3Player, AIInference& aiInference) {
        this->sensorMgr = &sensorMgr;
        this->ledCtrl = &ledCtrl;
        this->heaterCtrl = &heaterCtrl;
        this->mp3Player = &mp3Player;
        this->aiInference = &aiInference;
        
        stateStartTime = millis();
        printStateInfo();
        Serial.println("[STATE] 超精简状态机初始化完成");
    }
    
    void update() {
        static unsigned long lastStatusPrint = 0;
        if (millis() - lastStatusPrint > 15000) {
            printStatusReport();
            lastStatusPrint = millis();
        }
        
        if (checkAIStatus()) return;
        
        switch (currentState) {
            case STAGE0_STARTUP:
                handleStage0();
                break;
            case STAGE1_TIMER1:
                handleStage1();
                break;
            case STAGE2_TIMER2:
                handleStage2();
                break;
            case SYSTEM_SHUTDOWN:
                handleShutdown();
                break;
        }
    }

private:
    SystemState currentState;
    unsigned long stateStartTime;
    int successCount;
    
    SensorManager* sensorMgr;
    LEDController* ledCtrl;
    HeaterController* heaterCtrl;
    MP3Player* mp3Player;
    AIInference* aiInference;
    
    void printStateInfo() {
        const char* stateNames[] = {"STAGE0_STARTUP", "STAGE1_TIMER1", "STAGE2_TIMER2", "SYSTEM_SHUTDOWN"};
        Serial.println("\n" + String('=', 60));
        Serial.printf("🎮 当前阶段: %s\n", stateNames[currentState]);
        Serial.printf("⏱️ 阶段运行时间: %lu 秒\n", (millis() - stateStartTime) / 1000);
        Serial.printf("🎵 成功次数: %d次\n", successCount);
        Serial.println(String('=', 60));
    }
    
    void printStatusReport() {
        unsigned long elapsed = (millis() - stateStartTime) / 1000;
        const char* stateNames[] = {"启动检测", "计时器1", "计时器2", "系统关机"};
        
        Serial.println("\n" + String('-', 50));
        Serial.printf("📊 阶段: %s | 运行: %lus | 音效成功率: %.1f%%\n", 
                     stateNames[currentState], elapsed, mp3Player->getSuccessRate());
        
        Serial.print("🔢 传感器触发: ");
        for (int i = 0; i < Config::NUM_SENSORS; i++) {
            Serial.printf("%d ", sensorMgr->getTriggerCount(i));
        }
        Serial.println();
        
        Serial.printf("🔥 加热器: 1=%s 2=%s 3=%s | 🎵 音效播放中: %s\n",
                      heaterCtrl->isHeaterOn(1) ? "开" : "关",
                      heaterCtrl->isHeaterOn(2) ? "开" : "关",
                      heaterCtrl->isHeaterOn(3) ? "开" : "关",
                      mp3Player->isPlaying() ? "是" : "否");
        
        Serial.println(String('-', 50));
    }
    
    void changeState(SystemState newState) {
        if (currentState != newState) {
            currentState = newState;
            stateStartTime = millis();
            printStateInfo();
        }
    }
    
    bool checkAIStatus() {
        if (!aiInference->isReadyFor30sCheck()) return false;
        
        String aiResult = aiInference->runInference();
        if (aiResult == "rolling") {
            Serial.println("\n🤖 [AI检测] rolling状态 - 系统重置");
            mp3Player->playSystemReset(); // 🎵 使用精简音效
            delay(2000);
            resetToStage0();
            return true;
        } else if (aiResult == "playing" || aiResult == "stop") {
            Serial.printf("🤖 [AI检测] %s状态 - 继续运行\n", aiResult.c_str());
        }
        return false;
    }
    
    void resetToStage0() {
        Serial.println("🔄 [系统重置] 重置到阶段0");
        heaterCtrl->turnOffAll();
        ledCtrl->turnOffAllLeds();
        sensorMgr->resetTriggerCounts();
        successCount = 0;
        changeState(STAGE0_STARTUP);
    }
    
    void handleStage0() {
        static unsigned long phaseStartTime = 0;
        static bool initialized = false;
        
        if (!initialized) {
            Serial.println("\n🚀 [STAGE0] 启动检测开始");
            sensorMgr->resetTriggerCounts();
            successCount = 0;
            phaseStartTime = millis();
            initialized = true;
        }
        
        unsigned long elapsed = millis() - phaseStartTime;
        
        if (elapsed < 4000) {
            static unsigned long lastFlashReport = 0;
            if (millis() - lastFlashReport > 1000) {
                Serial.printf("✨ [STAGE0] 灯带1-5闪烁 %lu/4秒\n", elapsed / 1000 + 1);
                lastFlashReport = millis();
            }
            ledCtrl->irregularFlash(0, 4, 100);
            return;
        }
        
        if (elapsed < 14000) {
            if (elapsed >= 4000 && elapsed < 5000) {
                Serial.println("👀 [STAGE0] 启动10秒检测");
                ledCtrl->turnOffAllLeds();
            }
            
            static unsigned long lastCountdown = 0;
            if (millis() - lastCountdown > 1000) {
                int remaining = 14 - (elapsed / 1000);
                Serial.printf("⏰ [STAGE0] 检测倒计时: %d秒\n", remaining);
                lastCountdown = millis();
            }
            
            sensorMgr->readSensors();
            
            for (int i = 0; i < Config::NUM_SENSORS; i++) {
                if (sensorMgr->isTriggered(i)) {
                    Serial.printf("✅ [STAGE0] 传感器%d触发！\n", i + 1);
                    ledCtrl->turnOffAllLeds();
                    delay(2000);
                    changeState(STAGE1_TIMER1);
                    initialized = false;
                    return;
                }
            }
        } else {
            Serial.println("❌ [STAGE0] 10秒内无触发，关机");
            mp3Player->playShutdown(); // 🎵 使用精简音效
            changeState(SYSTEM_SHUTDOWN);
            initialized = false;
        }
    }
    
    void handleStage1() {
        static int currentLed = -1;
        static unsigned long ledStartTime = 0;
        static bool waitingForSensor = false;
        static bool initialized = false;
        
        if (!initialized) {
            Serial.println("\n🎮 [STAGE1] 计时器1开始");
            initialized = true;
        }
        
        unsigned long elapsed = millis() - stateStartTime;
        
        static unsigned long lastProgressReport = 0;
        if (millis() - lastProgressReport > 15000) {
            Serial.printf("📈 [STAGE1] 进度: %lu/60s | 成功: %d/4次\n",
                          elapsed / 1000, successCount);
            lastProgressReport = millis();
        }
        
        // 加热模块1控制
        if (elapsed < 20000) {
            static bool heaterStarted = false;
            if (!heaterStarted) {
                Serial.println("🔥 [STAGE1] 启动加热模块1");
                heaterCtrl->safeHeaterOn(1);
                heaterStarted = true;
            }
        } else {
            static bool heaterStopped = false;
            if (!heaterStopped) {
                Serial.println("🔥 [STAGE1] 关闭加热模块1");
                heaterCtrl->safeHeaterOff(1);
                heaterStopped = true;
            }
        }
        
        // 主游戏循环
        if (!waitingForSensor) {
            currentLed = random(0, 4);
            Serial.printf("💡 [STAGE1] 选择灯带%d\n", currentLed + 1);
            ledCtrl->setLed(currentLed);
            ledStartTime = millis();
            waitingForSensor = true;
        }
        
        if (waitingForSensor && (millis() - ledStartTime >= 3000)) {
            sensorMgr->readSensors();
            bool triggered = sensorMgr->isTriggered(currentLed);
            
            if (triggered) {
                // 🎵 播放阶段1成功音效
                if (mp3Player->playStage1Success()) {
                    successCount++;
                    sensorMgr->incrementTriggerCount(currentLed);
                    Serial.printf("✅ [STAGE1] 成功！次数: %d\n", successCount);
                }
                ledCtrl->turnOffAllLeds();
            } else {
                // 🎵 播放统一失败音效
                mp3Player->playMiss();
                ledCtrl->flashLed(currentLed, 3);
                Serial.printf("❌ [STAGE1] 传感器%d未触发\n", currentLed + 1);
            }
            
            waitingForSensor = false;
            delay(500);
        }
        
        // 阶段结束判断
        if (elapsed >= 60000) {
            if (successCount < 4) {
                Serial.printf("🔄 [STAGE1] 成功次数不足(%d < 4)，重复\n", successCount);
                stateStartTime = millis();
                waitingForSensor = false;
                initialized = false;
            } else {
                Serial.printf("🎉 [STAGE1] 完成！成功次数: %d\n", successCount);
                ledCtrl->irregularFlash(0, 7, 5000);
                mp3Player->playStageComplete(); // 🎵 使用精简音效
                delay(1000);
                changeState(STAGE2_TIMER2);
                waitingForSensor = false;
                initialized = false;
            }
        }
    }
    
    void handleStage2() {
        static int currentLed = -1;
        static unsigned long ledStartTime = 0;
        static bool waitingForSensor = false;
        static bool initialized = false;
        
        if (!initialized) {
            Serial.println("\n🎲 [STAGE2] 计时器2开始");
            initialized = true;
        }
        
        unsigned long elapsed = millis() - stateStartTime;
        
        static unsigned long lastProgressReport = 0;
        if (millis() - lastProgressReport > 20000) {
            Serial.printf("📈 [STAGE2] 进度: %lu/60s\n", elapsed / 1000);
            lastProgressReport = millis();
        }
        
        // 主游戏循环
        if (!waitingForSensor) {
            currentLed = random(0, Config::NUM_LEDS);
            Serial.printf("💡 [STAGE2] 选择灯带%d\n", currentLed + 1);
            ledCtrl->setLed(currentLed);
            ledStartTime = millis();
            waitingForSensor = true;
        }
        
        if (waitingForSensor && (millis() - ledStartTime >= 2000)) {
            sensorMgr->readSensors();
            bool triggered = sensorMgr->isTriggered(currentLed, true);
            
            if (!triggered) {
                // 🎵 播放统一失败音效
                mp3Player->playMiss();
                ledCtrl->flashLed(currentLed, 3);
                Serial.printf("❌ [STAGE2] 灯带%d未触发\n", currentLed + 1);
            } else {
                ledCtrl->turnOffAllLeds();
                sensorMgr->incrementTriggerCount(currentLed);
                int triggerCount = sensorMgr->getTriggerCount(currentLed);
                
                // 🎵 根据触发次数播放对应音效
                if (triggerCount >= 3) {
                    mp3Player->playStage2Excellent();
                    Serial.printf("✅ [STAGE2] 灯带%d优秀表现！累计%d次\n", 
                                 currentLed + 1, triggerCount);
                } else {
                    mp3Player->playStage1Success(); // 使用合并的普通成功音效
                    Serial.printf("✅ [STAGE2] 灯带%d触发！累计%d次\n", 
                                 currentLed + 1, triggerCount);
                }
            }
            
            waitingForSensor = false;
            delay(500);
        }
        
        // 60秒结束判断
        if (elapsed >= 60000) {
            bool allTriggeredOver10 = true;
            for (int i = 0; i < Config::NUM_SENSORS; i++) {
                if (sensorMgr->getTriggerCount(i) <= 10) {
                    allTriggeredOver10 = false;
                    break;
                }
            }
            
            if (allTriggeredOver10) {
                Serial.println("🎉 [STAGE2] 所有传感器>10次，重置系统");
                mp3Player->playStageComplete(); // 🎵 使用精简音效
                heaterCtrl->safeHeaterOn(3);
                delay(20000);
                heaterCtrl->safeHeaterOff(3);
                resetToStage0();
                initialized = false;
            } else {
                Serial.println("❌ [STAGE2] 条件不满足，返回阶段1");
                mp3Player->playSystemReset(); // 🎵 使用精简音效
                heaterCtrl->safeHeaterOn(2);
                delay(20000);
                heaterCtrl->safeHeaterOff(2);
                changeState(STAGE1_TIMER1);
                initialized = false;
            }
            waitingForSensor = false;
        }
    }
    
    void handleShutdown() {
        Serial.println("💀 [SYSTEM] 系统关机中...");
        mp3Player->playShutdown(); // 🎵 使用精简音效
        delay(3000);
        ledCtrl->turnOffAllLeds();
        heaterCtrl->turnOffAll();
        Serial.println("💀 [SYSTEM] 所有硬件已关闭");
        
        while(1) {
            delay(5000);
            heaterCtrl->checkSafety();
            Serial.println("💀 [SYSTEM] 系统已关机...");
        }
    }
};

// ================= 全局实例 =================
SensorManager sensorManager;
LEDController ledController;
HeaterController heaterController;
MP3Player mp3Player;
AIInference aiInference;
StateMachine stateMachine;

// ================= 主程序 =================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n" + String('=', 70));
    Serial.println("🎮 ESP32游戏系统启动 (4音效超精简版)");
    Serial.println("🎵 特色: 4音效超精简设计，极致简化管理");
    Serial.println("🔧 硬件: 8传感器 + 8LED + 3加热器 + 超精简DFPlayer + MPU6050");
    Serial.println("🔧 优化: 超精简音效库 + 万能系统音效 + 合并成功音效");
    Serial.println(String('=', 70));
    
    // 初始化所有模块
    sensorManager.init();
    ledController.init();
    heaterController.init();
    
    // 🎵 初始化MP3播放器（超精简声音系统）
    mp3Player.init();
    
    // 🎵 播放开机欢迎音效
    Serial.println("🎵 播放开机欢迎音效");
    if (mp3Player.playStartup()) {
        Serial.println("✅ 开机音效播放成功");
        delay(3000);
    } else {
        Serial.println("⚠️ 开机音效播放失败");
        delay(1000);
    }
    
    // 显示音效系统状态
    mp3Player.printStatus();
    
    // 初始化AI推理模块
    aiInference.init();
    
    // 初始化状态机
    stateMachine.init(sensorManager, ledController, heaterController,
                      mp3Player, aiInference);
    
    Serial.println("✅ [SYSTEM] 所有模块初始化完成");
    Serial.println("🚀 [SYSTEM] 超精简游戏系统启动完成");
}

void loop() {
    // AI数据收集
    aiInference.collectMPUData();
    
    // 读取传感器数据
    sensorManager.readSensors();
    
    // 检查加热器安全
    heaterController.checkSafety();
    
    // 🎵 MP3状态检查（包含超精简声音系统更新）
    mp3Player.checkStatus();
    
    // 更新状态机
    stateMachine.update();
    
    delay(10);
}

/*
================= 超精简优化说明 =================

🎵 音效超精简 (10→4个)：

【保留音效】
✅ stage_hit (001.mp3) - 阶段1成功 + 阶段2普通成功 (合并)
✅ stage2_excellent (002.mp3) - 阶段2优秀表现 (≥3次)
✅ miss (003.mp3) - 统一失败反馈 (合并所有失败)
✅ system_sound (004.mp3) - 开机关机 + 阶段完成 + 系统重置 (全合并)

【移除音效】
❌ stage1_hit - 合并到stage_hit
❌ stage2_hit - 合并到stage_hit  
❌ startup - 合并到system_sound
❌ shutdown - 合并到system_sound
❌ stage_complete - 合并到system_sound
❌ stage_return - 合并到system_sound

🔧 超精简设计：
✅ 只需4个MP3文件，极简管理
✅ 阶段1和阶段2普通成功统一音效
✅ 保留阶段2优秀表现的特殊奖励
✅ 万能系统音效，适用所有系统事件
✅ 统一失败反馈，简化用户认知

🎮 游戏逻辑：
✅ 阶段1成功: stage_hit
✅ 阶段2成功: <3次→stage_hit, ≥3次→stage2_excellent
✅ 任何失败: miss
✅ 任何系统事件: system_sound
✅ 所有其他功能完全保持不变

这个超精简版本实现了最少音效文件的同时保持核心体验！
==============================================
*/