#include "esp_check.h"
#include "esp_log.h"
#include "esp_brookesia.hpp"
/* These are built-in app examples in `esp-brookesia` library */
#include "app_examples/phone/simple_conf/src/phone_app_simple_conf.hpp"
#include "app_examples/phone/complex_conf/src/phone_app_complex_conf.hpp"
#include "app_examples/phone/squareline/src/phone_app_squareline.hpp"
#include "apps.h"

#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "zx4d30ce40s_lcd_rgb.h"

#define BSP_LCD_H_RES (800)
#define BSP_LCD_V_RES (480)

static const char *TAG = "brookesia";

extern "C"
{
#include "brookesia.h"
}

static void on_clock_update_timer_cb(struct _lv_timer_t *t);

void brookesia_demo_ui(lv_disp_t *dispdd, lv_indev_t *touch)
{
    bsp_display_lock(0);

    /* Create a phone object */
    ESP_Brookesia_Phone *phone = new ESP_Brookesia_Phone(dispdd);
    ESP_BROOKESIA_CHECK_NULL_EXIT(phone, "Create phone failed");

    /* Try using a stylesheet that corresponds to the resolution */
    ESP_Brookesia_PhoneStylesheet_t *stylesheet = nullptr;
    if ((BSP_LCD_H_RES == 1024) && (BSP_LCD_V_RES == 600))
    {
        // stylesheet = new ESP_Brookesia_PhoneStylesheet_t ESP_BROOKESIA_PHONE_1024_600_DARK_STYLESHEET();
        ESP_BROOKESIA_CHECK_NULL_EXIT(stylesheet, "Create stylesheet failed");
    }
    else if ((BSP_LCD_H_RES == 800) && (BSP_LCD_V_RES == 1280))
    {
        // stylesheet = new ESP_Brookesia_PhoneStylesheet_t ESP_BROOKESIA_PHONE_800_1280_DARK_STYLESHEET();
        ESP_BROOKESIA_CHECK_NULL_EXIT(stylesheet, "Create stylesheet failed");
    }
    if (stylesheet != nullptr)
    {
        ESP_LOGI(TAG, "Using stylesheet (%s)", stylesheet->core.name);
        ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->addStylesheet(stylesheet), "Add stylesheet failed");
        ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->activateStylesheet(stylesheet), "Activate stylesheet failed");
        delete stylesheet;
    }

    /* Configure and begin the phone */
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->setTouchDevice(touch), "Set touch device failed");
    phone->registerLvLockCallback((ESP_Brookesia_LvLockCallback_t)(bsp_display_lock), 0);
    phone->registerLvUnlockCallback((ESP_Brookesia_LvUnlockCallback_t)(bsp_display_unlock));
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->begin(), "Begin failed");
    // ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->getCoreHome().showContainerBorder(), "Show container border failed");

    /* Install apps */
    PhoneAppSimpleConf *app_simple_conf = new PhoneAppSimpleConf();
    ESP_BROOKESIA_CHECK_NULL_EXIT(app_simple_conf, "Create app simple conf failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT((phone->installApp(app_simple_conf) >= 0), "Install app simple conf failed");
    PhoneAppComplexConf *app_complex_conf = new PhoneAppComplexConf();
    ESP_BROOKESIA_CHECK_NULL_EXIT(app_complex_conf, "Create app complex conf failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT((phone->installApp(app_complex_conf) >= 0), "Install app complex conf failed");

    PhoneAppPanelLan *app_panellan = new PhoneAppPanelLan();
    ESP_BROOKESIA_CHECK_NULL_EXIT(app_panellan, "Create app panellan failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT((phone->installApp(app_panellan) >= 0), "Install app panellan failed");

    PhoneAppGame2048 *app_game2048 = new PhoneAppGame2048();
    ESP_BROOKESIA_CHECK_NULL_EXIT(app_game2048, "Create app game2048 failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT((phone->installApp(app_game2048) >= 0), "Install app game2048 failed");

    Camera *app_camera = new Camera(800, 640);
    ESP_BROOKESIA_CHECK_NULL_EXIT(app_camera, "Create app camera failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT((phone->installApp(app_camera) >= 0), "Install app camera failed");

    /* Create a timer to update the clock */
    lv_timer_create(on_clock_update_timer_cb, 1000, phone);

    /* Release the lock */
    bsp_display_unlock();

#if EXAMPLE_SHOW_MEM_INFO
    char buffer[128]; /* Make sure buffer is enough for `sprintf` */
    size_t internal_free = 0;
    size_t internal_total = 0;
    size_t external_free = 0;
    size_t external_total = 0;
    while (1)
    {
        internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        internal_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
        external_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        external_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
        sprintf(buffer, "   Biggest /     Free /    Total\n"
                        "\t  SRAM : [%8d / %8d / %8d]\n"
                        "\t PSRAM : [%8d / %8d / %8d]",
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), internal_free, internal_total,
                heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM), external_free, external_total);
        ESP_LOGI("MEM", "%s", buffer);

        /**
         * The `lockLv()` and `unlockLv()` functions are used to lock and unlock the LVGL task.
         * They are registered by the `registerLvLockCallback()` and `registerLvUnlockCallback()` functions.
         */
        phone->lockLv();
        // Update memory label on "Recents Screen"
        if (!phone->getHome().getRecentsScreen()->setMemoryLabel(
                internal_free / 1024, internal_total / 1024, external_free / 1024, external_total / 1024))
        {
            ESP_LOGE(TAG, "Set memory label failed");
        }
        phone->unlockLv();

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
#endif
}

static void on_clock_update_timer_cb(struct _lv_timer_t *t)
{
    time_t now;
    struct tm timeinfo;
    bool is_time_pm = false;
    ESP_Brookesia_Phone *phone = (ESP_Brookesia_Phone *)t->user_data;

    time(&now);
    localtime_r(&now, &timeinfo);
    is_time_pm = (timeinfo.tm_hour >= 12);

    /* Since this callback is called from LVGL task, it is safe to operate LVGL */
    // Update clock on "Status Bar"
    ESP_BROOKESIA_CHECK_FALSE_EXIT(
        phone->getHome().getStatusBar()->setClock(timeinfo.tm_hour, timeinfo.tm_min, is_time_pm),
        "Refresh status bar failed");
}