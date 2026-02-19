#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

/* =============================================
 * TOUCH PINS (VSPI - separate from TFT HSPI)
 * ============================================= */
#define TP_CLK 25
#define TP_CS 33
#define TP_DIN 32
#define TP_OUT 39
#define TP_IRQ 36

/* Change to your screen resolution */
static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

TFT_eSPI tft = TFT_eSPI(); /* TFT instance */
SPIClass touchSPI(VSPI);   /* Separate SPI bus for touch */

/* UI Elements */
lv_obj_t *label_cpu;
lv_obj_t *label_ram;
lv_obj_t *label_temp;
lv_obj_t *label_ip;
lv_obj_t *bar_cpu;
lv_obj_t *bar_ram;

/* =============================================
 * RAW SPI TOUCH (from firmware_v1)
 * ============================================= */
uint16_t touchReadChannel(uint8_t cmd) {
  touchSPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(TP_CS, LOW);
  touchSPI.transfer(cmd);
  uint8_t hi = touchSPI.transfer(0);
  uint8_t lo = touchSPI.transfer(0);
  digitalWrite(TP_CS, HIGH);
  touchSPI.endTransaction();
  return ((hi << 8) | lo) >> 3;
}

void touchPowerDown() {
  touchSPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(TP_CS, LOW);
  touchSPI.transfer(0x80);
  touchSPI.transfer(0);
  touchSPI.transfer(0);
  digitalWrite(TP_CS, HIGH);
  touchSPI.endTransaction();
}

bool getTouch(int &x, int &y) {
  if (digitalRead(TP_IRQ) != LOW)
    return false;
  uint16_t z1 = touchReadChannel(0xB1);
  uint16_t z2 = touchReadChannel(0xC1);
  int z = z1 + 4095 - z2;
  if (z > 400) {
    uint16_t rawX = touchReadChannel(0xD1);
    uint16_t rawY = touchReadChannel(0x91);
    touchPowerDown();
    x = map(rawY, 200, 3800, 0, 320);
    y = map(rawX, 300, 3700, 0, 240);
    x = constrain(x, 0, 319);
    y = constrain(y, 0, 239);
    return true;
  }
  touchPowerDown();
  return false;
}

/* =============================================
 * LVGL DISPLAY DRIVER
 * ============================================= */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area,
                   lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

/* =============================================
 * LVGL TOUCH INPUT DRIVER
 * ============================================= */
static int last_touch_x = 0, last_touch_y = 0;

void my_touchpad_read(lv_indev_drv_t *indev, lv_indev_data_t *data) {
  int x, y;
  if (getTouch(x, y)) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
    last_touch_x = x;
    last_touch_y = y;
  } else {
    data->state = LV_INDEV_STATE_REL;
    data->point.x = last_touch_x;
    data->point.y = last_touch_y;
  }
}

/* =============================================
 * SEND COMMAND TO BRIDGE
 * ============================================= */
void sendCommand(const char *action) {
  StaticJsonDocument<128> doc;
  doc["action"] = action;
  String json;
  serializeJson(doc, json);
  Serial.println(json);
}

/* =============================================
 * BUTTON EVENT HANDLER (with confirmation)
 * ============================================= */
static void btn_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED)
    return;

  const char *action = (const char *)lv_event_get_user_data(e);
  lv_obj_t *btn = lv_event_get_target(e);

  /* Get button label text for the dialog */
  lv_obj_t *label = lv_obj_get_child(btn, 0);
  const char *btn_text = lv_label_get_text(label);

  /* Create msgbox for confirmation */
  static const char *btns[] = {"YES", "NO", ""};
  char msg[64];
  snprintf(msg, sizeof(msg), "%s", btn_text);

  lv_obj_t *mbox = lv_msgbox_create(NULL, "Confirm?", msg, btns, false);
  lv_obj_center(mbox);
  lv_obj_set_width(mbox, 260);

  /* Store action string in mbox user data */
  lv_obj_set_user_data(mbox, (void *)action);

  /* Add event handler for msgbox buttons */
  lv_obj_add_event_cb(
      mbox,
      [](lv_event_t *ev) {
        lv_obj_t *msgbox = lv_event_get_current_target(ev);
        uint16_t idx = lv_msgbox_get_active_btn(msgbox);
        if (idx == 0) { /* YES */
          const char *act = (const char *)lv_obj_get_user_data(msgbox);
          sendCommand(act);

          /* Show "Sent!" notification */
          lv_obj_t *notif = lv_msgbox_create(NULL, NULL, "Sent!", NULL, true);
          lv_obj_center(notif);

          /* Auto-close after 1.5s */
          lv_timer_create(
              [](lv_timer_t *t) {
                lv_obj_t *n = (lv_obj_t *)t->user_data;
                if (n)
                  lv_msgbox_close(n);
                lv_timer_del(t);
              },
              1500, notif);
        }
        lv_msgbox_close(msgbox);
      },
      LV_EVENT_VALUE_CHANGED, NULL);
}

/* Helper: create a control button */
void create_ctrl_btn(lv_obj_t *parent, const char *label, const char *action,
                     lv_color_t color) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, 140, 36);
  lv_obj_set_style_bg_color(btn, color, 0);
  lv_obj_set_style_radius(btn, 6, 0);

  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, label);
  lv_obj_center(lbl);

  lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void *)action);
}

void build_ui() {
  lv_obj_t *tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 50);

  /* Tab 1: Dashboard */
  lv_obj_t *tab1 = lv_tabview_add_tab(tabview, "Status");

  /* CPU Label & Bar */
  lv_obj_t *l1 = lv_label_create(tab1);
  lv_label_set_text(l1, "CPU Usage");
  lv_obj_align(l1, LV_ALIGN_TOP_LEFT, 10, 10);

  bar_cpu = lv_bar_create(tab1);
  lv_obj_set_size(bar_cpu, 200, 20);
  lv_obj_align(bar_cpu, LV_ALIGN_TOP_LEFT, 10, 35);
  lv_bar_set_range(bar_cpu, 0, 100);

  label_cpu = lv_label_create(tab1);
  lv_label_set_text(label_cpu, "0%");
  lv_obj_align_to(label_cpu, bar_cpu, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

  /* RAM Label & Bar */
  lv_obj_t *l2 = lv_label_create(tab1);
  lv_label_set_text(l2, "RAM Usage");
  lv_obj_align(l2, LV_ALIGN_TOP_LEFT, 10, 70);

  bar_ram = lv_bar_create(tab1);
  lv_obj_set_size(bar_ram, 200, 20);
  lv_obj_align(bar_ram, LV_ALIGN_TOP_LEFT, 10, 95);
  lv_bar_set_range(bar_ram, 0, 100);

  label_ram = lv_label_create(tab1);
  lv_label_set_text(label_ram, "0%");
  lv_obj_align_to(label_ram, bar_ram, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

  /* IP Address */
  label_ip = lv_label_create(tab1);
  lv_label_set_text(label_ip, "IP: Waiting...");
  lv_obj_align(label_ip, LV_ALIGN_BOTTOM_LEFT, 10, -10);

  /* Temp */
  label_temp = lv_label_create(tab1);
  lv_label_set_text(label_temp, "0 C");
  lv_obj_align(label_temp, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

  /* Tab 2: Controls */
  lv_obj_t *tab2 = lv_tabview_add_tab(tabview, "Controls");

  /* Flex wrap layout for buttons */
  lv_obj_set_flex_flow(tab2, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(tab2, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_column(tab2, 8, 0);
  lv_obj_set_style_pad_row(tab2, 6, 0);

  /* Create all 7 buttons matching firmware_v1 */
  create_ctrl_btn(tab2, "Reset Net", "reset_network", lv_color_hex(0xFC6000));
  create_ctrl_btn(tab2, "FW Strict", "fw_strict", lv_color_hex(0xD00000));
  create_ctrl_btn(tab2, "FW Maint", "fw_maint", lv_color_hex(0xC8A000));
  create_ctrl_btn(tab2, "Start SMB", "start_smb", lv_color_hex(0x00804A));
  create_ctrl_btn(tab2, "Stop SMB", "stop_smb", lv_color_hex(0xD00000));
  create_ctrl_btn(tab2, "Reboot", "reboot", lv_color_hex(0xFC6000));
  create_ctrl_btn(tab2, "Shutdown", "shutdown", lv_color_hex(0x800020));
}

void update_stats(String json) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  float cpu = doc["cpu"];
  int ram_pct = doc["ram"]["percent"];
  float temp = doc["temp"];

  /* Update UI */
  lv_bar_set_value(bar_cpu, (int)cpu, LV_ANIM_OFF);
  lv_label_set_text_fmt(label_cpu, "%d%%", (int)cpu);

  lv_bar_set_value(bar_ram, ram_pct, LV_ANIM_OFF);
  lv_label_set_text_fmt(label_ram, "%d%%", ram_pct);

  lv_label_set_text_fmt(label_temp, "%.1f C", temp);

  /* Network IP (Just grabbing wlan0 for demo) */
  const char *ip = doc["net"]["wlan0"];
  if (ip) {
    lv_label_set_text_fmt(label_ip, "IP: %s", ip);
  } else {
    const char *ip_ap = doc["net"]["uap0"]; // Raspberry AP often uap0
    if (ip_ap)
      lv_label_set_text_fmt(label_ip, "AP: %s", ip_ap);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  /* Init Display */
  tft.init();
  tft.setRotation(1); /* Landscape */
  tft.fillScreen(TFT_BLACK);

  /* Init Touch (separate VSPI bus) */
  pinMode(TP_CS, OUTPUT);
  digitalWrite(TP_CS, HIGH);
  pinMode(TP_IRQ, INPUT);
  touchSPI.begin(TP_CLK, TP_OUT, TP_DIN, TP_CS);
  touchPowerDown();

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

  /* Initialize the display driver */
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /* Initialize the touch input driver */
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  build_ui();
}

void loop() {
  lv_timer_handler(); /* let the GUI do its work */

  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    update_stats(data);
  }

  delay(5);
}