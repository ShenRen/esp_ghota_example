#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fnmatch.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"

#include "lwip/err.h"
#include <lwip/sys.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_ghota.h>

#include <esp_ota_ops.h>
#include <esp_https_ota.h>

// #define EXAMPLE_GITHUB_TEST 1
#define EXAMPLE_GITEE_TEST  1

#define EXAMPLE_ESP_WIFI_SSID  CONFIG_EXAMPLE_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS  CONFIG_EXAMPLE_ESP_WIFI_PASS

    #define EXAMPLE_GITHUB_HOSTNAME CONFIG_EXAMPLE_GITHUB_HOSTNAME
    #define EXAMPLE_GITHUB_OWNER    CONFIG_EXAMPLE_GITHUB_OWNER
    #define EXAMPLE_GITHUB_REPO     CONFIG_EXAMPLE_GITHUB_REPO
    #define EXAMPLE_GITHUB_USERNAME   CONFIG_EXAMPLE_GITHUB_USERNAME
    #define EXAMPLE_GITHUB_PAT_TOKEN  CONFIG_EXAMPLE_GITHUB_PAT_TOKEN

#define DO_BACKGROUND_UPDATE    CONFIG_EXAMPLE_DO_BACKGROUND_UPDATE
#define DO_FOREGROUND_UPDATE    CONFIG_EXAMPLE_DO_FOREGROUND_UPDATE
#define DO_MANUAL_CHECK_UPDATE  CONFIG_EXAMPLE_DO_MANUAL_CHECK_UPDATE

// #define EXAMPLE_FIRMWARE_FILE_NAME  CONFIG_EXAMPLE_FIRMWARE_FILE_NAME
// #define EXAMPLE_STORAGE_FILE_NAME   CONFIG_EXAMPLE_STORAGE_FILE_NAME
#define EXAMPLE_FIRMWARE_FILE_NAME  (PROJECT_NAME "_firmware_" PROJECT_TARGET "_%s.bin")
#define EXAMPLE_STORAGE_FILE_NAME   (PROJECT_NAME "_storage_" PROJECT_TARGET "_%s.bin")
#define EXAMPLE_DATA_FILE_NAME      (PROJECT_NAME "_merged_" PROJECT_TARGET "_%s.bin")

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

static const char* TAG = "main";

#if EXAMPLE_CUSTOM_TEST
static esp_err_t git_apiurlformat_cb(char* url_buf, size_t url_size, const struct ghota_config_t * ghota_config)
{
    //snprintf(url_buf, url_size, "https://%s/api/v5/repos/%s/%s/releases", ghota_config->hostname, ghota_config->onwername, ghota_config->reponame);
    //return ESP_OK;

    ESP_LOGE(TAG, "Unimplemented function");
    return ESP_FAIL;
}
#endif

esp_err_t get_asset_version_cb(semver_t* version, const ghota_asset_t * asset, const struct ghota_config_t * ghota_config){
    if(asset->type == GHOTA_ASSET_FIRMWARE){
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        const esp_app_desc_t *app_desc = esp_app_get_description();
#else
        const esp_app_desc_t *app_desc = esp_ota_get_app_description();
#endif
        if (semver_parse(app_desc->version, version)){
            ESP_LOGE(TAG, "Failed to parse firmware version");
            return ESP_FAIL;
        }
        return ESP_OK;
    } else if(asset->type == GHOTA_ASSET_STORAGE){
        // TODO: Implement this
        ESP_LOGE(TAG, "Unimplemented case GHOTA_ASSET_FILE");
        return ESP_FAIL;
    } else if(asset->type == GHOTA_ASSET_FILE){
        // TODO: Implement this
        ESP_LOGE(TAG, "Unimplemented case GHOTA_ASSET_FILE");
        return ESP_FAIL;
    }
    return ESP_FAIL;
}


/* The order is the upgrade order */
static ghota_asset_t ghota_assets[] = {
    // Test update file first
    {
        .type = GHOTA_ASSET_FIRMWARE,
        .nameformat = EXAMPLE_FIRMWARE_FILE_NAME,
    },
    {   .type = GHOTA_ASSET_STORAGE,
        .nameformat = EXAMPLE_STORAGE_FILE_NAME,
        .partitionname = "storage",
    },
    {   
        .type = GHOTA_ASSET_FILE,
        .nameformat = EXAMPLE_DATA_FILE_NAME,
        .filedirpath = "/spiffs",
    },
};

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 10) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

static void read_from_spiffs(void)
{
    ESP_LOGI(TAG, "Reading hello.txt");

    // Open for reading hello.txt
    FILE* f = fopen("/spiffs/test.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open test.txt");
        return;
    }

    char buf[64];
    memset(buf, 0, sizeof(buf));
    fread(buf, 1, sizeof(buf), f);
    fclose(f);

    // Display the read contents from the file
    ESP_LOGI(TAG, "Read from test.txt: %s", buf);
}

void mount_spiffs() {
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "storage",
      .max_files = 5,
      .format_if_mount_failed = false
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    read_from_spiffs();
    ESP_LOGI(TAG, "storage spiffs mounted.");
}

void unmount_spiffs() {
    esp_vfs_spiffs_unregister("storage");
        ESP_LOGI(TAG, "storage spiffs unmounted.");
}


static void ghota_event_callback(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
    ghota_client_handle_t *client = (ghota_client_handle_t *)handler_args;
    ESP_LOGI(TAG, "Got Update Callback: %s", ghota_get_event_str(id));
    if (id == GHOTA_EVENT_START_STORAGE_UPDATE) {
        ESP_LOGI(TAG, "Starting storage update");
        /* if we are updating the SPIFF storage we should unmount it */
        unmount_spiffs();
    } else if (id == GHOTA_EVENT_FINISH_STORAGE_UPDATE) {
        ESP_LOGI(TAG, "Ending storage update");
        /* after updating we can remount, but typically the device will reboot shortly after recieving this event. */
        mount_spiffs();
    } else if (id == GHOTA_EVENT_FIRMWARE_UPDATE_PROGRESS) {
        /* display some progress with the firmware update */
        ESP_LOGI(TAG, "Firmware Update Progress: %d%%", *((int*) event_data));
    } else if (id == GHOTA_EVENT_STORAGE_UPDATE_PROGRESS) {
        /* display some progress with the spiffs partition update */
        ESP_LOGI(TAG, "Storage Update Progress: %d%%", *((int*) event_data));
    } else if (id == GHOTA_EVENT_FILE_UPDATE_PROGRESS) {
        /* display some progress with the spiffs partition update */
        ESP_LOGI(TAG, "File Update Progress: %d%%", *((int*) event_data));
    }
    (void)client;
    return;
}

void app_main() {
    ESP_LOGI(TAG, "Starting");

    // 关闭remote_wifi和esp_hosted的日志
    esp_log_level_set("H_SDIO_DRV", ESP_LOG_WARN);     
    esp_log_level_set("sdio_wrapper", ESP_LOG_WARN);     
    esp_log_level_set("transport", ESP_LOG_WARN);     

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    mount_spiffs();

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    /* initialize our ghota config */
    ghota_config_t ghconfig = {
        .assets = ghota_assets,
        .assetssize = sizeof(ghota_assets) / sizeof(ghota_asset_t),
        /* 1 minute as a example, but in production you should pick something larger (remember, Github has ratelimites on the API! )*/
        .updateInterval = 1,
        .onwername = EXAMPLE_GITHUB_OWNER,
        .reponame = EXAMPLE_GITHUB_REPO,
#if EXAMPLE_GITHUB_TEST
        .githost = GHOTA_HOST_GITHUB,
#elif EXAMPLE_GITEE_TEST
        .githost = GHOTA_HOST_GITEE,
#else
        .hostname = EXAMPLE_GITHUB_HOSTNAME,
        .githost = GHOTA_HOST_CUSTOM,
        .apiurlformatcb = git_apiurlformat_cb,
#endif
        //.getversioncb = get_asset_version_cb,

    };

    /* initialize ghota. */
    ghota_client_handle_t *ghota_client = ghota_init(&ghconfig);
    if (ghota_client == NULL) {
        ESP_LOGE(TAG, "ghota_client_init failed");
        return;
    }
    /* register for events relating to the update progress */
    esp_event_handler_register(GHOTA_EVENTS, ESP_EVENT_ANY_ID, &ghota_event_callback, ghota_client);

    /* for private repositories or to get more API calls than anonymouse, set a github username and PAT token
     * see https://docs.github.com/en/github/authenticating-to-github/creating-a-personal-access-token
     * for more information on how to create a PAT token.
     * 
     * Be carefull, as the PAT token will be stored in your firmware etc and can be used to access your github account.
     */
    
#ifdef CONFIG_EXAMPLE_GITHUB_AUTH_TOKEN
    ESP_ERROR_CHECK(ghota_set_auth(ghota_client, 
    EXAMPLE_GITHUB_USERNAME, // "<Insert GH Username>" 
    EXAMPLE_GITHUB_PAT_TOKEN)); // "<Insert PAT TOKEN>"
#endif

#ifdef DO_BACKGROUND_UPDATE
    /* start a timer that will automatically check for updates based on the interval specified above */
    ESP_ERROR_CHECK(ghota_start_update_timer(ghota_client));
#endif

#if DO_FOREGROUND_UPDATE
    /* or do a check/update now
     * This runs in a new task under freeRTOS, so you can do other things while it is running.
     */
    ESP_ERROR_CHECK(ghota_start_update_task(ghota_client));

#elif DO_MANUAL_CHECK_UPDATE
    /* Alternatively you can do manual checks 
     * but note, you probably have to increase the Stack size for the task this runs on
     */

    /* Query the Github Release API for the latest release */
    ESP_ERROR_CHECK(ghota_check(ghota_client));

    /* get the semver version of the currently running firmware */
    semver_t *cur = ghota_get_current_version(ghota_client);
    if (cur) {
         ESP_LOGI(TAG, "Current version: %d.%d.%d", cur->major, cur->minor, cur->patch);
         semver_free(cur);
    

    /* get the version of the latest release on Github */
    semver_t *new = ghota_get_latest_version(ghota_client);
    if (new) {
        ESP_LOGI(TAG, "New version: %d.%d.%d", new->major, new->minor, new->patch);
        semver_free(new);
    }

    /* do some comparisions */
    if (semver_gt(new, cur) == 1) {
        ESP_LOGI(TAG, "New version is greater than current version");
    } else if (semver_eq(new, cur) == 1) {
        ESP_LOGI(TAG, "New version is equal to current version");
    } else {
        ESP_LOGI(TAG, "New version is less than current version");
    }

    /* assuming we have a new version, then do a actual update */
    ESP_ERROR_CHECK(ghota_update(ghota_client));
    /* if there was a new version installed, the esp will reboot after installation and will not reach this code */    
    
#endif

    while (1) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "This is where we do other things. Memory Dump Below to see the memory usage");
        ESP_LOGI(TAG, "Memory: Free %dKiB Low: %dKiB\n", (int)xPortGetFreeHeapSize()/1024, (int)xPortGetMinimumEverFreeHeapSize()/1024);
    }

}