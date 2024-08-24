
// 参考
// https://github.com/espressif/arduino-esp32/blob/master/libraries/BLE/examples/BLE_server/BLE_server.ino

#include <M5Stack.h>
#include <BLEDevice.h>
#include <BLE2902.h>
#include "float16.h"

#ifdef __cplusplus
extern "C"
{
  #include "IMU_6886.h"
}
#endif


// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "a01d9034-21c3-4618-b9ee-d6d785b218c9"
#define CHARACTERISTIC_UUID "f98bb903-5c5a-4f46-a0f2-dbbcf658b445"

#define DEFAULT_HIT_THRESHOLD 50

// IMU6886のインスタンスを作成
IMU_6886 imu6886;

// 加速度
float accX = 0;
float accY = 0;
float accZ = 0;
// ジャイロ
float gyroX = 0;
float gyroY = 0;
float gyroZ = 0;

// 当たり判定閾値
uint16_t hit_threshold = DEFAULT_HIT_THRESHOLD;

// Bluetooth送信クラス
BLECharacteristic *pCharacteristic = NULL;
BLEServer *pServer = NULL;

// Bluetooth接続状態
static bool deviceConnected = false;
static bool oldDeviceConnected = false;


// Bluetooth LE Change Connect State
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      M5.Lcd.setCursor(10, 210);
      M5.Lcd.println("Bluetooth: connect    ");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      M5.Lcd.setCursor(10, 210);
      M5.Lcd.println("Bluetooth: disconnect");
      
    }
};


// Bluetooth LE initialize
void initBLE() {
  // Create the BLE Device
  BLEDevice::init("Fishing Buddy BLE Server");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_NOTIFY
                        );
  
  pCharacteristic->addDescriptor(new BLE2902());  // Descriptorを定義しておかないとClient側でエラーログが出力される

  // BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
  //                      CHARACTERISTIC_UUID_RX,
  //                     BLECharacteristic::PROPERTY_WRITE
  //                   );

  // pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true); // trueにしないと、Advertising DataにService UUIDが含まれない。
    // minIntervalはデフォルトの20でとくに問題なさそうなため、setMinPreferredは省略
    BLEDevice::startAdvertising();
}


void loopBLE() {
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(1000); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("startAdvertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
}


// 加速度とジャイロのデータを指定したバッファーに書き込む
// - buffer uint8_tのoffset+6Byte分以上のサイズのバッファを指定すること
static void pack(uint8_t* buffer, int offset, float dataX, float dataY, float dataZ) {
    // 1~2バイトにXの情報を格納
  uint16_t dataX100 = float32ToFloat16(dataX);
  buffer[offset + 0] = dataX100 & 0x00ff;
  buffer[offset + 1] = (dataX100 & 0xff00) >> 8;

  // 3~4バイトにYの情報を格納
  uint16_t dataY100 = float32ToFloat16(dataY);
  buffer[offset + 2] = dataY100 & 0x00ff;
  buffer[offset + 3] = (dataY100 & 0xff00) >> 8;

  // 5~6バイトにZの情報を格納
  uint16_t dataZ100 = float32ToFloat16(dataZ);
  buffer[offset + 4] = dataZ100 & 0x00ff;
  buffer[offset + 5] = (dataZ100 & 0xff00) >> 8;

}

// 合成ベクトルの大きさを計算
float calcVectorNorm(float X, float Y, float Z) {
  return sqrt(X*X + Y*Y + Z*Z);
}

// 当たり判定
bool judgeHit(float X, float Y, float Z) {
  float vec_norm = calcVectorNorm(X, Y, Z);
  return vec_norm >= hit_threshold;
}

// 当たり判定の閾値を変更する
void changeHitThreshold() {
  // 左側のボタンを押すと、閾値を下げる
  if (M5.BtnA.isPressed() == 1) {
    hit_threshold -= 5;
  }
  // 真ん中のボタンを押すと、閾値をデフォルトに戻す
  if (M5.BtnB.isPressed() == 1) {
    hit_threshold = DEFAULT_HIT_THRESHOLD;
  }
  // 右側のボタンを押すと、閾値を上げる
  if (M5.BtnC.isPressed() == 1) {
    hit_threshold += 5;
  }
}

void setup()
{
  // put your setup code here, to run once:
  M5.begin();
  M5.Power.begin();
  Serial.begin(115200);

  // Bluetooth初期化、通信開始
  initBLE();

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 0);
  M5.Lcd.println("Fishing Buddy BLE Server");
  M5.Lcd.setCursor(10, 25);
  M5.Lcd.println("  X      Y       Z");
  M5.Lcd.setCursor(10, 210);
  M5.Lcd.println("Bluetooth: disconnect");

  imu6886.Init(21, 22);
}

void loop()
{
  // put your main code here, to run repeatedly:
  loopBLE();

  // ボタンが押されたときに当たり判定閾値を変更
  changeHitThreshold();

  imu6886.getGyroData(&gyroX, &gyroY, &gyroZ);
  imu6886.getAccelData(&accX, &accY, &accZ);

  // ジャイロXYZ軸の合成ベクトルのノルムを計算し、当たりを判定
  float vec_norm = calcVectorNorm(gyroX, gyroY, gyroZ);
  bool judge_hit = judgeHit(gyroX, gyroY, gyroZ);

  M5.Lcd.setCursor(10, 50);
  M5.Lcd.printf("%.2f   %.2f   %.2f   ", gyroX, gyroY, gyroZ);
  M5.Lcd.setCursor(270, 50);
  M5.Lcd.print("o/s");
  M5.Lcd.setCursor(10, 100);
  M5.Lcd.printf("%.2f   %.2f   %.2f   ", accX, accY, accZ);
  M5.Lcd.setCursor(270, 100);
  M5.Lcd.print("G");
  

  M5.Lcd.setCursor(10, 150);
  M5.Lcd.printf("gyro_vec_norm: %.2f", vec_norm);
  M5.Lcd.setCursor(10, 170);
  M5.Lcd.printf("thresh: %d, hit: %s", hit_threshold, (judge_hit ? "true " : "false"));

  // BLE送信
  // BLEでのデータ通知用バッファを定義
  uint8_t dataBuffer[13];
  // ジャイロセンサーと加速度センサーのデータをdataBufferに格納
  // BLEでは最大20Byteまでしかデータを送信できないため、float32->float16に変換して送信
  pack(dataBuffer, 0, gyroX, gyroY, gyroZ);
  pack(dataBuffer, 6, accX, accY, accZ);
  // 当たり判定結果を格納
  dataBuffer[12] = (uint8_t)judge_hit;
  pCharacteristic->setValue(dataBuffer, 13);
  pCharacteristic->notify(); // 通知を送信

  M5.update();
  delay(100);
}
