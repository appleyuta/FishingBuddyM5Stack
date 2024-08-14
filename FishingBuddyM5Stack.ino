
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
// 温度
float temp = 0;
// Bluetooth送信クラス
BLECharacteristic *pCharacteristic = NULL;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "a01d9034-21c3-4618-b9ee-d6d785b218c9"
#define CHARACTERISTIC_UUID "f98bb903-5c5a-4f46-a0f2-dbbcf658b445"


void startService(BLEServer *pServer)
{
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_NOTIFY
      // BLECharacteristic::PROPERTY_READ |
      // BLECharacteristic::PROPERTY_WRITE
      );
  pCharacteristic->addDescriptor(new BLE2902()); // Descriptorを定義しておかないとClient側でエラーログが出力される
  // pCharacteristic->setValue("Hello World");

  pService->start();
}

void startAdvertising()
{
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true); // trueにしないと、Advertising DataにService UUIDが含まれない。
  // minIntervalはデフォルトの20でとくに問題なさそうなため、setMinPreferredは省略
  BLEDevice::startAdvertising();
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


void setup()
{
  // put your setup code here, to run once:
  M5.begin();
  Serial.begin(115200);

  // Bluetooth初期化、通信開始
  BLEDevice::init("Fishing Buddy BLE Server");
  BLEServer *pServer = BLEDevice::createServer();
  startService(pServer);
  startAdvertising();

  M5.Power.begin();
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(100, 0);
  M5.Lcd.println("MPU6886 TEST");
  M5.Lcd.setCursor(10, 25);
  M5.Lcd.println("  X      Y       Z");
  M5.Lcd.setCursor(10, 210);
  M5.Lcd.println("Advertising!");

  imu6886.Init(21, 22);
}

void loop()
{
    // put your main code here, to run repeatedly:
  imu6886.getGyroData(&gyroX,&gyroY,&gyroZ);
  imu6886.getAccelData(&accX,&accY,&accZ);
  imu6886.getTempData(&temp);

  M5.Lcd.setCursor(10, 70);
  M5.Lcd.printf("%.2f   %.2f   %.2f   ", gyroX, gyroY,gyroZ);
  M5.Lcd.setCursor(270, 70);
  M5.Lcd.print("o/s");
  M5.Lcd.setCursor(10, 140);
  M5.Lcd.printf("%.2f   %.2f   %.2f   ",accX ,accY , accZ);
  M5.Lcd.setCursor(270, 140);
  M5.Lcd.print("G");
  M5.Lcd.setCursor(10, 210);
  // M5.Lcd.printf("Temperature : %.2f C", temp);

  // BLE送信
  // BLEでのデータ通知用バッファを定義
  uint8_t dataBuffer[12];
  // ジャイロセンサーと加速度センサーのデータをdataBufferに格納
  // BLEでは最大20Byteまでしかデータを送信できないため、float32->float16に変換して送信
  pack(dataBuffer, 0, gyroX, gyroY, gyroZ);
  pack(dataBuffer, 6, accX, accY, accZ);
  pCharacteristic->setValue(dataBuffer, 12);
  pCharacteristic->notify(); // 通知を送信

  delay(500);
}
