// ## Sơ đồ chân
//
// ### Microphone Analog
//
// | MCU     | Mic       |
// | ------- | --------- |
// | GND     | GND       |
// | 3.3V    | VCC / VDD |
// | GPIO 35 | Out       |
//
// ### Microphone Digital (I2S)
//
// | MCU     | Mic         |
// | ------- | ----------- |
// | GND     | GND         |
// | 3.3V    | VCC / VDD   |
// | GPIO 26 | BCLK        |
// | GPIO 33 | Data / DOUT |
// | GPIO 25 | LRCL        |
//
// ### Màn hình I2C (tùy chọn)
//
// | MCU     | Display   |
// | ------- | --------- |
// | GND     | GND       |
// | 3.3V    | VCC / VDD |
// | GPIO 21 | SDA       |
// | GPIO 22 | SCL       |
//

#define MIC_I2S_SPH0645

// Bỏ comment dòng này để bật đầu ra màn hình SSD1306
#define DISPLAY_OUTPUT 1

// Bỏ comment dòng này để bật truyền tải tầm xa
// Các giao thức này chậm hơn và sử dụng nhiều bộ nhớ hơn để giải mã, nhưng mạnh mẽ hơn nhiều
//#define LONG_RANGE 1

#include <ggwave.h>
#include <Arduino.h>
#include <soc/adc_channel.h>
#include <driver/i2s.h>

// Cấu hình chân
const int kPinLED0 = 2;

// Instance GGwave toàn cục
GGWave ggwave;

// Cấu hình thu âm thanh
using TSample      = int16_t;
using TSampleInput = int32_t;


const size_t kSampleSize_bytes = sizeof(TSample);

// Tần số lấy mẫu cao - chất lượng tốt hơn, nhưng sử dụng nhiều CPU/Memory hơn
const int sampleRate = 24000;
const int samplesPerFrame = 512;

// Tần số lấy mẫu thấp
// Chỉ các giao thức MT sẽ hoạt động trong chế độ này
//const int sampleRate = 12000;
//const int samplesPerFrame = 256;

TSample sampleBuffer[samplesPerFrame];
TSampleInput sampleBufferRaw[samplesPerFrame];


const i2s_port_t i2s_port = I2S_NUM_0;


#if defined(MIC_I2S) || defined(MIC_I2S_SPH0645)
// cấu hình i2s để sử dụng đầu vào mic I2S từ kênh RIGHT
const i2s_config_t i2s_config = {
    .mode                     = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate              = sampleRate,
    .bits_per_sample          = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format           = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format     = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags         = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count            = 4,
    .dma_buf_len              = samplesPerFrame,
    .use_apll                 = false,
    .tx_desc_auto_clear       = false,
    .fixed_mclk               = 0
};

// Cấu hình chân theo thiết lập
const i2s_pin_config_t pin_config = {
    .bck_io_num   = 26,   // Serial Clock (SCK)
    .ws_io_num    = 25,    // Word Select (WS)
    .data_out_num = I2S_PIN_NO_CHANGE, // không sử dụng (chỉ dành cho loa)
    .data_in_num  = 33   // Serial Data (SD)
};
#endif

#ifdef DISPLAY_OUTPUT

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Khai báo cho màn hình LCD 1602 kết nối với I2C
// 16 cột, 2 hàng
LiquidCrystal_I2C lcd(0x27, 16, 2);

#endif

void setup() {
    Serial.begin(115200);
    while (!Serial);

    pinMode(kPinLED0, OUTPUT);
    digitalWrite(kPinLED0, LOW);

#ifdef DISPLAY_OUTPUT
    {
        Serial.println(F("Đang khởi tạo màn hình..."));
        // Khởi tạo màn hình LCD
        lcd.init();
        lcd.backlight();
        
        // Hiển thị thông báo khởi tạo
        lcd.setCursor(0, 0);
        lcd.print("VXL ESP32 RX:");
        lcd.setCursor(0, 1);
        lcd.print("Dang lang nghe..");
    }
#endif

    // Khởi tạo "ggwave"
    {
        Serial.println(F("Đang cố gắng khởi tạo instance ggwave"));

        ggwave.setLogFile(nullptr);

        auto p = GGWave::getDefaultParameters();

        // Điều chỉnh các tham số "ggwave" theo nhu cầu của bạn.
        // Đảm bảo rằng tham số "payloadLength" khớp với tham số được sử dụng ở phía truyền.
#ifdef LONG_RANGE
        // Các giao thức "FAST" yêu cầu nhiều bộ nhớ gấp 2 lần, vì vậy chúng ta giảm độ dài payload để bù:
        p.payloadLength   = 8;
#else
        p.payloadLength   = 16;
#endif
        Serial.print(F("Sử dụng độ dài payload: "));
        Serial.println(p.payloadLength);

        p.sampleRateInp   = sampleRate;
        p.sampleRateOut   = sampleRate;
        p.sampleRate      = sampleRate;
        p.samplesPerFrame = samplesPerFrame;
        p.sampleFormatInp = GGWAVE_SAMPLE_FORMAT_I16;
        p.sampleFormatOut = GGWAVE_SAMPLE_FORMAT_U8;
        p.operatingMode   = GGWAVE_OPERATING_MODE_RX | GGWAVE_OPERATING_MODE_TX | GGWAVE_OPERATING_MODE_USE_DSS | GGWAVE_OPERATING_MODE_TX_ONLY_TONES;

        // Các giao thức sử dụng cho TX
        // Loại bỏ những cái bạn không cần để giảm sử dụng bộ nhớ
        GGWave::Protocols::tx().disableAll();
        //GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_MT_NORMAL,  true);
        //GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_MT_FAST,    true);
        GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_MT_FASTEST, true);

        // Các giao thức sử dụng cho RX
        // Loại bỏ những cái bạn không cần để giảm sử dụng bộ nhớ
        GGWave::Protocols::rx().disableAll();
        //GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_DT_NORMAL,  true);
#ifdef LONG_RANGE
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_DT_FAST,    true);
#endif
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_DT_FASTEST, true);
        //GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_MT_NORMAL,  true);
#ifdef LONG_RANGE
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_MT_FAST,    true);
#endif
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_MT_FASTEST, true);

        // In bộ nhớ yêu cầu cho instance "ggwave":
        ggwave.prepare(p, false);

        Serial.print(F("Bộ nhớ yêu cầu bởi instance ggwave: "));
        Serial.print(ggwave.heapSize());
        Serial.println(F(" bytes"));

        // Khởi tạo instance "ggwave":
        ggwave.prepare(p, true);
        Serial.print(F("Instance được khởi tạo thành công! Bộ nhớ đã sử dụng: "));
    }

    // Bắt đầu thu âm thanh
    {
        Serial.println(F("Đang khởi tạo giao diện I2S"));

        // Cài đặt và khởi động driver i2s
        i2s_driver_install(i2s_port, &i2s_config, 0, NULL);

#if defined(MIC_I2S) || defined(MIC_I2S_SPH0645)
        Serial.println(F("Sử dụng đầu vào I2S"));

#if defined(MIC_I2S_SPH0645)
        Serial.println(F("Áp dụng fix cho SPH0645"));

        // https://github.com/atomic14/esp32_audio/blob/d2ac3490c0836cb46a69c83b0570873de18f695e/i2s_sampling/src/I2SMEMSSampler.cpp#L17-L22
        REG_SET_BIT(I2S_TIMING_REG(i2s_port), BIT(9));
        REG_SET_BIT(I2S_CONF_REG(i2s_port), I2S_RX_MSB_SHIFT);
#endif

        i2s_set_pin(i2s_port, &pin_config);
#endif
    }
}

int niter = 0;
int tLastReceive = -10000;

GGWave::TxRxData result;

void loop() {
    // Đọc từ i2s
    {
        size_t bytes_read = 0;
        i2s_read(i2s_port, sampleBufferRaw, sizeof(TSampleInput)*samplesPerFrame, &bytes_read, portMAX_DELAY);

        int nSamples = bytes_read/sizeof(TSampleInput);
        if (nSamples != samplesPerFrame) {
            Serial.println("Thất bại khi đọc samples");
            return;
        }

#if defined(MIC_I2S) || defined(MIC_I2S_SPH0645)
        for (int i = 0; i < nSamples; ++i) {
            sampleBuffer[i] = (sampleBufferRaw[i] & 0xFFFFFFF0) >> 11;
        }
#endif
    }

    // Sử dụng cái này với serial plotter để quan sát tín hiệu âm thanh thời gian thực
    //for (int i = 0; i < nSamples; i++) {
    //    Serial.println(sampleBuffer[i]);
    //}

    // Cố gắng giải mã bất kỳ dữ liệu "ggwave" nào:
    auto tStart = millis();

    if (ggwave.decode(sampleBuffer, samplesPerFrame*kSampleSize_bytes) == false) {
        Serial.println("Thất bại khi giải mã");
    }

    auto tEnd = millis();

    if (++niter % 10 == 0) {
        // in thời gian mà lần gọi decode() cuối cùng hoàn thành
        // nên nhỏ hơn samplesPerFrame/sampleRate giây
        // ví dụ: samplesPerFrame = 128, sampleRate = 6000 => không quá 20 ms
        Serial.println(tEnd - tStart);
        if (tEnd - tStart > 1000*(float(samplesPerFrame)/sampleRate)) {
            Serial.println(F("Cảnh báo: decode() mất quá nhiều thời gian để thực thi!"));
        }
    }

    // Kiểm tra xem chúng ta đã giải mã thành công dữ liệu nào chưa:
    int nr = ggwave.rxTakeData(result);
    if (nr > 0) {
        Serial.println(tEnd - tStart);
        Serial.print(F("Nhận được dữ liệu với độ dài "));
        Serial.print(nr); // nên bằng với p.payloadLength
        Serial.println(F(" bytes:"));

        Serial.println((char *) result.data());
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("Receive: "));
        lcd.setCursor(0, 1);
        lcd.print((char *) result.data());
        tLastReceive = tEnd;
    }
}