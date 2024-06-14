#include <lwip/dns.h>
#include <errno.h>
#include <hardware/adc.h>
#include <hardware/rtc.h>
#include <lwip/apps/mqtt.h>
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include <time.h>

#define WIFI_SSID       "WIFI-SSID"
#define WIFI_PASSWORD   "WIFI-PASSWORD"
#define MQTT_SERVER     "io.adafruit.com"  // This sample does not support TLS, so the port number is 1883
#define MQTT_USERNAME   "MQTT_USERNAME"
#define MQTT_PASSWORD   "MQTT_PASSWORD"

extern bool ntp_sync(void);
extern const char *get_timestamp(void);

static float read_sensor_data() {
    uint16_t raw = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    float voltage = raw * conversion_factor;
    float temperature = 27.0f - (voltage - 0.706f) / 0.001721f;
    return temperature;
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    (void)client;
    (void)arg;
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("MQTT connect OK\n");
    } else {
        printf("MQTT connect failed: status=%d\n", status);
    }
}

static void send_mqtt_message(mqtt_client_t *client, float data, const char* timestamp) {
    (void)client;
    char payload[100] = {0};
    snprintf(payload, sizeof(payload), "{\"timestamp\":\"%s\", \"value\":%.2f}", timestamp, data);
    mqtt_publish(client, MQTT_USERNAME "/feeds/temperature", payload, strlen(payload), 1, 0, NULL, NULL);
    printf("MQTT publish: %s\n", payload);
}

static void save_data_to_file(float data, const char* timestamp) {
    FILE *fp = fopen("/temperature.txt", "a");
    if (fp == NULL) {
        printf("fopen failed: %s\n", strerror(errno));
        return;
    }
    fprintf(fp, "%s,%.2f\n", timestamp, data);
    printf("Queue data: %s,%.2f\n", timestamp, data);
    fclose(fp);
}

static void send_data_from_file(mqtt_client_t *client) {
    FILE *fp = fopen("/temperature.txt", "r");
    if (fp == NULL) {
        return;
    }

    char timestamp[30] = {0};
    float data = 0;
    while (fscanf(fp, "%[^,],%f\n", timestamp, &data) != EOF) {
        send_mqtt_message(client, data, timestamp);
    }
    fclose(fp);
    remove("/temperature.txt");
}

static bool mantain_network_connection(mqtt_client_t *client, ip_addr_t *addr) {
    bool wifi_status = netif_is_up(&cyw43_state.netif[0]) && netif_is_link_up(&cyw43_state.netif[0]);
    if (!wifi_status) {
        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
        int err = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 1000);
        if (err != 0) {
            printf("WiFi connection failed: err=%d\n", err);
            return false;
        }
        printf("Wi-Fi connect ok\n");
    }
    if (!mqtt_client_is_connected(client)) {
        static struct mqtt_connect_client_info_t ci = {0};
        ci.client_id = "";
        ci.client_user = MQTT_USERNAME;
        ci.client_pass = MQTT_PASSWORD;
        ci.keep_alive = 30;
        mqtt_client_connect(client, addr, MQTT_PORT, mqtt_connection_cb, 0, &ci);
        return true;
    }
    return true;
}

int main(void) {
    stdio_init_all();
    rtc_init();
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);

    if (cyw43_arch_init()) {
        printf("WiFi init failed");
        return -1;
    }
    cyw43_arch_enable_sta_mode();
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000) != 0) {
        printf("WiFi connection failed\n");
        sleep_ms(1000);
    }
    printf("WiFi connect ok\n");

    while (!ntp_sync()) {
        printf("NTP sync failed\n");
        sleep_ms(1000);
    }
    printf("NTP sync ok\n");

    mqtt_client_t *client = mqtt_client_new();
    ip_addr_t mqtt_server_ip;
    int err = 0;
    while ((err = dns_gethostbyname(MQTT_SERVER, &mqtt_server_ip, NULL, NULL)) != ERR_OK) {
        printf("dns_gethostbyname(%s) failed=%d\n", MQTT_SERVER, err);
        sleep_ms(1000);
    }

    while (1) {
        float sensor_value = read_sensor_data();
        const char *timestamp = get_timestamp();
        if (mantain_network_connection(client, &mqtt_server_ip)) {
            send_mqtt_message(client, sensor_value, timestamp);
            send_data_from_file(client);
        } else {
            save_data_to_file(sensor_value, timestamp);
        }
        sleep_ms(10 * 1000);
    }

    mqtt_client_free(client);
    cyw43_arch_deinit();
    return 0;
}
