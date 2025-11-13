# WONDERORB-project2-by-Caifengyi

## 🌟 Overview
"WONDERORB" supports children with tactile sensitivity by applying neural plasticity and gradual desensitization. It provides multimodal, gamified training at home and during treatment waiting periods. The system tracks responses to different materials, offering parents data for progress monitoring. It integrates perceptual feedback, game-based interaction, and a simple machine learning system for adaptive judgment. Future work will optimize visual and auditory experiences, refine feedback grading, and enhance the parent app to generate personalized suggestions based on data and parental input.

## 🧠 Features
- Arduino-based data acquisition
- AI model for pattern recognition
- Product design model

## 🧩 Structure
WONDERBROB/
│
├── aimodel/                          # AI/ML Model & Training
│   ├── Integrated Arduino library/   # Edge Impulse SDK integration
│   │   ├── examples/                 # Device-specific examples
│   │   │   ├── esp32/
│   │   │   ├── nano_ble33_sense/
│   │   │   ├── portenta_h7/
│   │   │   └── ...
│   │   └── src/                      # Core SDK components
│   │       ├── edge-impulse-sdk/     # Edge Impulse inference engine
│   │       ├── model-parameters/      # Model metadata & variables
│   │       └── tflite-model/          # TensorFlow Lite model files
│   └── mpu_train_rolling.ino         # MPU training sketch
│
├── assets/                           # Project documentation & media
│   ├── makingprocess.mp4
│   ├── usingprocess.mp4
│   ├── projectbrief.pdf
│   └── research/
│       └── Assignment1Nathan.pdf
│
├── hardware/                         # Hardware design files
│   ├── 3dmodel/                      # 3D CAD models
│   │   ├── model.3dm
│   │   └── separate model.3dm
│   └── arduino/                      # Production Arduino code
│       └── main-productdata/
│           └── project4code.ino
│
└── test/                             # Testing & verification
    ├── alldevicetest.ino
    └── exampletest.ino


## 📘 Note
This repository serves as a demonstration of the project's technical research and development process.  
Some implementation details, data, and experimental records have been simplified for public release.

For further information or collaboration, please contact:
📩 nissen2417@gamil.com

