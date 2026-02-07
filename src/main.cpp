#include <Arduino.h>  // Thư viện Arduino cơ bản cho ESP32
#include <ggwave.h>   // Thư viện GGWave để mã hóa âm thanh

// Cấu hình chân GPIO
const int kPinLed0    = 18;  // Chân kết nối LED báo hiệu
const int kPinSpeaker = 19;  // Chân kết nối loa/buzzer để phát âm thanh
const int kPinButton0 = 22;  // Chân kết nối nút nhấn để kích hoạt truyền

// Các tham số âm thanh cho GGWave
const int samplesPerFrame = 128;  // Số mẫu âm thanh mỗi khung hình
const int sampleRate      = 6000; // Tần số lấy mẫu (Hz)

// Đối tượng GGWave toàn cục để xử lý mã hóa âm thanh
GGWave ggwave;

char txt[64];  // Buffer để lưu chuỗi văn bản tạm thời
#define P(str) (strcpy_P(txt, PSTR(str)), txt)  // Macro để sao chép chuỗi từ PROGMEM

// Hàm trợ giúp để phát sóng âm GGWave đã được mã hóa qua loa/buzzer
void send_text(GGWave & ggwave, uint8_t pin, const char * text, GGWave::TxProtocolId protocolId) {
    Serial.print(F("Sending text: "));
    Serial.println(text);

    ggwave.init(text, protocolId);
    ggwave.encode();

    const auto & protocol = GGWave::Protocols::tx()[protocolId];
    const auto tones = ggwave.txTones();
    const auto duration_ms = protocol.txDuration_ms(ggwave.samplesPerFrame(), ggwave.sampleRateOut());
    for (auto & curTone : tones) {
        const auto freq_hz = (protocol.freqStart + curTone)*ggwave.hzPerSample();
        tone(pin, freq_hz);
        delay(duration_ms);
    }

    noTone(pin);
    digitalWrite(pin, LOW);
}

void setup() {
    Serial.begin(115200);  
    while (!Serial);

    pinMode(kPinLed0,    OUTPUT);
    pinMode(kPinSpeaker, OUTPUT);
    pinMode(kPinButton0, INPUT_PULLUP);

    {
        Serial.println(F("Trying to initialize the ggwave instance"));

        ggwave.setLogFile(nullptr);

        auto p = GGWave::getDefaultParameters();

        p.payloadLength   = 16;
        p.sampleRateInp   = sampleRate;
        p.sampleRateOut   = sampleRate;
        p.sampleRate      = sampleRate;
        p.samplesPerFrame = samplesPerFrame;
        p.sampleFormatInp = GGWAVE_SAMPLE_FORMAT_I16;
        p.sampleFormatOut = GGWAVE_SAMPLE_FORMAT_U8;
        p.operatingMode   = GGWAVE_OPERATING_MODE_TX | GGWAVE_OPERATING_MODE_TX_ONLY_TONES | GGWAVE_OPERATING_MODE_USE_DSS;

        GGWave::Protocols::tx().only(GGWAVE_PROTOCOL_MT_FASTEST);

        ggwave.prepare(p);
        Serial.println(ggwave.heapSize());

        Serial.println(F("Instance initialized successfully!"));
    }
}

String receivedText = "";  // Biến toàn cục lưu văn bản nhận từ Serial

void loop() {
    // Đọc dữ liệu từ Serial (nếu có)
    if (Serial.available() > 0) {
        receivedText = Serial.readStringUntil('\n');
        Serial.print("Received: ");
        Serial.println(receivedText);
    }

    // Đọc trạng thái nút bấm
    int but0 = digitalRead(kPinButton0);

    // Xử lý nút đếm nhấn nút kPinButton1 (giữ nguyên)
    static bool isDown = false;  // biến trạng thái nút kPinButton1
    static int pressed = 0;



    // Khi nút kPinButton0 được nhấn -> phát âm thanh văn bản đã nhận
    if (but0 == LOW) {
        if (receivedText.length() > 0) {
            digitalWrite(kPinLed0, HIGH);
            send_text(ggwave, kPinSpeaker, receivedText.c_str(), GGWAVE_PROTOCOL_MT_FASTEST);
            digitalWrite(kPinLed0, LOW);
        } else {
            // Nếu chưa nhận được văn bản, gửi thông báo
            Serial.println(F("No text received yet."));
        }
        // reset counter nút nhấn nếu muốn
        pressed = 0;
        delay(500);  // tránh nhấn liên tục quá nhanh
    }

    delay(10);  // giảm tải CPU, tránh vòng lặp chạy quá nhanh
}
