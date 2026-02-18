#include <Arduino.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

/* Change to your screen resolution */
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * 10 ];

TFT_eSPI tft = TFT_eSPI(); /* TFT instance */

/* UI Elements */
lv_obj_t * label_cpu;
lv_obj_t * label_ram;
lv_obj_t * label_temp;
lv_obj_t * label_ip;
lv_obj_t * bar_cpu;
lv_obj_t * bar_ram;

/* Display flushing */
void my_disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p )
{
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );

    tft.startWrite();
    tft.setAddrWindow( area->x1, area->y1, w, h );
    tft.pushColors( ( uint16_t * )&color_p->full, w * h, true );
    tft.endWrite();

    lv_disp_flush_ready( disp );
}

void build_ui() {
    lv_obj_t * tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 50);

    /* Tab 1: Dashboard */
    lv_obj_t * tab1 = lv_tabview_add_tab(tabview, "Status");

    /* CPU Label & Bar */
    lv_obj_t * l1 = lv_label_create(tab1);
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
    lv_obj_t * l2 = lv_label_create(tab1);
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
    lv_obj_t * tab2 = lv_tabview_add_tab(tabview, "Controls");
    
    lv_obj_t * btn = lv_btn_create(tab2);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t * btn_l = lv_label_create(btn);
    lv_label_set_text(btn_l, "Reboot Pi");
    // TODO: Add event handler for button
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
    const char* ip = doc["net"]["wlan0"];
    if (ip) {
        lv_label_set_text_fmt(label_ip, "IP: %s", ip);
    } else {
         const char* ip_ap = doc["net"]["uap0"]; // Raspberry AP often uap0
         if(ip_ap) lv_label_set_text_fmt(label_ip, "AP: %s", ip_ap);
    }
}

void setup() {
    Serial.begin(115200);

    /* Init Display */
    tft.begin();
    tft.setRotation(1); /* Landscape */

    lv_init();
    lv_disp_draw_buf_init( &draw_buf, buf, NULL, screenWidth * 10 );

    /* Initialize the display driver */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init( &disp_drv );
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register( &disp_drv );

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
