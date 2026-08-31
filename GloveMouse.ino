#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BleMouse.h>

// DEBUG CONFIGURATION 
// Set to 1 to enable Serial output, 0 to disable for production (saves MCU cycles)
#define DEBUG_MODE 1

// BLUETOOTH CONFIG 
BleMouse bleMouse("GloveMouse", "ESP32", 100);

//I2C & MPU CONFIG 
TwoWire I2C_MPU = TwoWire(1);   // I2C1 -> MPU

#define MPU_ADDR 0x68           // MPU I2C address
#define LCD_ADDR 0x27           // LCD I2C 16x2 address
#define MPU_PWR_MGMT_1 0x6B     // Power management register
#define MPU_ACCEL_XOUT_H 0x3B   // First data register (accelerometer X high byte)
#define MPU_DATA_SIZE 14        // Number of bytes to read in one I2C cycle

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2); 

//  PINS & SENSITIVITY THRESHOLDS
#define TOUCH_LEFT 12           // Left touch pad
#define TOUCH_RIGHT 13          // Right touch pad
#define GPIO_LEFT 26            // Left click LED indicator
#define GPIO_RIGHT 27           // Right click LED indicator
#define POT1_PIN 34             // X-axis sensitivity potentiometer
#define POT2_PIN 35             // Y-axis sensitivity potentiometer
#define STOP_PIN 16             // Pullup button for pause mode

#define TOUCH_LEFT_THRESH 22    // Calibrated capacitive touch threshold (Left)
#define TOUCH_RIGHT_THRESH 28   // Calibrated capacitive touch threshold (Right)

// PROGRAM STATE VARIABLES 
bool programPaused = false;        // Is the mouse in STOP mode?
unsigned long lastStopBtnTime = 0; // Timestamp of the last STOP button press
int lastStopBtnState = HIGH;       // Previous button state

// GYRO PARAMETERS 
const float GYRO_SCALE = 131.0; // MPU scale factor
const float DEAD_GYRO = 2.0;    // Deadzone to prevent cursor drift

// Resting state offsets (inverted sign)
int16_t GZ_OFFSET = 150;
int16_t GY_OFFSET = -100;

// Sensitivity ranges (calibrated for optimal user experience)
const float MIN_SENS_X = -0.1;
const float MAX_SENS_X = -1.0;  // 0.5 mid
const float MIN_SENS_Y = -0.01; 
const float MAX_SENS_Y = -0.49; // 0.25 mid

// POTENTIOMETER FILTERING 
// Exponential moving average filter to smooth out ADC readings and prevent jitter
float pot1Filtered = 0;
float pot2Filtered = 0;
const float ALPHA = 0.02;  // Smoothing factor (lower = smoother but slightly delayed response)

// BUTTON DEBOUNCING 
const unsigned long CLICK_DEBOUNCE = 400; // Prevent accidental double clicks
unsigned long lastLeftTime = 0;
unsigned long lastRightTime = 0;
bool isLeftPressed = false;  
bool isRightPressed = false;

// LCD TIMER 
unsigned long lastLcdUpdate = 0;
const unsigned long LCD_INTERVAL = 100; // Refresh LCD at 10Hz to save energy

//  UTILITY FUNCTIONS
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void setup() {
#if DEBUG_MODE
    Serial.begin(115200);
#endif

    // Set 12-bit ADC resolution for better potentiometer precision (0-4095 range)
    analogReadResolution(12); 

    // I2C INITIALIZATION 
    Wire.begin(19, 18);      // I2C0 -> LCD
    I2C_MPU.begin(21, 22);   // I2C1 -> MPU

    // LCD INITIALIZATION 
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.print("START BLE MOUSE");

    //  PIN INITIALIZATION 
    pinMode(GPIO_LEFT, OUTPUT);
    pinMode(GPIO_RIGHT, OUTPUT);
    pinMode(STOP_PIN, INPUT_PULLUP);

    bleMouse.begin();

    //  WAKE UP MPU 
    I2C_MPU.beginTransmission(MPU_ADDR);
    I2C_MPU.write(MPU_PWR_MGMT_1); 
    I2C_MPU.write(0x00);  // Clear sleep mode bit
    I2C_MPU.endTransmission();

    delay(1000);
    lcd.clear();
}

void loop() {
    unsigned long currentMillis = millis();

    // 0. STOP BUTTON HANDLING (PIN 16) 
    int stopReading = digitalRead(STOP_PIN);

    // Detect state change with 200ms debounce
    if (stopReading == LOW && lastStopBtnState == HIGH && (currentMillis - lastStopBtnTime > 200)) {
        programPaused = !programPaused; 
        lastStopBtnTime = currentMillis;
        lcd.clear(); 
        
        // Force release all clicks when paused
        if (programPaused && bleMouse.isConnected()) {
            bleMouse.release(MOUSE_LEFT);
            bleMouse.release(MOUSE_RIGHT);
            digitalWrite(GPIO_LEFT, LOW);
            digitalWrite(GPIO_RIGHT, LOW);
            isLeftPressed = false;
            isRightPressed = false;
        }
    }
    lastStopBtnState = stopReading;

    // 1. POTENTIOMETER READING & FILTERING 
    int rawPot1 = analogRead(POT1_PIN);
    int rawPot2 = analogRead(POT2_PIN);

    // Exponential filter prioritizes previous state and applies a small fraction (ALPHA) of the new reading
    pot1Filtered = ALPHA * rawPot1 + (1.0 - ALPHA) * pot1Filtered;
    pot2Filtered = ALPHA * rawPot2 + (1.0 - ALPHA) * pot2Filtered;

    // Map 12-bit ADC values (0-4095) to percentage (0-100%)
    int sensX_Percent = constrain((int)((pot1Filtered * 100.0) / 4095.0), 0, 100);
    int sensY_Percent = constrain((int)((pot2Filtered * 100.0) / 4095.0), 0, 100);

    // 2. LCD HANDLING 
    if (currentMillis - lastLcdUpdate > LCD_INTERVAL) {
        lastLcdUpdate = currentMillis;
        lcd.setCursor(0,0);
        
        if (bleMouse.isConnected()) {
            if (programPaused) {
                lcd.print("     STOP       ");
                lcd.setCursor(0,1);
                lcd.print("  WORK PAUSED   ");
            } else {
                lcd.print("Sens X:  "); lcd.print(sensX_Percent); lcd.print("%      ");
                lcd.setCursor(0,1);
                lcd.print("Sens Y:  "); lcd.print(sensY_Percent); lcd.print("%      ");
            }
        } else {
            lcd.print("DISCONNECTED    ");
            lcd.setCursor(0,1);
            lcd.print("Pair Bluetooth  ");
        }
    }

    // Skip sensor processing if disconnected or paused
    if (!bleMouse.isConnected() || programPaused) {
        delay(10);
        return; 
    }

    // 3. MPU READING (ACCELEROMETER & GYRO) 
    int16_t ax = 0, ay = 0, az = 0;
    int16_t temp_raw = 0;
    int16_t gx_raw = 0, gy_raw = 0, gz_raw = 0;

    I2C_MPU.beginTransmission(MPU_ADDR);
    I2C_MPU.write(MPU_ACCEL_XOUT_H); 
    I2C_MPU.endTransmission(false);
    I2C_MPU.requestFrom(MPU_ADDR, MPU_DATA_SIZE);

    // I2C accepts 8-bit frames, but MPU sends 16-bit values.
    // We shift the high byte by 8 bits and bitwise OR it with the low byte to reconstruct the data.
    if (I2C_MPU.available() == MPU_DATA_SIZE) { 
        // Accelerometer data
        ax = (I2C_MPU.read() << 8) | I2C_MPU.read();
        ay = (I2C_MPU.read() << 8) | I2C_MPU.read();
        az = (I2C_MPU.read() << 8) | I2C_MPU.read();
        
        // Temperature data
        temp_raw = (I2C_MPU.read() << 8) | I2C_MPU.read();
        
        // Gyroscope data
        gx_raw = (I2C_MPU.read() << 8) | I2C_MPU.read();
        gy_raw = (I2C_MPU.read() << 8) | I2C_MPU.read();
        gz_raw = (I2C_MPU.read() << 8) | I2C_MPU.read();
    }

#if DEBUG_MODE
    Serial.print("ACC [X,Y,Z]: "); 
    Serial.print(ax); Serial.print("\t");
    Serial.print(ay); Serial.print("\t");
    Serial.print(az); 
    
    Serial.print("\t | TEMP: "); 
    Serial.print(temp_raw / 340.0 + 36.53); 
    
    Serial.print("\t | GYRO [X,Y,Z]: "); 
    Serial.print(gx_raw); Serial.print("\t");
    Serial.print(gy_raw); Serial.print("\t");
    Serial.println(gz_raw);
#endif

    //  OFFSETS CORRECTION & SCALING 
    float gz_corrected = gz_raw + GZ_OFFSET;
    float gy_corrected = gy_raw + GY_OFFSET;

    float gz = gz_corrected / GYRO_SCALE; 
    float gy = gy_corrected / GYRO_SCALE;

    // Sensitivity mapping
    float currentSensX = mapFloat((float)sensX_Percent, 0.0, 100.0, MIN_SENS_X, MAX_SENS_X); 
    float currentSensY = mapFloat((float)sensY_Percent, 0.0, 100.0, MIN_SENS_Y, MAX_SENS_Y);

    // Apply deadzone and calculate final deltas
    int dX = 0, dY = 0;
    if (abs(gz) > DEAD_GYRO) dX = (int)(gz * currentSensX);
    if (abs(gy) > DEAD_GYRO) dY = (int)(-gy * currentSensY);

    // Send cursor movement if delta is non-zero
    if (dX != 0 || dY != 0) bleMouse.move(dX, dY);

    // 4. TOUCH BUTTONS HANDLING
    bool leftTouched = touchRead(TOUCH_LEFT) < TOUCH_LEFT_THRESH; 
    bool rightTouched = touchRead(TOUCH_RIGHT) < TOUCH_RIGHT_THRESH;

    // LEFT CLICK
    if (leftTouched) {
        if (!isLeftPressed && (currentMillis - lastLeftTime > CLICK_DEBOUNCE)) {
            bleMouse.press(MOUSE_LEFT);
            isLeftPressed = true;
            lastLeftTime = currentMillis;
            digitalWrite(GPIO_LEFT, HIGH);
        }
    } else {
        if (isLeftPressed) {
            bleMouse.release(MOUSE_LEFT);
            isLeftPressed = false;
            digitalWrite(GPIO_LEFT, LOW);
        }
    }

    // RIGHT CLICK
    if (rightTouched) {
        if (!isRightPressed && (currentMillis - lastRightTime > CLICK_DEBOUNCE)) {
            bleMouse.press(MOUSE_RIGHT);
            isRightPressed = true;
            lastRightTime = currentMillis;
            digitalWrite(GPIO_RIGHT, HIGH);
        }
    } else {
        if (isRightPressed) {
            bleMouse.release(MOUSE_RIGHT);
            isRightPressed = false;
            digitalWrite(GPIO_RIGHT, LOW);
        }
    }

    delay(10); // Short loop delay
}