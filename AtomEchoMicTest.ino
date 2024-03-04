// ボタンを押すと3秒録音し、直ぐに再生する

#include <driver/i2s.h>
#include <M5Atom.h>

#define CONFIG_I2S_BCK_PIN      19
#define CONFIG_I2S_LRCK_PIN     33
#define CONFIG_I2S_DATA_PIN     22
#define CONFIG_I2S_DATA_IN_PIN  23

#define SPEAKER_I2S_NUMBER      I2S_NUM_0

#define MODE_MIC                0
#define MODE_SPK                1

#define BUFFER_LEN  2000
#define STORAGE_LEN 96000  // 3sec
unsigned char VOICE[STORAGE_LEN];
int REC;

// 録音用タスク
void i2sRecordTask(void* arg)
{
　　// 初期化
　　bool isRecording = true;
　　int recPos = 0;
　　memset(VOICE, 0, sizeof(VOICE));
　　unsigned char vbuff[BUFFER_LEN];
　　
　　Serial.println("REC START !");
　　
　　vTaskDelay(100);
　　
　　// 録音処理
　　while(isRecording){
　　　　size_t transBytes;
　　　　
　　　　// I2Sからデータ取得
　　　　i2s_read(I2S_NUM_0, (char*)vbuff, BUFFER_LEN, &transBytes, (100 / portTICK_RATE_MS));
　　　　
　　　　// int16_t(12bit精度)をuint8_tに変換
　　　　for(int i=0; i<transBytes; i++){
　　　　　　if(recPos < STORAGE_LEN){
　　　　　　　　VOICE[recPos] = vbuff[i];
　　　　　　　　recPos++;
　　　　　　　　if(recPos >= sizeof(VOICE)){
　　　　　　　　　　isRecording = false;
　　　　　　　　　　break;
　　　　　　　　}
　　　　　　}
　　　　}
　　　　vTaskDelay(1 / portTICK_RATE_MS);
　　}
　　
　　Serial.println("REC STOP !");
　　REC = 2;
　　
　　// タスク削除
　　vTaskDelete(NULL);
}

// マイク＆スピーカー切替
void InitI2SSpeakerOrMic(int mode)
{
　　esp_err_t err = ESP_OK;
　　
　　i2s_driver_uninstall(SPEAKER_I2S_NUMBER);
　　i2s_config_t i2s_config = {
　　　　.mode                 = (i2s_mode_t)(I2S_MODE_MASTER),
　　　　.sample_rate          = 16000,
　　　　.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
　　　　.channel_format       = I2S_CHANNEL_FMT_ALL_RIGHT,
　　　　.communication_format = I2S_COMM_FORMAT_I2S,
　　　　.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
　　　　.dma_buf_count        = 6,
　　　　.dma_buf_len          = 60,
　　　　.use_apll             = false,
　　　　.tx_desc_auto_clear   = true,
　　　　.fixed_mclk           = 0
　　};
　　
　　if(mode == MODE_MIC){
　　　　i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
　　　　i2s_config.sample_rate = 8000; // PDM側が2倍速で取り込まれるので減速
　　}
　　else{
　　　　i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
　　　　i2s_config.sample_rate = 16000;
　　}
　　
　　err += i2s_driver_install(SPEAKER_I2S_NUMBER, &i2s_config, 0, NULL);
　　
　　i2s_pin_config_t tx_pin_config = {
　　　　.bck_io_num           = CONFIG_I2S_BCK_PIN,
　　　　.ws_io_num            = CONFIG_I2S_LRCK_PIN,
　　　　.data_out_num         = CONFIG_I2S_DATA_PIN,
　　　　.data_in_num          = CONFIG_I2S_DATA_IN_PIN,
　　};
　　
　　err += i2s_set_pin(SPEAKER_I2S_NUMBER, &tx_pin_config);
　　
　　if(mode != MODE_MIC){
　　　　err += i2s_set_clk(SPEAKER_I2S_NUMBER, 16000, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
　　}
　　
　　i2s_zero_dma_buffer(SPEAKER_I2S_NUMBER);
}

// 音声再生
void play()
{
　　size_t bytes_written;
　　
　　Serial.println("PLAY START !");
　　
　　InitI2SSpeakerOrMic(MODE_SPK);
　　
　　// Write Speaker
　　i2s_write(SPEAKER_I2S_NUMBER, VOICE, STORAGE_LEN, &bytes_written, portMAX_DELAY);
　　i2s_zero_dma_buffer(SPEAKER_I2S_NUMBER);
　　
　　// Set Mic Mode
　　InitI2SSpeakerOrMic(MODE_MIC);
　　
　　Serial.println("PLAY STOP !");
}

// 初期設定
void setup() 
{
　　// put your setup code here, to run once:
　　M5.begin(true, false, true);
　　delay(50);
　　M5.dis.drawpix(0, CRGB(0, 128, 0));
　　
　　REC = 0;
　　
　　InitI2SSpeakerOrMic(MODE_MIC);
　　delay(2000);
　　
　　Serial.println("SETUP COMPLETE !");
}

// 繰り返しメイン処理
void loop() 
{
　　// put your main code here, to run repeatedly:
　　M5.update();
　　
　　// ボタンを押す音が入るのを防ぐために放した時に起動する
　　if(M5.Btn.wasReleased()){
　　　　size_t bytes_written;
　　　　
　　　　Serial.println("BUTTON PUSH !");
　　　　
　　　　M5.dis.drawpix(0, CRGB(255, 0, 0));
　　　　REC = 1;
　　　　
　　　　// 録音開始
　　　　xTaskCreatePinnedToCore(i2sRecordTask, "i2sRecordTask", 4096, NULL, 1, NULL, 1);
　　}
　　
　　if(REC == 2){
　　　　M5.dis.drawpix(0, CRGB(0, 0, 128));
　　　　// 再生開始
　　　　play();
　　　　REC = 0;
　　　　M5.dis.drawpix(0, CRGB(0, 128, 0));
　　}
　　
　　delay(100);
}
